# ProxiGate: IoT Biometric Lock System 🛡️

ProxiGate is a cutting-edge, IoT-enabled physical access control system that combines biometric security with real-time monitoring and remote management. The project features a full-stack solution including an ESP32-based hardware layer, a FastAPI backend, and a modern React frontend dashboard.

---

## 🌟 Key Features

-   **Biometric Authentication**: Fingerprint recognition for secure physical access.
-   **Facial Monitoring**: Integrated ESP32-CAM for real-time video surveillance and facial snapshots.
-   **Remote Management**: Web-based dashboard to manage users, view logs, and monitor lock status.
-   **Real-time Alerts**: Instant updates on access attempts and system status via webhooks/sockets.
-   **Secure Credentials**: Hashed passwords and secure database models using FastAPI and SQLAlchemy.

---

## 🛠️ Technology Stack

### **Hardware Layer**
-   **Microcontrollers**: ESP32 DevKit, ESP32-CAM.
-   **Sensors**: AS608 Fingerprint Sensor.
-   **Connectivity**: WiFi (HTTP/WebSockets).

### **Backend (API)**
-   **Framework**: [FastAPI](https://fastapi.tiangolo.com/) (Python).
-   **Database**: SQLite with [SQLAlchemy](https://www.sqlalchemy.org/) ORM.
-   **Authentication**: JWT-based security with Passlib/Bcrypt.

### **Frontend (Dashboard)**
-   **Framework**: [React 19](https://react.dev/) with [Vite](https://vitejs.dev/).
-   **Styling**: [Tailwind CSS 4](https://tailwindcss.com/).
-   **Icons**: [Lucide React](https://lucide.dev/).

---

## 🚀 Getting Started

### **1. Backend Setup**
Navigate to the `backend` directory:
```bash
cd backend
python -m venv venv
source venv/bin/activate  # On Windows: venv\\Scripts\\activate
pip install -r requirements.txt
uvicorn main:app --reload
```

### **2. Frontend Setup**
Navigate to the `frontend` directory:
```bash
cd frontend
npm install
npm run dev
```

### **3. Hardware Configuration**
1.  Open `hardware/ESP32_Lock/ESP32_Lock.ino` in the Arduino IDE.
2.  Configure your WiFi credentials (`SSID`, `PASSWORD`) and the Backend API URL.
3.  Upload the code to your ESP32.
4.  Repeat for `hardware/ESP32_CAM/ESP32_CAM.ino`.

---

## 📂 Project Structure

```text
ProxiGate/
├── backend/            # FastAPI application logic and database
├── frontend/           # React dashboard UI
├── hardware/           # ESP32 and ESP32-CAM Arduino sketches
│   ├── ESP32_CAM/      # Camera streaming and facial capture
│   └── ESP32_Lock/     # Fingerprint sensor and solenoid control
└── .gitignore          # Project-wide git exclusions
```

---

## 🔒 Security & Privacy

-   All biometric data is processed locally on the hardware or encrypted if stored.
-   Passwords are never stored in plain text.
-   Secure communication between IoT devices and the central server is prioritized.

---

### Developed with ❤️ for Secure Access.
