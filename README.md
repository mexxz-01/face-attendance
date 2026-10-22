# 🏛️ Imperial Biometric Face Attendance System

A high-performance facial recognition attendance system engineered with:
1. **Python 3.13 + FastAPI + OpenCV + NumPy** backend (sub-millisecond vector math, SQLite database, server-side Excel `.xlsx` and PDF reporting)
2. **Pure C HTTP Server** alternative (Winsock2 multithreaded binary)
3. **Hardware-Accelerated Client Engine**: in-browser neural network powered by **face-api.js** (`tinyFaceDetector` + 128-dimensional facial embeddings)
4. **Modern Luxury UI**: Deep wine crimson and champagne gold aesthetic with glassmorphism, autonomous kiosk tracking, and secured administrator panel.

---

## ✨ Key Features

- **⚡ Sub-Millisecond AI Vector Matching**: Powered by vectorized NumPy Euclidean norm across 128-dimensional facial descriptors in under `0.05ms`.
- **🤖 Autonomous Kiosk Terminal**:
  - **No Face**: Displays *"Face didn't detected"*.
  - **New Face**: Displays *"New face detected - enter your name to register"* and prompts with an inline drawer.
  - **Recognized Student**: Confirms attendance automatically (*"{Student Name} + attendance confirmed"*).
  - **Facial Admin Recognition**: When an administrator faces the camera, the system recognizes their role and **automatically redirects straight to the Admin Portal (`/admin`)**.
- **🛡️ Secure Hidden Admin Portal (`/admin`)**:
  - Gated behind access key (`mexxz@2007`) or facial admin biometric bypass.
  - Complete roster inspection with real-time stats (Total Enrolled, Present, Absent).
  - **Role Management**: Easily promote any student to an Admin (`👑 Make Admin`) or demote back to Student.
  - **High-Performance Excel Export**: Generates styled `.xlsx` spreadsheets with `openpyxl`.
  - **High-Performance PDF Export**: Generates styled PDF summaries with `reportlab`.
  - **Search & Record Deletion**: Real-time name filtering and student record deletion.

---

## 🛠️ Multi-Language Architecture

```
┌────────────────────────────────────────────────────────┐
│               Client Browser (HTML5 / CSS3 / JS)       │
│  ┌──────────────────────────────────────────────────┐  │
│  │ face-api.js (TinyFaceDetector + Landmark + Net)  │  │
│  │ - 15ms frame inference                           │  │
│  │ - 128-D facial vector embedding extraction       │  │
│  └─────────────────────────┬────────────────────────┘  │
└────────────────────────────┼───────────────────────────┘
                             │ JSON REST API
┌────────────────────────────▼───────────────────────────┐
│     Backend Engine Option A: Python 3 + FastAPI       │
│  - Asynchronous Uvicorn server                         │
│  - Vectorized NumPy Euclidean matching (<0.05ms)       │
│  - SQLite database persistence (attendance.db)         │
│  - Server-side openpyxl & reportlab export             │
├────────────────────────────────────────────────────────┤
│     Backend Engine Option B: Pure C Server (server.c)  │
│  - Native Windows Winsock2 multithreading              │
│  - Binary struct file persistence (data/users.dat)     │
└────────────────────────────────────────────────────────┘
```

---

## 🚀 Getting Started

### Option 1: Python Engine (Recommended)

1. **Install Dependencies:**
   ```bash
   pip install -r requirements.txt
   ```

2. **Run Server:**
   ```bash
   python app.py
   ```
   *Or double-click `run.bat`*

### Option 2: C Engine

1. **Build with GCC (MinGW):**
   ```cmd
   build.bat
   ```
   *Or:*
   ```cmd
   gcc server.c -o server.exe -lws2_32 -lm -O2
   ```

2. **Run Server:**
   ```cmd
   server.exe
   ```

---

## 🌐 Endpoints

- **Live Public Terminal:** [http://localhost:8080](http://localhost:8080)
- **Secured Admin Portal:** [http://localhost:8080/admin](http://localhost:8080/admin)
  - **Security Password:** `mexxz@2007` (or scan your admin face at the kiosk)

---

## 📂 Project Structure

```
face-attendance/
├── app.py             # Python FastAPI + NumPy + OpenCV + SQLite backend
├── requirements.txt   # Python dependencies
├── run.bat            # 1-click Python launcher
├── server.c           # Pure C HTTP server alternative
├── build.bat          # 1-click C compilation script
├── README.md          # Full documentation
├── .gitignore         # Clean repository ignore rules
├── www/
│   ├── index.html     # Kiosk biometric terminal
│   ├── app.js         # Optimized TinyFace recognition loop
│   ├── admin.html     # Secured administrator portal
│   ├── admin.js       # Admin logic, role toggle & dual export
│   └── style.css      # Deep crimson & metallic gold design system
└── data/
    ├── attendance.db  # SQLite database (auto-created & migrated)
    └── .gitkeep       # Git persistence placeholder
```

---

## 📄 License
MIT License. Created by [mexxz-01](https://github.com/mexxz-01).
