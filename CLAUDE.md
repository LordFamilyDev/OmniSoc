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
- UART binary framing v3: 2-byte sync (0xA5 0x5A) + 1-byte header + 1-byte length + opaque byte payload (up to 48 B) + CRC-16/CCITT-FALSE
- `PackBytes.h` helpers (C++/Arduino) and `struct.pack/unpack` (Python) for typed field composition inside the byte payload
- Socket heartbeat: empty delimiter (`;`) sent periodically to detect stale connections

**Supported Platforms:** Linux, Windows, macOS, Arduino, Raspberry Pi, Android, iOS, Unity

---

## State of Development

**Alpha/Beta — actively developed, not yet production-stable.**

- C++ UART and Socket implementations are the most mature; UART has been debugged and tested on Linux against Arduino UNO R4
- Unity (C#) port was added recently and is still early
- ROS2 integration is a working demo, not a polished package
- BLE is a stub — no implementation exists yet
- No benchmarking or stress testing has been done
- Cross-platform testing is incomplete

**Known TODOs / Limitations:**
- BLE implementation is missing entirely
- UART payload capped at 48 bytes (`MAX_PAYLOAD`); chosen so a max-size frame (54 B) fits comfortably under the AVR 64 B HW UART RX buffer
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
- `read_thread_` — bounded read loop (kernel-side VTIME=100ms timeout via termios) that drains the serial port into a mutex-guarded internal buffer
- Main thread / caller — calls `receiveMessage()` / `sendMessage()` directly. `receiveMessage()` does sync-scan + CRC-validate parsing on demand.

**Arduino mode**: single-threaded. Caller MUST invoke `receiveMessage()` every main loop iteration — it's the only thing draining the HardwareSerial RX buffer. Skipping calls > ~10 ms (at 57600 baud) overflows the 64 B HW buffer.

**UART Message Format (v3):**
```
[Sync 0xA5 0x5A] [Header (1B)] [Length (1B)] [Payload bytes (N)] [CRC-16 (2B, little-endian)]
Total: 6 + N bytes. Max payload N = 48 → max frame 54 B.
CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF) covers [Header][Length][Payload] only — sync excluded.
Payload is opaque bytes; use PackBytes.h (C++/Arduino) or struct.pack (Python) for typed fields.
Float-array convenience overloads of sendMessage/receiveMessage exist for backward-shaped code.
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
│   │   ├── PackBytes.h              # Typed pack/unpack helpers for byte payloads
│   │   ├── Socket_Serial.h          # TCP socket class interface (client + server)
│   │   └── BLE_Serial.h             # BLE stub (empty)
│   └── src/
│       ├── UART_Serial.cpp          # UART implementation
│       ├── Socket_Serial.cpp        # Socket implementation
│       ├── BLE_Serial.cpp           # BLE stub
│       ├── ChatClient.cpp           # Interactive TCP demo app
│       └── UART_Serial_Tester.cpp   # Interactive UART tester: lists available ports, accepts header+floats input, displays latest received message

├── Arduino_UART/                    # Arduino C implementation
│   ├── README.md
│   ├── Arduino_UART.ino             # Example sketch
│   ├── SerialManager.h
│   ├── SerialManager.cpp
│   └── PackBytes.h                  # Mirror of CPP_OmniSoc/include/PackBytes.h

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

## Hardware Notes

**Arduino UNO R4 (Renesas core)**
- `Serial.availableForWrite()` always returns 0 — the Renesas core does not buffer outgoing data; writes are blocking per-character. Do not use `availableForWrite()` as a TX gate; call `write()` directly. This is a known limitation of ArduinoCore-renesas.
- On Linux, opening the serial port toggles DTR which resets the R4. Allow ~2 seconds for the board to reboot before expecting communication.

**Linux serial ports (Boost ASIO)**
- Boost ASIO opens serial ports with `O_NONBLOCK` but does not clear `ICANON` (canonical/line mode). In canonical mode the kernel buffers binary data until a newline arrives, so no bytes are delivered to `read()`. If adding new platforms or seeing `buf:0` in diagnostics, ensure the port is configured for raw mode.
- Graceful shutdown is bounded by termios `VMIN=0, VTIME=1` (100 ms) configured after `cfmakeraw()` in `connect()`. This makes blocking `read_some()` calls return on idle so `disconnect()` → `join()` completes within ~100 ms instead of deadlocking. **Do not switch to user-space `poll()`** — a previous attempt (commits dddd198 + 8535766, reverted in 17d2499) caused mysterious gimbal-shake regression in PT_Forwarder; root cause never identified. Keeping the timeout in the kernel avoids changing user-space read/write interleaving.
- **(Open: needs investigation)** Empirically, the VTIME bound does NOT prevent shutdown deadlock when the FTDI is connected but the downstream Arduino is powered off (verified 2026-05-04: SC_Forwarder SIGTERM took 263 s against `/dev/ttyUSB0` with Mega off). On a normally-responding device, shutdown is <300 ms. On a silent pty test rig, shutdown only completes when the master closes (so pty is not a faithful test of VTIME). Hypotheses to chase: (a) the FTDI driver may not honor VTIME the same way under "DTR asserted but device not transmitting", (b) `tcsetattr` may be racing or being reset by a later ASIO call, (c) the driver may distinguish "hardware silent" from "no data this poll period". Workaround for now: rely on systemd Restart=on-failure with TimeoutStopSec to bound real-world recovery.

**TX self-pacing (`tx_pacing_enabled`)**
- `UART_Serial::sendMessage()` (and `SerialManager.send_message()` in Python) self-paces consecutive sends. After dispatching a frame of N bytes, the next send blocks until `N * (10/baud)` seconds have elapsed. Caps the on-wire byte rate at the baud rate. Default ON; opt out via the constructor's 4th param (C++) or `tx_pacing_enabled` kwarg (Python).
- Note: this is partly redundant with the UART hardware itself (which always shifts bits at the configured baud rate). Pacing's real value is **deterministic send-completion semantics** — when `sendMessage` returns, the bytes have actually left the wire. Useful for crash-safety, sequencing logic, and back-pressure visibility. NOT a substitute for receivers calling `receiveMessage()` every loop iteration; that's still the only thing that prevents HW UART buffer overflow during loop stalls.

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
