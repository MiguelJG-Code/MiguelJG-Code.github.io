Cerberus — Multi-Mode Battle Bot
An ESP32-powered battle robot with four autonomous and remote-control modes, live Wi-Fi sensor telemetry, and over-the-air firmware updates — no USB cable needed after the first flash.

What it does
Cerberus runs four competition modes selectable by physical DIP switches. In Sumo and Maze modes it operates fully autonomously using ultrasonic and IR sensors. In Hockey mode it's driven over Bluetooth from a phone or controller. A custom Java desktop app connects over Wi-Fi to flash new firmware and watch live sensor data while the bot runs.

Built with

C++ / Arduino (ESP32 firmware)
Java Swing (desktop GUI — zero external libraries)
ESP32 Arduino Core v3.3.8
HTTP + raw TCP (OTA upload and telemetry streaming)
Classic Bluetooth Serial (Hockey remote control)
esptool.py (initial USB flash)


Features

Sumo mode — dual IR sensors detect the opponent at close range; three ultrasonic sensors (front, left, right) extend detection; spins to search when nothing is found
Hockey mode — Bluetooth remote control with F/B/L/R/S commands; Wi-Fi is fully disabled during this mode to eliminate interference
Maze Solver — left-hand rule navigation with a median filter on ultrasonic readings to reduce noise; handles dead-ends with a U-turn
Line Follower — pivot steering (one wheel at a time) for tight turns; remembers which side the line was last seen on to recover when it's lost
OTA firmware updates — HTTP server on port 80 accepts .bin uploads; Java GUI handles file selection, ping check, chunked upload, and live progress bar
Live telemetry — TCP server on port 81 streams JSON sensor packets to up to 4 clients at 300 ms intervals; GUI displays color-coded distance bars and IR indicator lights
Dynamic radio switching — Wi-Fi starts automatically for autonomous modes; Bluetooth starts for Hockey; neither runs at the same time


What I learned

How to run an HTTP server and a raw TCP server on the same ESP32 simultaneously
Implementing OTA firmware updates over Wi-Fi without any external OTA library
Building a multi-threaded Java Swing app where network I/O runs on daemon threads and all UI updates go through SwingUtilities.invokeLater
Writing a lightweight JSON parser in plain Java with no dependencies
Tuning a median-filter ultrasonic sensor reading loop to avoid phantom echoes during motor vibration
Managing two radios (Wi-Fi and Bluetooth) on ESP32 and switching between them cleanly at runtime
