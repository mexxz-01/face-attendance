# 🏛️ Imperial Biometric Face Attendance System

A high-performance facial recognition attendance system engineered with a pure **C** HTTP/REST backend and an in-browser neural network powered by **face-api.js** (TinyFaceDetector + 128-dimensional facial embeddings).

Featuring a luxury deep crimson and metallic champagne gold aesthetic, real-time autonomous kiosk scanning, an isolated hidden administrator portal, and instant **Excel (.xlsx)** & **PDF** reporting.

---

## ✨ Key Features

- **⚡ Hardware-Accelerated Neural Tracking**: Powered by `tinyFaceDetector` (~190KB) running at ~15ms per frame with near-zero CPU overhead.
- **🤖 Autonomous Kiosk Terminal**:
  - **No Face**: Displays *"Face didn't detected"*.
  - **New Face**: Displays *"New face detected - enter your name to register"* and reveals an inline registration drawer.
  - **Recognized Student**: Confirms attendance automatically (*"{Student Name} + attendance confirmed"*).
  - **Facial Admin Recognition**: When an administrator faces the camera, the system recognizes their role and **automatically redirects straight to the Admin Portal (`/admin`)**.
- **🛡️ Secure Hidden Admin Portal (`/admin`)**:
  - Gated behind access key (`mexxz@2007`) or facial admin biometric bypass.
  - Complete roster inspection with real-time stats (Total Enrolled, Present, Absent).
  - **Role Management**: Easily promote any student to an Admin (`👑 Make Admin`) or demote back to Student.
  - **Excel Export**: Download styled `.xlsx` spreadsheets with SheetJS.
  - **PDF Export**: Generate clean formatted PDF attendance summaries with jsPDF and autoTable.
  - **Search & Record Deletion**: Real-time name filtering and student record deletion.

---

## 🛠️ Architecture

```
┌────────────────────────────────────────────────────────┐
│               Client Browser (HTML/CSS/JS)             │
│  ┌──────────────────────────────────────────────────┐  │
│  │ face-api.js (TinyFaceDetector + Landmark + Net)  │  │
│  │ - 15ms frame inference                           │  │
│  │ - 128-D facial vector embedding extraction       │  │
│  └─────────────────────────┬────────────────────────┘  │
└────────────────────────────┼───────────────────────────┘
                             │ JSON REST API
┌────────────────────────────▼───────────────────────────┐
│              Pure C Multithreaded HTTP Server          │
│  - Built on Windows Winsock2 API                       │
│  - Euclidean vector distance comparison                │
│  - Dynamic Bearer token authentication                 │
│  - Binary record persistence (data/users.dat)          │
└────────────────────────────────────────────────────────┘
```

---

## 🚀 Getting Started

### Prerequisites
- **GCC (MinGW)** or **MSVC (cl)** on Windows
- Modern web browser with webcam access (Chrome, Edge, Firefox, Brave)

### 1. Build the Server
Run the automated build script:
```cmd
build.bat
```
*Or compile manually with GCC:*
```cmd
gcc server.c -o server.exe -lws2_32 -lm -O2
```

### 2. Run the System
```cmd
server.exe
```

### 3. Open in Browser
- **Public Terminal Kiosk:** [http://localhost:8080](http://localhost:8080)
- **Admin Portal:** [http://localhost:8080/admin](http://localhost:8080/admin)
  - **Default Admin Key:** `mexxz@2007`

---

## 📂 Project Structure

```
face-attendance/
├── server.c           # Pure C HTTP server & biometric matcher
├── build.bat          # 1-click compilation script
├── README.md          # Documentation
├── .gitignore         # Build & runtime ignore rules
├── www/
│   ├── index.html     # Autonomous biometric kiosk terminal
│   ├── app.js         # Optimized TinyFace recognition loop
│   ├── admin.html     # Secured administrator portal
│   ├── admin.js       # Admin logic, role toggle & Excel/PDF export
│   └── style.css      # Deep crimson & metallic gold design system
└── data/
    └── .gitkeep       # Binary storage directory for users & records
```

---

## 📄 License
MIT License. Created by [mexxz-01](https://github.com/mexxz-01).
