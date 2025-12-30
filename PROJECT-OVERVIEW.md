# Project Overview: MacroMan AI Aimbot v2

## 📝 What is this project?
The MacroMan AI Aimbot is a modern, high-performance computer vision system designed for educational purposes. It uses artificial intelligence (AI) to "see" targets in video games and provides smooth, human-like mouse assistance. 

Unlike traditional aimbots that read a game's internal memory (which is easily detected and often against terms of service), this system works purely by **analyzing the screen pixels**, just like a human player would.

---

## 🎯 Key Goals
1.  **Lightning Fast**: The system aims to capture, analyze, and react in less than **10 milliseconds** (1/100th of a second).
2.  **Human-Like**: Instead of "snapping" instantly to targets, it uses advanced math (Bezier curves) to mimic how a human hand moves, including natural jitter and slight overshooting.
3.  **Safe & Reliable**: It includes "Deadman Switches" and safety clamps to prevent erratic behavior if the AI stutters or the game lags.
4.  **Hardware Emulation**: It can send mouse movements through an external Arduino, making it appear to the computer as a real physical mouse.

---

## 🏗️ How it works (The 4-Thread Pipeline)
The system is split into four dedicated "workers" that run at the same time to ensure zero lag:

1.  **The Photographer (Capture)**: Grabs the game screen at 144+ frames per second and sends it directly to the graphics card (GPU).
2.  **The Brain (AI Detection)**: Uses a YOLO (You Only Look Once) AI model to identify players, heads, and bodies in the image.
3.  **The Tracker (Prediction)**: Follows targets across frames. If a player is running left, the tracker calculates where they *will be* by the time the mouse moves.
4.  **The Driver (Input)**: Calculates the smooth path for the mouse and sends the signal to move, updating 1,000 times per second.

---

## 🛠️ Visualizing the Experience
-   **Transparent Overlay**: A see-through window drawn over your game shows what the AI is seeing (bounding boxes around targets) and performance stats.
-   **External Control Center**: A separate app allows you to tune "Smoothness," "Field of View," and switch between different game profiles (e.g., Valorant vs. CS2) without restarting the system.

---

## ⚠️ Educational Disclaimer
This project is built for **research and learning**. It is a deep dive into real-time C++ programming, high-performance concurrency, and computer vision. It is **not** intended for use in competitive online matchmaking. Use it responsibly in private testing environments to understand how modern anti-cheat and AI-vision systems interact.
