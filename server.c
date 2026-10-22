/*
 * ============================================================================
 *  Face Attendance System — C HTTP Server with Facial Admin Recognition
 * ============================================================================
 *  Compile:  gcc server.c -o server.exe -lws2_32 -lm -O2
 *  Admin Password:  mexxz@2007
 * ============================================================================
 */

#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <direct.h>
#include <time.h>

#ifdef _MSC_VER
#pragma comment(lib, "ws2_32.lib")
#endif

/* ── Configuration ───────────────────────────────────────────────────────── */

#define SERVER_PORT       8080
#define MAX_HEADERS       (8 * 1024)
#define MAX_BODY          (2 * 1024 * 1024)
#define MAX_USERS         500
#define DESC_SIZE         128
#define MATCH_THRESHOLD   0.6f
#define MAX_ATTENDANCE    10000
#define WWW_DIR           "www"
#define DATA_DIR          "data"
#define USERS_FILE        "data\\users.dat"
#define ATTEND_FILE       "data\\attend.dat"

#define ADMIN_PASSWORD    "mexxz@2007"
#define TOKEN_LEN         64

/* ── Data Structures ─────────────────────────────────────────────────────── */

typedef struct {
    char  name[128];
    float descriptor[DESC_SIZE];
    int   is_admin; /* 1 = Admin, 0 = Student */
} User;

typedef struct {
    char name[128];
    char date[16];
} AttendanceRecord;

/* ── Globals ─────────────────────────────────────────────────────────────── */

static User             g_users[MAX_USERS];
static int              g_user_count = 0;
static AttendanceRecord g_attend[MAX_ATTENDANCE];
static int              g_attend_count = 0;
static CRITICAL_SECTION g_cs;

/* Admin session token */
static char             g_admin_token[TOKEN_LEN + 1] = {0};
static int              g_token_valid = 0;

/* ═══════════════════════════════════════════════════════════════════════════
 *  PERSISTENCE
 * ═══════════════════════════════════════════════════════════════════════════ */

static void ensure_dirs(void) { _mkdir(DATA_DIR); _mkdir(WWW_DIR); }

static void load_users(void) {
    FILE *f = fopen(USERS_FILE, "rb");
    if (!f) { g_user_count = 0; return; }

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (fsize < (long)sizeof(int)) { fclose(f); g_user_count = 0; return; }

    fread(&g_user_count, sizeof(int), 1, f);
    if (g_user_count > MAX_USERS) g_user_count = MAX_USERS;

    /* Detect record size for backward compatibility */
    long data_bytes = fsize - (long)sizeof(int);
    int rec_size = (g_user_count > 0) ? (int)(data_bytes / g_user_count) : sizeof(User);

    if (rec_size >= (int)sizeof(User)) {
        fread(g_users, sizeof(User), g_user_count, f);
    } else {
        /* Legacy format (without is_admin field: 128 + 512 = 640 bytes) */
        for (int i = 0; i < g_user_count; i++) {
            fread(g_users[i].name, 128, 1, f);
            fread(g_users[i].descriptor, sizeof(float) * DESC_SIZE, 1, f);
            g_users[i].is_admin = 0;
        }
    }
    fclose(f);
    printf("[INFO] Loaded %d registered user(s)\n", g_user_count);
}

static void save_users(void) {
    FILE *f = fopen(USERS_FILE, "wb");
    if (!f) return;
    fwrite(&g_user_count, sizeof(int), 1, f);
    fwrite(g_users, sizeof(User), g_user_count, f);
    fclose(f);
}

static void load_attendance(void) {
    FILE *f = fopen(ATTEND_FILE, "rb");
    if (!f) { g_attend_count = 0; return; }
    fread(&g_attend_count, sizeof(int), 1, f);
    if (g_attend_count > MAX_ATTENDANCE) g_attend_count = MAX_ATTENDANCE;
    fread(g_attend, sizeof(AttendanceRecord), g_attend_count, f);
    fclose(f);
    printf("[INFO] Loaded %d attendance record(s)\n", g_attend_count);
}

static void save_attendance(void) {
    FILE *f = fopen(ATTEND_FILE, "wb");
    if (!f) return;
    fwrite(&g_attend_count, sizeof(int), 1, f);
    fwrite(g_attend, sizeof(AttendanceRecord), g_attend_count, f);
    fclose(f);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  UTILITIES
 * ═══════════════════════════════════════════════════════════════════════════ */

static void get_today(char *buf, int len) {
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    strftime(buf, len, "%Y-%m-%d", tm);
}

static float euclidean_dist(const float *a, const float *b, int n) {
    float sum = 0.0f;
    for (int i = 0; i < n; i++) { float d = a[i] - b[i]; sum += d * d; }
    return sqrtf(sum);
}

static void generate_token(void) {
    srand((unsigned)(time(NULL) ^ GetCurrentProcessId() ^ GetTickCount()));
    for (int i = 0; i < TOKEN_LEN; i += 2) {
        sprintf(g_admin_token + i, "%02x", rand() % 256);
    }
    g_admin_token[TOKEN_LEN] = '\0';
    g_token_valid = 1;
}

static int check_token(const char *token) {
    if (!g_token_valid || !token || strlen(token) == 0) return 0;
    return strcmp(token, g_admin_token) == 0;
}

static void extract_bearer(const char *headers, char *out, int out_sz) {
    out[0] = '\0';
    const char *p = strstr(headers, "Authorization: Bearer ");
    if (!p) p = strstr(headers, "authorization: Bearer ");
    if (!p) return;
    p += strlen("Authorization: Bearer ");
    int i = 0;
    while (*p && *p != '\r' && *p != '\n' && i < out_sz - 1) {
        out[i++] = *p++;
    }
    out[i] = '\0';
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  JSON HELPERS
 * ═══════════════════════════════════════════════════════════════════════════ */

static int json_get_string(const char *json, const char *key,
                           char *out, int out_sz) {
    char pattern[256];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    const char *p = strstr(json, pattern);
    if (!p) return 0;
    p += strlen(pattern);
    while (*p == ' ' || *p == '\t' || *p == ':' || *p == '\n' || *p == '\r') p++;
    if (*p != '"') return 0;
    p++;
    int i = 0;
    while (*p && *p != '"' && i < out_sz - 1) {
        if (*p == '\\' && *(p + 1)) p++;
        out[i++] = *p++;
    }
    out[i] = '\0';
    return 1;
}

static int json_get_int(const char *json, const char *key, int *out) {
    char pattern[256];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    const char *p = strstr(json, pattern);
    if (!p) return 0;
    p += strlen(pattern);
    while (*p == ' ' || *p == '\t' || *p == ':' || *p == '\n' || *p == '\r') p++;
    if (*p == '"') p++;
    *out = atoi(p);
    return 1;
}

static int json_get_float_array(const char *json, const char *key,
                                float *out, int max_count) {
    char pattern[256];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    const char *p = strstr(json, pattern);
    if (!p) return 0;
    p += strlen(pattern);
    p = strchr(p, '[');
    if (!p) return 0;
    p++;
    int count = 0;
    while (*p && *p != ']' && count < max_count) {
        while (*p == ' ' || *p == ',' || *p == '\t' || *p == '\n' || *p == '\r') p++;
        if (*p == ']') break;
        char *end;
        float val = strtof(p, &end);
        if (end == p) break;
        out[count++] = val;
        p = end;
    }
    return count;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  HTTP HELPERS
 * ═══════════════════════════════════════════════════════════════════════════ */

static const char *mime_for(const char *path) {
    const char *ext = strrchr(path, '.');
    if (!ext) return "application/octet-stream";
    if (strcmp(ext, ".html") == 0) return "text/html; charset=utf-8";
    if (strcmp(ext, ".css")  == 0) return "text/css; charset=utf-8";
    if (strcmp(ext, ".js")   == 0) return "application/javascript; charset=utf-8";
    if (strcmp(ext, ".json") == 0) return "application/json";
    if (strcmp(ext, ".png")  == 0) return "image/png";
    if (strcmp(ext, ".jpg")  == 0) return "image/jpeg";
    if (strcmp(ext, ".ico")  == 0) return "image/x-icon";
    if (strcmp(ext, ".svg")  == 0) return "image/svg+xml";
    if (strcmp(ext, ".csv")  == 0) return "text/csv";
    return "application/octet-stream";
}

static void send_http(SOCKET s, int code, const char *ctype,
                      const char *body, int body_len) {
    const char *reason = "OK";
    if (code == 400) reason = "Bad Request";
    if (code == 401) reason = "Unauthorized";
    if (code == 404) reason = "Not Found";
    if (code == 409) reason = "Conflict";
    if (code == 500) reason = "Internal Server Error";

    char header[1024];
    int hlen = snprintf(header, sizeof(header),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %d\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Access-Control-Allow-Methods: GET, POST, DELETE, OPTIONS\r\n"
        "Access-Control-Allow-Headers: Content-Type, Authorization\r\n"
        "Connection: close\r\n"
        "\r\n",
        code, reason, ctype, body_len);

    send(s, header, hlen, 0);
    if (body && body_len > 0) {
        int sent = 0;
        while (sent < body_len) {
            int n = send(s, body + sent, body_len - sent, 0);
            if (n <= 0) break;
            sent += n;
        }
    }
}

static void send_http_download(SOCKET s, const char *ctype,
                               const char *filename,
                               const char *body, int body_len) {
    char header[1024];
    int hlen = snprintf(header, sizeof(header),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %d\r\n"
        "Content-Disposition: attachment; filename=\"%s\"\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Connection: close\r\n"
        "\r\n",
        ctype, body_len, filename);

    send(s, header, hlen, 0);
    int sent = 0;
    while (sent < body_len) {
        int n = send(s, body + sent, body_len - sent, 0);
        if (n <= 0) break;
        sent += n;
    }
}

static void send_json(SOCKET s, int code, const char *json) {
    send_http(s, code, "application/json", json, (int)strlen(json));
}

static void send_404(SOCKET s) {
    send_json(s, 404, "{\"error\":\"Not found\"}");
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  STATIC FILE SERVER
 * ═══════════════════════════════════════════════════════════════════════════ */

static void serve_static(SOCKET s, const char *url_path) {
    char filepath[512];

    if (strcmp(url_path, "/") == 0) {
        snprintf(filepath, sizeof(filepath), "%s\\index.html", WWW_DIR);
    } else if (strcmp(url_path, "/admin") == 0) {
        snprintf(filepath, sizeof(filepath), "%s\\admin.html", WWW_DIR);
    } else {
        if (strstr(url_path, "..")) { send_404(s); return; }
        snprintf(filepath, sizeof(filepath), "%s%s", WWW_DIR, url_path);
        for (char *p = filepath; *p; p++) {
            if (*p == '/') *p = '\\';
        }
    }

    FILE *f = fopen(filepath, "rb");
    if (!f) { send_404(s); return; }
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = (char *)malloc(fsize);
    if (!buf) { fclose(f); send_404(s); return; }
    fread(buf, 1, fsize, f);
    fclose(f);
    send_http(s, 200, mime_for(filepath), buf, (int)fsize);
    free(buf);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  PUBLIC API HANDLERS
 * ═══════════════════════════════════════════════════════════════════════════ */

/* POST /api/register */
static void api_register_user(SOCKET s, const char *body) {
    char name[128] = {0};
    float desc[DESC_SIZE] = {0};

    if (!json_get_string(body, "name", name, sizeof(name))) {
        send_json(s, 400, "{\"error\":\"Missing 'name'\"}");
        return;
    }
    int n = json_get_float_array(body, "descriptor", desc, DESC_SIZE);
    if (n != DESC_SIZE) {
        char err[256];
        snprintf(err, sizeof(err), "{\"error\":\"Expected %d descriptor values, got %d\"}", DESC_SIZE, n);
        send_json(s, 400, err);
        return;
    }

    EnterCriticalSection(&g_cs);

    /* Update existing user */
    for (int i = 0; i < g_user_count; i++) {
        if (_stricmp(g_users[i].name, name) == 0) {
            memcpy(g_users[i].descriptor, desc, sizeof(desc));
            save_users();

            char today[16]; get_today(today, sizeof(today));
            int already = 0;
            for (int j = 0; j < g_attend_count; j++) {
                if (_stricmp(g_attend[j].name, name) == 0 && strcmp(g_attend[j].date, today) == 0) {
                    already = 1; break;
                }
            }
            if (!already && g_attend_count < MAX_ATTENDANCE) {
                strncpy(g_attend[g_attend_count].name, name, 127);
                strncpy(g_attend[g_attend_count].date, today, 15);
                g_attend_count++;
                save_attendance();
            }

            LeaveCriticalSection(&g_cs);
            char resp[512];
            snprintf(resp, sizeof(resp),
                "{\"success\":true,\"message\":\"User updated\",\"name\":\"%s\",\"date\":\"%s\",\"is_admin\":%s}",
                name, today, g_users[i].is_admin ? "true" : "false");
            send_json(s, 200, resp);
            return;
        }
    }

    if (g_user_count >= MAX_USERS) {
        LeaveCriticalSection(&g_cs);
        send_json(s, 400, "{\"error\":\"Maximum users reached\"}");
        return;
    }

    strncpy(g_users[g_user_count].name, name, 127);
    memcpy(g_users[g_user_count].descriptor, desc, sizeof(desc));
    g_users[g_user_count].is_admin = 0; /* default student */
    g_user_count++;
    save_users();

    char today[16]; get_today(today, sizeof(today));
    if (g_attend_count < MAX_ATTENDANCE) {
        strncpy(g_attend[g_attend_count].name, name, 127);
        strncpy(g_attend[g_attend_count].date, today, 15);
        g_attend_count++;
        save_attendance();
    }

    LeaveCriticalSection(&g_cs);
    printf("[REG] Enrolled user: %s\n", name);
    char resp[512];
    snprintf(resp, sizeof(resp),
        "{\"success\":true,\"message\":\"User registered\",\"name\":\"%s\",\"date\":\"%s\",\"is_admin\":false}",
        name, today);
    send_json(s, 200, resp);
}

/* POST /api/verify - with Facial Admin Recognition & Token Generation */
static void api_verify_user(SOCKET s, const char *body) {
    float desc[DESC_SIZE] = {0};
    int n = json_get_float_array(body, "descriptor", desc, DESC_SIZE);
    if (n != DESC_SIZE) {
        send_json(s, 400, "{\"error\":\"Invalid descriptor\"}");
        return;
    }

    EnterCriticalSection(&g_cs);

    if (g_user_count == 0) {
        LeaveCriticalSection(&g_cs);
        send_json(s, 200, "{\"matched\":false,\"message\":\"No registered users\"}");
        return;
    }

    int   best_idx  = -1;
    float best_dist = 999.0f;
    for (int i = 0; i < g_user_count; i++) {
        float d = euclidean_dist(desc, g_users[i].descriptor, DESC_SIZE);
        if (d < best_dist) { best_dist = d; best_idx = i; }
    }

    if (best_dist > MATCH_THRESHOLD || best_idx < 0) {
        LeaveCriticalSection(&g_cs);
        char resp[256];
        snprintf(resp, sizeof(resp),
            "{\"matched\":false,\"distance\":%.4f,\"message\":\"Face not recognized\"}",
            best_dist);
        send_json(s, 200, resp);
        return;
    }

    /* Record attendance */
    char today[16]; get_today(today, sizeof(today));
    int already = 0;
    for (int i = 0; i < g_attend_count; i++) {
        if (_stricmp(g_attend[i].name, g_users[best_idx].name) == 0 &&
            strcmp(g_attend[i].date, today) == 0) { already = 1; break; }
    }
    if (!already && g_attend_count < MAX_ATTENDANCE) {
        strncpy(g_attend[g_attend_count].name, g_users[best_idx].name, 127);
        strncpy(g_attend[g_attend_count].date, today, 15);
        g_attend_count++;
        save_attendance();
    }

    int is_adm = g_users[best_idx].is_admin;
    if (is_adm) {
        /* Generate or refresh session token so admin can directly bypass lock screen */
        generate_token();
        printf("[ADMIN] Face of admin recognized: %s -> redirecting to /admin\n", g_users[best_idx].name);
    }

    char resp[512];
    snprintf(resp, sizeof(resp),
        "{\"matched\":true,\"name\":\"%s\",\"distance\":%.4f,"
        "\"already_marked\":%s,\"date\":\"%s\",\"is_admin\":%s,\"admin_token\":\"%s\"}",
        g_users[best_idx].name, best_dist,
        already ? "true" : "false", today,
        is_adm ? "true" : "false",
        is_adm ? g_admin_token : "");

    LeaveCriticalSection(&g_cs);
    send_json(s, 200, resp);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  ADMIN API HANDLERS
 * ═══════════════════════════════════════════════════════════════════════════ */

/* POST /api/admin/login — body: { "password": "..." } */
static void api_admin_login(SOCKET s, const char *body) {
    char pw[256] = {0};
    if (!json_get_string(body, "password", pw, sizeof(pw))) {
        send_json(s, 400, "{\"error\":\"Missing password\"}");
        return;
    }

    if (strcmp(pw, ADMIN_PASSWORD) != 0) {
        printf("[ADMIN] Invalid password attempt\n");
        send_json(s, 401, "{\"error\":\"Wrong password\"}");
        return;
    }

    generate_token();
    printf("[ADMIN] Password authenticated (token: %.8s...)\n", g_admin_token);

    char resp[256];
    snprintf(resp, sizeof(resp), "{\"success\":true,\"token\":\"%s\"}", g_admin_token);
    send_json(s, 200, resp);
}

/* POST /api/admin/toggle_admin — body: { "name": "...", "is_admin": 1 } */
static void api_admin_toggle_admin(SOCKET s, const char *body) {
    char name[128] = {0};
    int is_adm = 0;

    if (!json_get_string(body, "name", name, sizeof(name))) {
        send_json(s, 400, "{\"error\":\"Missing 'name'\"}");
        return;
    }
    json_get_int(body, "is_admin", &is_adm);

    EnterCriticalSection(&g_cs);

    int found = -1;
    for (int i = 0; i < g_user_count; i++) {
        if (_stricmp(g_users[i].name, name) == 0) {
            g_users[i].is_admin = is_adm ? 1 : 0;
            found = i;
            break;
        }
    }

    if (found < 0) {
        LeaveCriticalSection(&g_cs);
        send_json(s, 404, "{\"error\":\"User not found\"}");
        return;
    }

    save_users();
    LeaveCriticalSection(&g_cs);

    printf("[ADMIN] User %s role changed to: %s\n", name, is_adm ? "Admin" : "Student");
    send_json(s, 200, "{\"success\":true}");
}

/* GET /api/admin/attendance */
static void api_admin_attendance(SOCKET s) {
    char today[16]; get_today(today, sizeof(today));

    EnterCriticalSection(&g_cs);

    int buf_size = 256 + g_user_count * 350;
    char *buf = (char *)malloc(buf_size);
    if (!buf) { LeaveCriticalSection(&g_cs); send_json(s, 500, "{\"error\":\"OOM\"}"); return; }

    int pos = 0;
    pos += snprintf(buf + pos, buf_size - pos,
                    "{\"date\":\"%s\",\"total\":%d,", today, g_user_count);

    int present_count = 0;
    for (int i = 0; i < g_user_count; i++) {
        for (int j = 0; j < g_attend_count; j++) {
            if (_stricmp(g_attend[j].name, g_users[i].name) == 0 &&
                strcmp(g_attend[j].date, today) == 0) { present_count++; break; }
        }
    }

    pos += snprintf(buf + pos, buf_size - pos,
                    "\"present\":%d,\"absent\":%d,\"users\":[",
                    present_count, g_user_count - present_count);

    for (int i = 0; i < g_user_count; i++) {
        int present = 0;
        for (int j = 0; j < g_attend_count; j++) {
            if (_stricmp(g_attend[j].name, g_users[i].name) == 0 &&
                strcmp(g_attend[j].date, today) == 0) { present = 1; break; }
        }
        if (i > 0) pos += snprintf(buf + pos, buf_size - pos, ",");
        pos += snprintf(buf + pos, buf_size - pos,
            "{\"name\":\"%s\",\"status\":\"%s\",\"is_admin\":%s}",
            g_users[i].name,
            present ? "present" : "absent",
            g_users[i].is_admin ? "true" : "false");
    }

    pos += snprintf(buf + pos, buf_size - pos, "]}");
    LeaveCriticalSection(&g_cs);

    send_json(s, 200, buf);
    free(buf);
}

/* GET /api/admin/csv */
static void api_admin_csv(SOCKET s) {
    char today[16]; get_today(today, sizeof(today));

    EnterCriticalSection(&g_cs);

    int buf_size = 256 + g_user_count * 200;
    char *buf = (char *)malloc(buf_size);
    if (!buf) { LeaveCriticalSection(&g_cs); send_json(s, 500, "{\"error\":\"OOM\"}"); return; }

    int pos = 0;
    buf[pos++] = (char)0xEF;
    buf[pos++] = (char)0xBB;
    buf[pos++] = (char)0xBF;

    pos += snprintf(buf + pos, buf_size - pos, "S.No,Name,Role,Status,Date\r\n");

    for (int i = 0; i < g_user_count; i++) {
        int present = 0;
        for (int j = 0; j < g_attend_count; j++) {
            if (_stricmp(g_attend[j].name, g_users[i].name) == 0 &&
                strcmp(g_attend[j].date, today) == 0) { present = 1; break; }
        }
        pos += snprintf(buf + pos, buf_size - pos,
            "%d,\"%s\",%s,%s,%s\r\n",
            i + 1, g_users[i].name,
            g_users[i].is_admin ? "Admin" : "Student",
            present ? "Present" : "Absent", today);
    }

    LeaveCriticalSection(&g_cs);

    char filename[64];
    snprintf(filename, sizeof(filename), "attendance_%s.csv", today);
    send_http_download(s, "text/csv; charset=utf-8", filename, buf, pos);
    free(buf);
}

/* POST /api/admin/delete */
static void api_admin_delete(SOCKET s, const char *body) {
    char name[128] = {0};
    if (!json_get_string(body, "name", name, sizeof(name))) {
        send_json(s, 400, "{\"error\":\"Missing 'name'\"}");
        return;
    }

    EnterCriticalSection(&g_cs);

    int found = -1;
    for (int i = 0; i < g_user_count; i++) {
        if (_stricmp(g_users[i].name, name) == 0) { found = i; break; }
    }
    if (found < 0) {
        LeaveCriticalSection(&g_cs);
        send_json(s, 404, "{\"error\":\"User not found\"}");
        return;
    }

    for (int i = found; i < g_user_count - 1; i++) g_users[i] = g_users[i + 1];
    g_user_count--;
    save_users();

    int w = 0;
    for (int i = 0; i < g_attend_count; i++) {
        if (_stricmp(g_attend[i].name, name) != 0) {
            if (w != i) g_attend[w] = g_attend[i];
            w++;
        }
    }
    g_attend_count = w;
    save_attendance();

    LeaveCriticalSection(&g_cs);
    printf("[ADMIN] Deleted user: %s\n", name);
    send_json(s, 200, "{\"success\":true}");
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  REQUEST HANDLER
 * ═══════════════════════════════════════════════════════════════════════════ */

static void handle_request(SOCKET client) {
    char *req = (char *)calloc(MAX_HEADERS + MAX_BODY + 1, 1);
    if (!req) { closesocket(client); return; }

    int total = 0, header_end = -1;

    while (total < MAX_HEADERS) {
        int n = recv(client, req + total, MAX_HEADERS - total, 0);
        if (n <= 0) break;
        total += n;
        for (int i = 0; i <= total - 4; i++) {
            if (req[i]=='\r' && req[i+1]=='\n' && req[i+2]=='\r' && req[i+3]=='\n') {
                header_end = i + 4; break;
            }
        }
        if (header_end >= 0) break;
    }
    if (header_end < 0) { free(req); closesocket(client); return; }

    char method[16] = {0}, path[512] = {0};
    sscanf(req, "%15s %511s", method, path);

    if (_stricmp(method, "OPTIONS") == 0) {
        send_http(client, 200, "text/plain", "", 0);
        free(req); closesocket(client); return;
    }

    int content_length = 0;
    const char *cl = strstr(req, "Content-Length:");
    if (!cl) cl = strstr(req, "content-length:");
    if (cl) content_length = atoi(strchr(cl, ':') + 1);

    if (content_length > 0 && content_length < MAX_BODY) {
        int body_remaining = content_length - (total - header_end);
        while (body_remaining > 0) {
            int n = recv(client, req + total, body_remaining, 0);
            if (n <= 0) break;
            total += n; body_remaining -= n;
        }
    }
    req[total] = '\0';
    char *body = req + header_end;

    char bearer[TOKEN_LEN + 1] = {0};
    extract_bearer(req, bearer, sizeof(bearer));

    /* ── ROUTING ────────────────────────────────────────────────────────── */

    if (_stricmp(method, "POST") == 0) {
        if (strcmp(path, "/api/register") == 0) {
            api_register_user(client, body);
        } else if (strcmp(path, "/api/verify") == 0) {
            api_verify_user(client, body);
        } else if (strcmp(path, "/api/admin/login") == 0) {
            api_admin_login(client, body);
        } else if (strcmp(path, "/api/admin/toggle_admin") == 0) {
            if (!check_token(bearer)) send_json(client, 401, "{\"error\":\"Unauthorized\"}");
            else api_admin_toggle_admin(client, body);
        } else if (strcmp(path, "/api/admin/delete") == 0) {
            if (!check_token(bearer)) send_json(client, 401, "{\"error\":\"Unauthorized\"}");
            else api_admin_delete(client, body);
        } else {
            send_404(client);
        }
    } else if (_stricmp(method, "GET") == 0) {
        if (strcmp(path, "/api/admin/attendance") == 0) {
            if (!check_token(bearer)) send_json(client, 401, "{\"error\":\"Unauthorized\"}");
            else api_admin_attendance(client);
        } else if (strcmp(path, "/api/admin/csv") == 0) {
            if (!check_token(bearer)) send_json(client, 401, "{\"error\":\"Unauthorized\"}");
            else api_admin_csv(client);
        } else if (strcmp(path, "/api/admin/verify") == 0) {
            if (!check_token(bearer)) send_json(client, 401, "{\"error\":\"Unauthorized\"}");
            else send_json(client, 200, "{\"valid\":true}");
        } else {
            serve_static(client, path);
        }
    } else {
        send_404(client);
    }

    free(req);
    closesocket(client);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  MAIN
 * ═══════════════════════════════════════════════════════════════════════════ */

static DWORD WINAPI client_thread(LPVOID param) {
    handle_request((SOCKET)(intptr_t)param);
    return 0;
}

int main(void) {
    printf("=============================================\n");
    printf("  Face Attendance System — C HTTP Server\n");
    printf("  Port: %d\n", SERVER_PORT);
    printf("  Admin Password: %s\n", ADMIN_PASSWORD);
    printf("=============================================\n\n");

    ensure_dirs();
    InitializeCriticalSection(&g_cs);
    load_users();
    load_attendance();

    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        fprintf(stderr, "[FATAL] WSAStartup failed\n"); return 1;
    }

    SOCKET server_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_sock == INVALID_SOCKET) {
        fprintf(stderr, "[FATAL] socket() failed\n"); WSACleanup(); return 1;
    }

    int opt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));

    struct sockaddr_in addr = {0};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(SERVER_PORT);

    if (bind(server_sock, (struct sockaddr *)&addr, sizeof(addr)) == SOCKET_ERROR) {
        fprintf(stderr, "[FATAL] bind() failed\n");
        closesocket(server_sock); WSACleanup(); return 1;
    }

    listen(server_sock, SOMAXCONN);

    printf("[OK] Kiosk: http://localhost:%d\n", SERVER_PORT);
    printf("[OK] Admin: http://localhost:%d/admin\n\n", SERVER_PORT);

    for (;;) {
        struct sockaddr_in ca; int cl = sizeof(ca);
        SOCKET c = accept(server_sock, (struct sockaddr *)&ca, &cl);
        if (c == INVALID_SOCKET) continue;
        HANDLE h = CreateThread(NULL, 0, client_thread, (LPVOID)(intptr_t)c, 0, NULL);
        if (h) CloseHandle(h); else closesocket(c);
    }

    closesocket(server_sock);
    DeleteCriticalSection(&g_cs);
    WSACleanup();
    return 0;
}
