# SocketSerial for Unity

C# port of the C++ Socket_Serial library for Unity. Provides TCP/IP socket communication with both client and server modes, asynchronous or synchronous operation.

## Features

- **Client and Server modes** - Connect to a remote server or listen for incoming connections
- **Asynchronous threading** - Non-blocking operation with automatic connection management
- **Synchronous mode** - Manual update mode for more control
- **Auto-reconnect** - Automatically reconnect on disconnect
- **Heartbeat detection** - Detect and handle connection drops
- **Thread-safe buffers** - Safe message queuing for send/receive
- **Message delimiting** - Automatic message framing with delimiters

## Installation

### Option 1: Copy Scripts (Simple)

1. Copy these files into your Unity project's `Assets/Scripts/` folder:
   - `SocketSerial.cs`
   - `SocketSerialBehaviour.cs` (optional MonoBehaviour wrapper)
   - `SocketSerialExamples.cs` (optional examples)

### Option 2: Unity Package (Reusable)

Create a Unity Package for easy reuse across projects:

1. Create this folder structure:
```
Assets/
└── SocketSerial/
    ├── Runtime/
    │   ├── SocketSerial.cs
    │   ├── SocketSerialBehaviour.cs
    │   └── SocketSerial.Runtime.asmdef
    ├── Samples/
    │   └── SocketSerialExamples.cs
    └── package.json
```

2. Create `package.json`:
```json
{
  "name": "com.halfstack.socketseral",
  "version": "1.0.0",
  "displayName": "Socket Serial",
  "description": "TCP/IP socket communication for Unity",
  "unity": "2019.4",
  "keywords": ["networking", "tcp", "socket"],
  "author": {
    "name": "Half Stack Solutions",
    "email": "your@email.com"
  }
}
```

3. Create `SocketSerial.Runtime.asmdef`:
```json
{
    "name": "SocketSerial.Runtime",
    "references": [],
    "includePlatforms": [],
    "excludePlatforms": [],
    "allowUnsafeCode": false,
    "overrideReferences": false,
    "precompiledReferences": [],
    "autoReferenced": true,
    "defineConstraints": [],
    "versionDefines": [],
    "noEngineReferences": false
}
```

4. To use in other projects:
   - Copy the `SocketSerial` folder to the new project's `Assets/` folder
   - Or add to `Packages/` folder for local package
   - Or create a git repository and add via Package Manager

### Option 3: Local Package (Advanced)

1. Create the package outside your Assets folder:
```
MyUnityPackages/
└── SocketSerial/
    ├── Runtime/
    │   ├── SocketSerial.cs
    │   ├── SocketSerialBehaviour.cs
    │   └── SocketSerial.Runtime.asmdef
    ├── Samples~/
    │   └── SocketSerialExamples.cs
    └── package.json
```

2. In Unity, add to `Packages/manifest.json`:
```json
{
  "dependencies": {
    "com.halfstack.socketseral": "file:../../MyUnityPackages/SocketSerial"
  }
}
```

## Quick Start

### Using MonoBehaviour Wrapper (Easiest)

1. Create an empty GameObject in your scene
2. Add the `SocketSerialBehaviour` component
3. Configure in Inspector:
   - IP Address: `127.0.0.1` (for localhost)
   - Port: `12345`
   - Is Server: `false` (client mode)
   - Connect On Start: `true`

4. Create a script to use it:

```csharp
public class MyScript : MonoBehaviour
{
    public SocketSerialBehaviour socket;
    
    void Start()
    {
        socket.OnMessageReceived += HandleMessage;
    }
    
    void HandleMessage(string msg)
    {
        Debug.Log($"Received: {msg}");
    }
    
    void Update()
    {
        if (Input.GetKeyDown(KeyCode.Space))
        {
            socket.Send("Hello!");
        }
    }
}
```

### Using Direct API

```csharp
public class DirectExample : MonoBehaviour
{
    private SocketSerial socket;
    
    void Start()
    {
        // Create socket (client mode, async)
        socket = new SocketSerial("192.168.1.100", 12345, isServer: false);
        
        // Connect with auto-reconnect
        socket.Connect(blockingFlag: false, autoReconnect: true, _periodMs: 10);
    }
    
    void Update()
    {
        if (socket == null || !socket.IsConnected())
            return;
        
        // Receive messages
        List<string> messages = socket.Receive();
        foreach (string msg in messages)
        {
            Debug.Log($"Received: {msg}");
        }
        
        // Send message
        if (Input.GetKeyDown(KeyCode.Space))
        {
            socket.Send("Hello from Unity!");
        }
    }
    
    void OnDestroy()
    {
        socket?.Disconnect();
    }
}
```

## API Reference

### Constructor

```csharp
SocketSerial(string ipAddress, int port, bool isServer, bool asyncronousFlag = true)
```

- `ipAddress`: IP to connect to (client) or bind to (server, use "0.0.0.0" for all interfaces)
- `port`: Port number
- `isServer`: `true` for server mode, `false` for client mode
- `asyncronousFlag`: `true` for threaded operation, `false` for manual updates

### Methods

#### `void Connect(bool blockingFlag, bool autoReconnect, int periodMs)`
Start connection (async mode only)
- `blockingFlag`: Wait until connected before returning
- `autoReconnect`: Automatically reconnect on disconnect
- `periodMs`: Update period in milliseconds (default: 10)

#### `void Disconnect()`
Close connection and cleanup

#### `void Send(string message)`
Send a message (delimiter added automatically)

#### `List<string> Receive(int count = -1)`
Receive messages from buffer
- `count`: Number of messages to get, -1 for all

#### `bool IsConnected()`
Check if currently connected

#### `void ClearInBuffer()`
Clear incoming message buffer

#### `void ClearOutBuffer()`
Clear outgoing message buffer

#### `void SynchronousUpdate()`
Manual update for sync mode (call in Update loop)

### Properties

#### `bool SuppressCatchPrints`
Suppress error messages (default: true)

#### `int MissedHeartbeatLimit`
Number of missed heartbeats before disconnect (default: 50)

## Usage Patterns

### Client Mode
```csharp
socket = new SocketSerial("192.168.1.100", 12345, isServer: false);
socket.Connect(blockingFlag: true, autoReconnect: true, _periodMs: 10);
```

### Server Mode
```csharp
socket = new SocketSerial("0.0.0.0", 12345, isServer: true);
socket.Connect(blockingFlag: false, autoReconnect: true, _periodMs: 10);
```

### Synchronous Mode
```csharp
socket = new SocketSerial("127.0.0.1", 12345, isServer: false, asyncronousFlag: false);

void Update()
{
    socket.SynchronousUpdate();  // Call regularly, but not too fast
    // ... send/receive
}
```

### Robot Telemetry
```csharp
void SendTelemetry(Vector3 pos, Quaternion rot, float battery)
{
    string data = $"POS,{pos.x},{pos.y},{pos.z}";
    socket.Send(data);
    
    data = $"ROT,{rot.eulerAngles.x},{rot.eulerAngles.y},{rot.eulerAngles.z}";
    socket.Send(data);
    
    data = $"BAT,{battery}";
    socket.Send(data);
}
```

### Command Processing
```csharp
void Update()
{
    List<string> commands = socket.Receive();
    foreach (string cmd in commands)
    {
        string[] parts = cmd.Split(',');
        switch (parts[0])
        {
            case "MOVE":
                MoveRobot(float.Parse(parts[1]), float.Parse(parts[2]), float.Parse(parts[3]));
                break;
            case "STOP":
                StopRobot();
                break;
        }
    }
}
```

## Important Notes

### Threading
- Async mode uses background threads - messages are queued for thread-safe access
- All `Send()` and `Receive()` calls are thread-safe
- Don't access Unity API directly from socket events/callbacks (use queuing pattern)

### Message Delimiter
- Default delimiter is `;`
- Messages are automatically split by delimiter
- Don't include delimiter in your message content
- Heartbeats are sent as empty delimiters to detect disconnects

### Synchronous Mode
- Requires manual calls to `SynchronousUpdate()`
- Don't call too frequently or heartbeat detection may fail
- Recommended for debugging or when you need precise control

### Unity Lifecycle
- Always call `Disconnect()` in `OnDestroy()` or `OnApplicationQuit()`
- Use `OnApplicationQuit()` for persistent objects
- MonoBehaviour wrapper handles this automatically

### Platform Compatibility
- Works on Windows, Mac, Linux, Android, iOS
- Requires .NET 4.x or .NET Standard 2.0 in Unity player settings
- May require firewall configuration on some platforms

## Troubleshooting

### Connection fails
- Check firewall settings
- Verify IP address and port
- Ensure server is running before client connects
- Check server binding address (use "0.0.0.0" for all interfaces)

### Heartbeat disconnects
- Increase `MissedHeartbeatLimit`
- Check network stability
- Verify update period isn't too slow (should be 10-50ms)
- In sync mode, ensure `SynchronousUpdate()` is called regularly

### Thread exceptions
- Set `SuppressCatchPrints = false` to see error messages
- Check for proper cleanup in `OnDestroy()`
- Verify you're not accessing Unity API from socket threads

## Examples

See `SocketSerialExamples.cs` for complete working examples including:
- MonoBehaviour wrapper usage
- Client and server modes
- Synchronous operation
- Robot telemetry sender
- Command receiver
- Bidirectional chat

## License

Same as original C++ Socket_Serial library

## Credits

C# Unity port of C++ Socket_Serial library by Half Stack Solutions
