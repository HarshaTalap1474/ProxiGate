# ProxiGate: IoT Biometric Lock System 🛡️

ProxiGate is a cutting-edge, IoT-enabled physical access control system that combines biometric security with real-time monitoring and remote management. The project features a full-stack solution including an ESP32-based hardware layer, a FastAPI backend, and a modern React frontend dashboard.

---

## 🌟 Key Features

-   **Biometric Authentication**: Fingerprint recognition for secure physical access.
-   **Multi-Factor Fallbacks**: Multi-PIN configuration (Admin and User PINs) changeable natively on the dashboard and 16x2 Keypad module.
-   **Remote Management**: Web-based dashboard to enroll users, revoke access, and monitor real-time module status tracking (Online/Offline badges).
-   **Real-Time Video**: 180° rotated live stream from the ESP32-CAM on the Admin Dashboard.
-   **No-Code Network Configuration**: Dynamic Captive Portals using `WiFiManager`. Never re-compile code to connect to new WiFi networks!

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

### **3. Hardware Flashing & Wiring**
1. Ensure the `WiFiManager` library (by tzapu) is installed via Arduino IDE Library Manager.
2. Upload `hardware/ESP32_Lock/ESP32_Lock.ino` to your primary ESP32.
3. Upload `hardware/ESP32_CAM/ESP32_CAM.ino` to your Camera module. 

**ESP-CAM Wiring (Crucial for stability)**
- Supply **stable 5V/2A** power (e.g. from the ESP32 Vin or external source). Do not rely solely on PC USB power when initializing the WiFi modem.

### **4. Headless WiFi Configuration (Captive Portal)**
The ESP32s no longer use hardcoded WiFi passwords! On initial boot (or if moving routers):
1. Use your smartphone to connect to the newly created WiFi network (`ProxiGate-Lock` or `ProxiGate-Cam`).
2. Navigate to `192.168.4.1` on your browser (if it doesn't automatically pop up).
3. Type in your household WiFi credentials.
4. Very importantly, under **Backend IP**, type the IPv4 address of the computer running your FastAPI backend (e.g., `192.168.0.112`).
5. **Save** and the modules will reboot and hook into your local platform!

### **5. Resetting WiFi Networks**
If your IP address changes or you bring the system somewhere else, you can quickly wipe the saved configurations:
- **For the Smart Lock**: Enter your Admin PIN + `#`, and press option `4` on the Keypad (`WIFI Wipe`). It will automatically erase its memory and trigger the setup portal.
- **For the ESP-CAM**: Take a generic jumper wire and connect **Pin 13 to GND** while powering the module on. It immediately flushes its internal memory and triggers the setup portal.

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
