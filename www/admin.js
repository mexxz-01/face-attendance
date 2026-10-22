/* ═══════════════════════════════════════════════════════════════════════════
 *  ADMIN PORTAL JAVASCRIPT & EXPORT ENGINE
 *  Supports Role Elevation (Student <-> Admin) & Face-Token Authentication
 * ═══════════════════════════════════════════════════════════════════════════ */

let currentAttendanceData = null;
let activeSessionToken = sessionStorage.getItem('imperial_admin_token') || null;

window.addEventListener('DOMContentLoaded', async () => {
    if (activeSessionToken) {
        const isValid = await verifyCurrentToken(activeSessionToken);
        if (isValid) {
            revealAdminDashboard();
            loadAttendanceRecords();
            return;
        }
    }
    showLockScreen();
});

/* ── 1. Authentication ───────────────────────────────────────────────────── */

async function verifyCurrentToken(token) {
    try {
        const res = await fetch('/api/admin/verify', {
            headers: { 'Authorization': `Bearer ${token}` }
        });
        return res.status === 200;
    } catch {
        return false;
    }
}

async function handleAdminLogin(event) {
    event.preventDefault();
    const pwInput = document.getElementById('adminPassword');
    const errDiv = document.getElementById('lockError');
    const btn = document.getElementById('loginBtn');
    const password = pwInput.value.trim();

    if (!password) {
        errDiv.textContent = 'Please enter administrator password.';
        return;
    }

    btn.disabled = true;
    btn.textContent = 'Authenticating...';
    errDiv.textContent = '';

    try {
        const res = await fetch('/api/admin/login', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ password })
        });

        const data = await res.json();
        if (data.success && data.token) {
            activeSessionToken = data.token;
            sessionStorage.setItem('imperial_admin_token', activeSessionToken);
            revealAdminDashboard();
            loadAttendanceRecords();
        } else {
            errDiv.textContent = data.error || 'Access Denied: Invalid Password';
            pwInput.value = '';
            pwInput.focus();
        }
    } catch (err) {
        errDiv.textContent = 'Server communication fault: ' + err.message;
    }

    btn.disabled = false;
    btn.textContent = 'Unlock Dashboard';
}

function revealAdminDashboard() {
    document.getElementById('lockScreen').style.display = 'none';
    document.getElementById('adminContent').style.display = 'flex';
}

function showLockScreen() {
    document.getElementById('lockScreen').style.display = 'flex';
    document.getElementById('adminContent').style.display = 'none';
    const input = document.getElementById('adminPassword');
    if (input) input.focus();
}

function logoutAdmin() {
    sessionStorage.removeItem('imperial_admin_token');
    activeSessionToken = null;
    showLockScreen();
}

/* ── 2. Data Fetching & Table Rendering ───────────────────────────────────── */

async function loadAttendanceRecords() {
    try {
        const res = await fetch('/api/admin/attendance', {
            headers: { 'Authorization': `Bearer ${activeSessionToken}` }
        });

        if (res.status === 401) {
            logoutAdmin();
            return;
        }

        const data = await res.json();
        currentAttendanceData = data;

        document.getElementById('adminDate').textContent = `Record Date: ${data.date}`;
        document.getElementById('statEnrolled').textContent = data.total;
        document.getElementById('statPresent').textContent = data.present;
        document.getElementById('statAbsent').textContent = data.absent;

        renderTable(data.users, data.date);
    } catch (err) {
        console.error('[ERR] Failed to load attendance:', err);
    }
}

function renderTable(users, dateStr) {
    const tbody = document.getElementById('attendanceTableBody');
    if (!users || users.length === 0) {
        tbody.innerHTML = `
            <tr>
                <td colspan="6" style="text-align: center; color: var(--text-muted); padding: 3rem;">
                    No student records discovered in database.
                </td>
            </tr>`;
        return;
    }

    /* Sort by admin first, then present, then name */
    const sorted = [...users].sort((a, b) => {
        if (a.is_admin !== b.is_admin) return b.is_admin ? 1 : -1;
        if (a.status !== b.status) return a.status === 'present' ? -1 : 1;
        return a.name.localeCompare(b.name);
    });

    tbody.innerHTML = sorted.map((u, idx) => `
        <tr>
            <td style="color: var(--gold-400); font-weight: 700;">#${idx + 1}</td>
            <td style="font-weight: 600; font-size: 0.95rem; color: #fff;">
                ${escapeHtml(u.name)}
            </td>
            <td>
                ${u.is_admin
                    ? `<span style="background: rgba(212, 175, 55, 0.2); color: var(--gold-300); border: 1px solid var(--gold-400); padding: 0.25rem 0.65rem; border-radius: 4px; font-size: 0.76rem; font-weight: 700; letter-spacing: 0.08em;">
                        &#9813; ADMIN
                       </span>`
                    : `<span style="background: rgba(255, 255, 255, 0.05); color: var(--text-muted); border: 1px solid rgba(255, 255, 255, 0.1); padding: 0.25rem 0.65rem; border-radius: 4px; font-size: 0.76rem; font-weight: 600;">
                        STUDENT
                       </span>`
                }
            </td>
            <td>
                <span class="status-pill ${u.status}">
                    ${u.status === 'present' ? '&#10003; Present' : '&#10007; Absent'}
                </span>
            </td>
            <td style="color: var(--text-secondary); font-size: 0.88rem;">${dateStr}</td>
            <td style="text-align: right; display: flex; gap: 0.4rem; justify-content: flex-end; align-items: center;">
                ${u.is_admin
                    ? `<button class="btn-action" style="padding: 0.35rem 0.65rem; font-size: 0.76rem; background: rgba(78, 7, 16, 0.5); border: 1px solid rgba(212, 175, 55, 0.3); color: var(--text-secondary);" onclick="toggleAdminRole('${escapeHtml(u.name)}', false)">
                        Demote
                       </button>`
                    : `<button class="btn-action" style="padding: 0.35rem 0.65rem; font-size: 0.76rem; background: linear-gradient(135deg, rgba(212, 175, 55, 0.25), rgba(122, 12, 26, 0.35)); border: 1px solid var(--gold-400); color: var(--gold-200);" onclick="toggleAdminRole('${escapeHtml(u.name)}', true)">
                        &#9813; Make Admin
                       </button>`
                }
                <button class="btn-del" onclick="deleteStudentRecord('${escapeHtml(u.name)}')">
                    Delete
                </button>
            </td>
        </tr>
    `).join('');
}

function filterAttendanceTable() {
    if (!currentAttendanceData) return;
    const term = document.getElementById('searchInput').value.toLowerCase().trim();
    const filtered = currentAttendanceData.users.filter(u => u.name.toLowerCase().includes(term));
    renderTable(filtered, currentAttendanceData.date);
}

/* ── 3. Role Elevation (Make Admin / Revoke Admin) ────────────────────────── */

async function toggleAdminRole(studentName, makeAdmin) {
    const actionName = makeAdmin ? "promote to Admin" : "revoke Admin privileges for";
    if (!confirm(`Are you sure you want to ${actionName} "${studentName}"?`)) return;

    try {
        const res = await fetch('/api/admin/toggle_admin', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
                'Authorization': `Bearer ${activeSessionToken}`
            },
            body: JSON.stringify({ name: studentName, is_admin: makeAdmin ? 1 : 0 })
        });
        const data = await res.json();
        if (data.success) {
            loadAttendanceRecords();
        } else {
            alert('Failed to update role: ' + (data.error || 'Server error'));
        }
    } catch (err) {
        alert('Network fault: ' + err.message);
    }
}

async function deleteStudentRecord(studentName) {
    if (!confirm(`Are you certain you wish to remove student: "${studentName}"?`)) return;

    try {
        const res = await fetch('/api/admin/delete', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
                'Authorization': `Bearer ${activeSessionToken}`
            },
            body: JSON.stringify({ name: studentName })
        });
        const data = await res.json();
        if (data.success) {
            loadAttendanceRecords();
        } else {
            alert('Failed to delete student: ' + (data.error || 'Unknown error'));
        }
    } catch (err) {
        alert('Network fault: ' + err.message);
    }
}

/* ── 4. Excel Export (.xlsx) ──────────────────────────────────────────────── */

function exportToExcel() {
    if (!currentAttendanceData || !currentAttendanceData.users || currentAttendanceData.users.length === 0) {
        alert('No attendance data available to export.');
        return;
    }

    const rows = currentAttendanceData.users.map((u, i) => ({
        "Serial No.": i + 1,
        "Name": u.name,
        "Role": u.is_admin ? "Admin" : "Student",
        "Attendance Status": u.status === 'present' ? 'Present' : 'Absent',
        "Record Date": currentAttendanceData.date
    }));

    const ws = XLSX.utils.json_to_sheet(rows);
    const wb = XLSX.utils.book_new();
    XLSX.utils.book_append_sheet(wb, ws, "Attendance");

    ws['!cols'] = [
        { wch: 12 },
        { wch: 30 },
        { wch: 14 },
        { wch: 20 },
        { wch: 16 }
    ];

    const filename = `Attendance_${currentAttendanceData.date}.xlsx`;
    XLSX.writeFile(wb, filename);
}

/* ── 5. PDF Export (.pdf) ─────────────────────────────────────────────────── */

function exportToPdf() {
    if (!currentAttendanceData || !currentAttendanceData.users || currentAttendanceData.users.length === 0) {
        alert('No attendance data available to export.');
        return;
    }

    const { jsPDF } = window.jspdf;
    const doc = new jsPDF();

    doc.setFillColor(78, 7, 16);
    doc.rect(0, 0, 210, 32, 'F');

    doc.setTextColor(245, 232, 190);
    doc.setFont("helvetica", "bold");
    doc.setFontSize(16);
    doc.text("ATTENDANCE SUMMARY REPORT", 105, 15, { align: "center" });

    doc.setFontSize(9);
    doc.setTextColor(255, 255, 255);
    doc.text(`Biometric Facial Attendance Record  |  Date: ${currentAttendanceData.date}`, 105, 24, { align: "center" });

    doc.setTextColor(60, 60, 60);
    doc.setFontSize(10);
    doc.text(`Total Enrolled: ${currentAttendanceData.total}   |   Present: ${currentAttendanceData.present}   |   Absent: ${currentAttendanceData.absent}`, 14, 42);

    const tableData = currentAttendanceData.users.map((u, i) => [
        (i + 1).toString(),
        u.name,
        u.is_admin ? "ADMIN" : "STUDENT",
        u.status === 'present' ? 'PRESENT' : 'ABSENT',
        currentAttendanceData.date
    ]);

    doc.autoTable({
        startY: 48,
        head: [['#', 'Name', 'Role', 'Status', 'Date']],
        body: tableData,
        headStyles: {
            fillColor: [122, 12, 26],
            textColor: [245, 232, 190],
            fontStyle: 'bold'
        },
        alternateRowStyles: {
            fillColor: [250, 246, 247]
        },
        styles: {
            fontSize: 9.5,
            cellPadding: 4
        },
        didParseCell: function(data) {
            if (data.section === 'body' && data.column.index === 3) {
                if (data.cell.raw === 'PRESENT') {
                    data.cell.styles.textColor = [184, 144, 37];
                    data.cell.styles.fontStyle = 'bold';
                } else {
                    data.cell.styles.textColor = [168, 20, 38];
                }
            }
        }
    });

    doc.save(`Attendance_${currentAttendanceData.date}.pdf`);
}

function escapeHtml(str) {
    const div = document.createElement('div');
    div.textContent = str;
    return div.innerHTML;
}
