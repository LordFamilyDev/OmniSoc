using System.Collections.Generic;
using UnityEngine;

/// <summary>
/// Example usage patterns for SocketSerial
/// </summary>
public class SocketSerialExamples : MonoBehaviour
{
    // ========== EXAMPLE 1: Using SocketSerialBehaviour Component ==========
    
    public SocketSerialBehaviour socketBehaviour;
    
    void Start()
    {
        // If using the MonoBehaviour wrapper, subscribe to events
        if (socketBehaviour != null)
        {
            socketBehaviour.OnMessageReceived += HandleMessage;
            socketBehaviour.OnConnectionStateChanged += HandleConnectionChange;
        }
    }
    
    void HandleMessage(string message)
    {
        Debug.Log($"Received: {message}");
        
        // Echo back
        socketBehaviour.Send($"Echo: {message}");
    }
    
    void HandleConnectionChange(bool connected)
    {
        if (connected)
        {
            Debug.Log("Connected! Sending greeting...");
            socketBehaviour.Send("Hello from Unity!");
        }
    }
    
    // ========== EXAMPLE 2: Direct Usage (Client Mode) ==========
    
    private SocketSerial clientSocket;
    
    void StartClient()
    {
        // Create client socket
        clientSocket = new SocketSerial("192.168.1.100", 12345, isServer: false, asyncronousFlag: true);
        
        // Connect (blocking until connected, with auto-reconnect)
        clientSocket.Connect(blockingFlag: true, autoReconnect: true, _periodMs: 10);
        
        // Send some data
        clientSocket.Send("Hello Server!");
        clientSocket.Send("Another message");
    }
    
    void UpdateClient()
    {
        if (clientSocket == null || !clientSocket.IsConnected())
            return;
        
        // Receive all messages
        List<string> messages = clientSocket.Receive(-1);
        foreach (string msg in messages)
        {
            Debug.Log($"Client received: {msg}");
        }
    }
    
    void CleanupClient()
    {
        clientSocket?.Disconnect();
    }
    
    // ========== EXAMPLE 3: Direct Usage (Server Mode) ==========
    
    private SocketSerial serverSocket;
    
    void StartServer()
    {
        // Create server socket (binds to all interfaces)
        serverSocket = new SocketSerial("0.0.0.0", 12345, isServer: true, asyncronousFlag: true);
        
        // Start listening (non-blocking, with auto-reconnect for new clients)
        serverSocket.Connect(blockingFlag: false, autoReconnect: true, _periodMs: 10);
        
        Debug.Log("Server started on port 12345");
    }
    
    void UpdateServer()
    {
        if (serverSocket == null)
            return;
        
        // Check connection status
        if (serverSocket.IsConnected())
        {
            // Receive messages
            List<string> messages = serverSocket.Receive();
            foreach (string msg in messages)
            {
                Debug.Log($"Server received: {msg}");
                
                // Echo back with timestamp
                serverSocket.Send($"[{Time.time}] {msg}");
            }
        }
    }
    
    void CleanupServer()
    {
        serverSocket?.Disconnect();
    }
    
    // ========== EXAMPLE 4: Synchronous Mode (Manual Updates) ==========
    
    private SocketSerial syncSocket;
    
    void StartSyncSocket()
    {
        // Create socket with async disabled
        syncSocket = new SocketSerial("127.0.0.1", 12345, isServer: false, asyncronousFlag: false);
    }
    
    void UpdateSyncSocket()
    {
        if (syncSocket == null)
            return;
        
        // Manually update connection and serial processing
        // Note: Don't call this too fast or heartbeat will fail
        syncSocket.SynchronousUpdate();
        
        // Send and receive as normal
        List<string> messages = syncSocket.Receive();
        foreach (string msg in messages)
        {
            Debug.Log($"Sync received: {msg}");
        }
    }
    
    // ========== EXAMPLE 5: Robot Telemetry Sender ==========
    
    private SocketSerial telemetrySocket;
    
    void StartTelemetry()
    {
        telemetrySocket = new SocketSerial("192.168.1.100", 5000, isServer: false, asyncronousFlag: true);
        telemetrySocket.Connect(blockingFlag: false, autoReconnect: true, _periodMs: 10);
    }
    
    void SendRobotTelemetry(Vector3 position, Quaternion rotation, float batteryLevel)
    {
        if (telemetrySocket == null || !telemetrySocket.IsConnected())
            return;
        
        // Send JSON telemetry
        string json = JsonUtility.ToJson(new RobotTelemetry
        {
            posX = position.x,
            posY = position.y,
            posZ = position.z,
            rotX = rotation.x,
            rotY = rotation.y,
            rotZ = rotation.z,
            rotW = rotation.w,
            battery = batteryLevel,
            timestamp = Time.time
        });
        
        telemetrySocket.Send(json);
    }
    
    // ========== EXAMPLE 6: Command Receiver ==========
    
    private SocketSerial commandSocket;
    
    void StartCommandReceiver()
    {
        commandSocket = new SocketSerial("0.0.0.0", 6000, isServer: true, asyncronousFlag: true);
        commandSocket.Connect(blockingFlag: false, autoReconnect: true, _periodMs: 10);
    }
    
    void ProcessCommands()
    {
        if (commandSocket == null || !commandSocket.IsConnected())
            return;
        
        List<string> commands = commandSocket.Receive();
        foreach (string cmd in commands)
        {
            ProcessCommand(cmd);
        }
    }
    
    void ProcessCommand(string command)
    {
        string[] parts = command.Split(',');
        
        if (parts.Length < 1)
            return;
        
        switch (parts[0])
        {
            case "MOVE":
                if (parts.Length >= 4)
                {
                    float x = float.Parse(parts[1]);
                    float y = float.Parse(parts[2]);
                    float z = float.Parse(parts[3]);
                    Debug.Log($"Moving to ({x}, {y}, {z})");
                    // Execute move command
                }
                break;
                
            case "ROTATE":
                if (parts.Length >= 4)
                {
                    float rx = float.Parse(parts[1]);
                    float ry = float.Parse(parts[2]);
                    float rz = float.Parse(parts[3]);
                    Debug.Log($"Rotating to ({rx}, {ry}, {rz})");
                    // Execute rotate command
                }
                break;
                
            case "GRIP":
                if (parts.Length >= 2)
                {
                    bool close = parts[1] == "1";
                    Debug.Log($"Gripper: {(close ? "Close" : "Open")}");
                    // Execute gripper command
                }
                break;
                
            default:
                Debug.LogWarning($"Unknown command: {parts[0]}");
                break;
        }
        
        // Send acknowledgment
        commandSocket.Send($"ACK,{parts[0]}");
    }
    
    // ========== EXAMPLE 7: Bidirectional Chat ==========
    
    private SocketSerial chatSocket;
    
    void StartChat(string remoteIP, int port, bool asServer)
    {
        chatSocket = new SocketSerial(remoteIP, port, asServer, asyncronousFlag: true);
        chatSocket.Connect(blockingFlag: false, autoReconnect: true, _periodMs: 10);
    }
    
    void UpdateChat()
    {
        if (chatSocket == null || !chatSocket.IsConnected())
            return;
        
        List<string> messages = chatSocket.Receive();
        foreach (string msg in messages)
        {
            Debug.Log($"Chat: {msg}");
        }
    }
    
    void SendChatMessage(string message)
    {
        chatSocket?.Send(message);
    }
}

// Helper class for telemetry example
[System.Serializable]
public class RobotTelemetry
{
    public float posX, posY, posZ;
    public float rotX, rotY, rotZ, rotW;
    public float battery;
    public float timestamp;
}
