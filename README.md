# IoT AQI Monitoring System

This repository contains the complete "Winning Stack" for a professional IoT Edge Node project.

## Architecture

1. **Edge Node (`esp32_aqi_node/`):** C++ ESP32 firmware running asynchronously without blocking `delay()`. It manages physical sensors (MQ135, MQ8, MQ9, Dust), local failsafe logic, LCD UI, and bidirectional WebSocket communication.
2. **Intelligence Layer (`backend/`):** FastAPI python server managing WebSocket connections. It simulates Scikit-Learn Machine Learning inference predicting hazard risks, and commands the Edge Node to adjust specific actuators (e.g. Exhaust Fan).
3. **Insight Layer (`frontend/`):** A modern Next.js + Tailwind CSS Dashboard built with React. It provides real-time data visualization and the "Winning Move" remote calibration trigger.

## Quick Start

### 1. Backend (FastAPI)
```bash
cd backend
python -m venv venv
source venv/bin/activate  # On Windows: venv\Scripts\activate
pip install -r requirements.txt
python main.py
# Server runs on http://localhost:8000
```

### 2. Frontend (Next.js Dashboard)
```bash
npx create-next-app@latest frontend --typescript --tailwind --eslint
cd frontend
# Install Shadcn UI and dependencies
npx shadcn-ui@latest init
npx shadcn-ui@latest add card button badge
npm i recharts lucide-react

# Replace the generatd app/page.tsx with the provided frontend/app/page.tsx
npm run dev
# Dashboard runs on http://localhost:3000
```

### 3. ESP32 Node
- Open `esp32_aqi_node.ino` in Arduino IDE or PlatformIO.
- Install the `ArduinoJson`, `WebSockets`, and `LiquidCrystal_I2C` libraries.
- Update `ssid` and `password` variables.
- Flash to your ESP32 Dev Module.

---
**Why this wins**: It demonstrates a true Closed-Loop Feedback System (Sense -> Think -> Act -> Validate) and tackles real-world hardware issues (sensor drift) using software solutions (remote calibration).
