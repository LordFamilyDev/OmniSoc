#Usage
-build project (omnisoc compiles to a static lib)
-include the protocol that you intend to use (they do not conflict)
-initialize the base class (use a shared pointer if you want it to be copyable)
-run connect
-interact with serial object through 3 primary functions:
--isConnected()
--receive()
--send()
-omnisoc will handle socket connection and message buffering.

#Additional notes
-This class is intended to be run in asyncronous mode, but is also fully functional in syncronous mode.
-syncronous mode is mostly useful for debugging without dealing with threads.
-I have not benchmarked this at all as far as processing efficiency or data throughput capabilities.

#TODO
-build out BLE and UART
-create benchmark/ stress testing app
-test cross system


#MISC

Last built with boost-1.84.0

Note: on windows, boost needs to be pulled from boost.org (release version), not from github