# Arduino Uart Omnisoc

# Usage
- Either copy files into local directory, or add library directory for import
- Initialize the SerialManager object with a hardware or software serial port and connect with the specified baud rate.
- This library does not check if you are overloading the entered baud rate.
- Rough baud rate calculation: (10 * (4 + 4 * (floatsPerMessage) ) * (messagesPerSecond) * 2
  - each byte is 10 bits
  - message header (with checksum) costs 4 bytes
  - each float is an additional 4 bytes
  - leave at least 10% baud rate overhead to allow syncronization (double target baud rate if non-regular messaging is also being used)
  - Example: 3 float standard message at 10ms interval (100hz): 160 bits per message, 16000 bits/sec, min baud 19200, 38400 to support non-regular messaging
- If only sending messages, simply make calls to sendMessage.
- If sending and receiving messages, call handleSynchronization() followed by receiveMessage() at the top of every loop.

# Notes
- Due to memory limits on arduino, a software buffer was not implemented and therefore receiveMessage should either be called every loop, or else called multiple times until all messages have been read.
- No heartbeat has been implemented at this time, but it could be implemented if needed. (connection monitoring is only available when sending and receiving messages regularly)
- Max message size is hardcoded to 10 floats (could be 14 floats if needed, but is capped by arduino hardware serial buffers of 64 bytes)

# TODO
- Implement on demand heartbeat (only sent if no other message sent for some period)
- Implement baud rate to datathroughput monitoring (possibly with internal flag set if flooding)
