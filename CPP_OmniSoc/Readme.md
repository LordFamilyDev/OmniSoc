# Usage
- build project with cmake (omnisoc compiles to a static lib)
- Chat client can be used to test the functionality of the lib after building
- include the protocol that you intend to use (they do not conflict)
- initialize the base class (use a shared pointer if you want it to be copyable)
- run connect
- interact with serial object through 3 primary functions:
  - isConnected()
  - receiveMessage()
  - sendMessage()
- omnisoc will handle socket connection and message buffering.
- omnisoc implementations should be able to handle fixed frequency and irregular messages concurrently.

# Additional notes
- This class is intended to be run in asyncronous mode, but is also fully functional in syncronous mode.
- syncronous mode is mostly useful for debugging without dealing with threads.
- I have not benchmarked this at all as far as processing efficiency or data throughput capabilities.

# TODO
- build out BLE and UART
- create benchmark/ stress testing app
- test cross system

# Building
- Use cmake to build
- Depends on Boost asio


# MISC

Last built with boost-1.84.0

Note: on windows, boost needs to be pulled from boost.org (release version), not from github
