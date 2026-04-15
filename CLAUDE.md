# OmniSoc — Claude Context

## Permissions

Claude has full read and write access to all files and directories within this repository. No confirmation is needed before reading, editing, creating, or deleting files here. For anything outside 
this directory, always ask before proceeding.

## What It Is

OmniSoc is a cross-platform, cross-language serial communication library targeting robotics and embedded systems. It provides a standardized, plug-and-play interface for reliable inter-device communication over UART/serial and TCP/IP sockets, with the same core API available across C++, Python, Arduino (C), Unity (C#), and ROS2.

**License:** MIT (Copyright 2024 LordFamilyDev)

---

## Current Capabilities

**Transport Protocols:**
- UART/Serial — hardware serial with frame sync and checksum (all platforms)
- TCP/IP Sockets — client and server modes (C++, Python, Unity)
- BLE — stubbed, not yet implemented

**Core Features:**
- Consistent API: `isConnected()`, `sendMessage()`, `receiveMessage()`
- Async (threaded) and sync (polling) operation modes
- Auto-reconnect with configurable heartbeat/timeout detection
- Thread-safe message queuing (mutex-backed buffers)
- Binary message framing: 2-byte header + float count + float payload (up to 10 floats) + XOR checksum
- Socket heartbeat: empty delimiter (`;`) sent periodically to detect stale connections

**Supported Platforms:** Linux, Windows, macOS, Arduino, Raspberry Pi, Android, iOS, Unity

---

## State of Development

**Alpha/Beta — actively developed, not yet production-stable.**

- C++ UART and Socket implementations are the most mature (tested stable)
- Unity (C#) port was added recently and is still early
- ROS2 integration is a working demo, not a polished package
- BLE is a stub — no implementation exists yet
- No benchmarking or stress testing has been done
- Cross-platform testing is incomplete

**Known TODOs / Limitations:**
- BLE implementation is missing entirely
- Arduino limited to 10 floats/message due to 64-byte hardware buffer
- No on-demand heartbeat (connection health only detectable when messaging regularly)
- No protocol buffers or MessagePack support (noted as future enhancement)
- Baud rate must be manually calculated to avoid overrun

---

## General Architecture

All language implementations share the same conceptual layering:

```
Application
    │
    ▼
OmniSoc API  ──  isConnected() / sendMessage() / receiveMessage()
    │
    ▼
Backend (transport-specific)
├── UART_Serial  (Boost ASIO on C++; HardwareSerial on Arduino)
├── Socket_Serial (Boost ASIO / .NET sockets / Python sockets)
└── BLE_Serial   (stub)
    │
    ▼
OS / Hardware
```

**Threading (C++ async mode):**
- `connectionThread` — manages lifecycle, reconnection, heartbeat
- `serialThread` — blocking read/write loop
- Main thread — non-blocking, enqueues/dequeues messages

**Sync mode** (Arduino, or debugging): caller drives updates via `synchronousUpdate()` / `handleSynchronization()` in the main loop.

**UART Message Format:**
```
[Header 0xAB 0xCD (2B)] [Float Count (1B)] [Float Data (N×4B)] [XOR Checksum (1B)]
Max payload: 10 floats = 43 bytes/message
```

**Socket Message Format:**
```
[Arbitrary String][Delimiter ';']
Heartbeat = bare ';' with no preceding data
```

---

## Folder Structure

```
OmniSoc/
├── CLAUDE.md                        # This file
├── README.md                        # Project overview
├── LICENSE

├── CPP_OmniSoc/                     # Core C++ library (most mature)
│   ├── CMakeLists.txt               # CMake build (requires Boost ASIO, C++14)
│   ├── Config.cmake.in              # CMake package config for downstream use
│   ├── Readme.md                    # Build and usage instructions
│   ├── include/
│   │   ├── UART_Serial.h            # UART class interface
│   │   ├── Socket_Serial.h          # TCP socket class interface (client + server)
│   │   └── BLE_Serial.h             # BLE stub (empty)
│   └── src/
│       ├── UART_Serial.cpp          # UART implementation
│       ├── Socket_Serial.cpp        # Socket implementation
│       ├── BLE_Serial.cpp           # BLE stub
│       ├── ChatClient.cpp           # Interactive TCP demo app
│       └── UART_Serial_Tester.cpp   # UART unit test utility

├── Arduino_UART/                    # Arduino C implementation
│   ├── README.md
│   ├── Arduino_UART.ino             # Example sketch
│   ├── SerialManager.h
│   └── SerialManager.cpp

├── Python_UART/                     # Python implementation
│   └── omnisoc_serial.py            # Pure Python; only external dep is PySerial

├── ros2Soc/                         # ROS2 integration (demo-quality)
│   ├── README.md
│   └── src/
│       ├── message_definitions/msg/ChatMessage.msg
│       └── chat_client_py/          # ROS2 Python chat demo node

└── Unity_Port/                      # Unity C# implementation (recently added)
    └── SocketSerial/
        ├── package.json             # Unity package manifest
        ├── README.md
        ├── PACKAGE_STRUCTURE.md
        └── Runtime/
            ├── SocketSerial.cs          # Core C# socket class
            ├── SocketSerialBehaviour.cs # MonoBehaviour wrapper
            └── SocketSerial.Runtime.asmdef
```

---

## Build Notes

**C++ (CPP_OmniSoc):**
- Requires Boost 1.84.0+ (from boost.org, not GitHub on Windows)
- CMake 3.10+, C++14 standard
- Standard `cmake .. && make && sudo make install` workflow

**Python:** No build step; install PySerial only.

**Arduino:** No external libraries; uses native `HardwareSerial`.

**ROS2:** `colcon build --symlink-install`; requires ROS 2 Iron.

**Unity:** .NET 4.x or .NET Standard 2.0; import as a Unity package.
