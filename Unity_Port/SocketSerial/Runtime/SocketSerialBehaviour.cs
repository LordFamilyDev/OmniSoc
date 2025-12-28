using System.Collections.Generic;
using UnityEngine;

/// <summary>
/// MonoBehaviour wrapper for SocketSerial
/// Attach this to a GameObject to use SocketSerial in Unity
/// Handles cleanup on destroy automatically
/// </summary>
public class SocketSerialBehaviour : MonoBehaviour
{
    [Header("Connection Settings")]
    [Tooltip("IP address to connect to (client) or bind to (server)")]
    public string ipAddress = "127.0.0.1";
    
    [Tooltip("Port number")]
    public int port = 12345;
    
    [Tooltip("Server mode (listen for connections) or Client mode (connect to server)")]
    public bool isServer = false;
    
    [Tooltip("Auto-reconnect on disconnect")]
    public bool autoReconnect = true;
    
    [Tooltip("Update period in milliseconds")]
    public int updatePeriodMs = 10;
    
    [Header("Connection Control")]
    [Tooltip("Connect on Start()")]
    public bool connectOnStart = true;
    
    [Tooltip("Wait for connection before continuing Start()")]
    public bool blockUntilConnected = false;
    
    [Header("Debug Settings")]
    public bool suppressCatchPrints = true;
    public int missedHeartbeatLimit = 50;
    
    // Events
    public delegate void MessageReceivedHandler(string message);
    public event MessageReceivedHandler OnMessageReceived;
    
    public delegate void ConnectionStateChanged(bool connected);
    public event ConnectionStateChanged OnConnectionStateChanged;
    
    private SocketSerial socket;
    private bool wasConnected = false;
    
    void Start()
    {
        socket = new SocketSerial(ipAddress, port, isServer);
        socket.SuppressCatchPrints = suppressCatchPrints;
        socket.MissedHeartbeatLimit = missedHeartbeatLimit;
        
        if (connectOnStart)
        {
            Connect();
        }
    }
    
    void Update()
    {
        if (socket == null) return;
        
        // Check connection state changes
        bool currentlyConnected = socket.IsConnected();
        if (currentlyConnected != wasConnected)
        {
            wasConnected = currentlyConnected;
            OnConnectionStateChanged?.Invoke(currentlyConnected);
            
            if (currentlyConnected)
                Debug.Log("Socket connected!");
            else
                Debug.Log("Socket disconnected!");
        }
        
        // Process received messages
        List<string> messages = socket.Receive();
        foreach (string msg in messages)
        {
            OnMessageReceived?.Invoke(msg);
        }
    }
    
    void OnDestroy()
    {
        Disconnect();
    }
    
    void OnApplicationQuit()
    {
        Disconnect();
    }
    
    /// <summary>
    /// Connect to remote endpoint
    /// </summary>
    public void Connect()
    {
        if (socket != null)
        {
            socket.Connect(blockUntilConnected, autoReconnect, updatePeriodMs);
        }
    }
    
    /// <summary>
    /// Disconnect
    /// </summary>
    public void Disconnect()
    {
        if (socket != null)
        {
            socket.Disconnect();
            socket = null;
        }
    }
    
    /// <summary>
    /// Send a message
    /// </summary>
    public void Send(string message)
    {
        if (socket != null)
        {
            socket.Send(message);
        }
    }
    
    /// <summary>
    /// Check if connected
    /// </summary>
    public bool IsConnected()
    {
        return socket != null && socket.IsConnected();
    }
    
    /// <summary>
    /// Clear incoming buffer
    /// </summary>
    public void ClearInBuffer()
    {
        socket?.ClearInBuffer();
    }
    
    /// <summary>
    /// Clear outgoing buffer
    /// </summary>
    public void ClearOutBuffer()
    {
        socket?.ClearOutBuffer();
    }
}
