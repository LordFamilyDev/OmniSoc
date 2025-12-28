using System;
using System.Collections.Generic;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Threading;
using UnityEngine;

/// <summary>
/// Unity C# port of Socket_Serial for TCP/IP communication
/// Supports both client and server modes with async or sync operation
/// </summary>
public class SocketSerial
{
    private TcpClient client;
    private TcpListener server;
    private NetworkStream stream;
    
    private Thread connectionThread;
    private Thread serialThread;
    
    private readonly object inBufferLock = new object();
    private readonly object outBufferLock = new object();
    
    private List<string> incomingBuffer = new List<string>();
    private List<string> outgoingBuffer = new List<string>();
    
    private bool asyncronousFlag = false;
    private bool autoReconnect = false;
    private int missedHeartbeats = 0;
    private bool isServer = false;
    private bool connectedFlag = false;
    private bool killFlag = false;
    private string msgDelimiter = ";";
    private string inMessageRemainder = "";
    
    private string ipAddress;
    private int port;
    private int periodMs;
    
    // Public settings
    public bool SuppressCatchPrints { get; set; } = true;
    public int MissedHeartbeatLimit { get; set; } = 50;
    
    /// <summary>
    /// Create a new SocketSerial instance
    /// </summary>
    /// <param name="_ipAddress">IP address to connect to (client) or bind to (server)</param>
    /// <param name="_port">Port number</param>
    /// <param name="_isServer">True for server mode, false for client mode</param>
    /// <param name="_asyncronousFlag">True for async operation with threads, false for manual sync updates</param>
    public SocketSerial(string _ipAddress, int _port, bool _isServer, bool _asyncronousFlag = true)
    {
        asyncronousFlag = _asyncronousFlag;
        ipAddress = _ipAddress;
        port = _port;
        isServer = _isServer;
    }
    
    /// <summary>
    /// Connect to remote endpoint (async mode only)
    /// </summary>
    /// <param name="blockingFlag">If true, blocks until connected</param>
    /// <param name="autoReconnect">If true, automatically reconnect on disconnect</param>
    /// <param name="_periodMs">Update period in milliseconds for serial thread</param>
    public void Connect(bool blockingFlag = false, bool autoReconnect = false, int _periodMs = 10)
    {
        if (!asyncronousFlag) return;
        
        periodMs = _periodMs;
        this.autoReconnect = autoReconnect;
        
        connectionThread = new Thread(ConnectionThread);
        connectionThread.IsBackground = true;
        connectionThread.Start();
        
        if (blockingFlag)
        {
            while (!connectedFlag && !killFlag)
            {
                Thread.Sleep(50);
            }
        }
    }
    
    /// <summary>
    /// Disconnect and cleanup
    /// </summary>
    public void Disconnect()
    {
        try
        {
            if (server != null)
            {
                server.Stop();
            }
        }
        catch { }
        
        try
        {
            Debug.Log("Connection Closed");
            autoReconnect = false;
            killFlag = true;
            
            if (connectionThread != null && connectionThread.IsAlive)
            {
                connectionThread.Join(1000);
                if (connectionThread.IsAlive)
                    connectionThread.Abort();
            }
            
            if (serialThread != null && serialThread.IsAlive)
            {
                serialThread.Join(1000);
                if (serialThread.IsAlive)
                    serialThread.Abort();
            }
            
            CloseSocket();
        }
        catch (Exception e)
        {
            if (!SuppressCatchPrints)
                Debug.LogError($"Exception caught: {e.Message}");
        }
    }
    
    /// <summary>
    /// Send a message
    /// </summary>
    /// <param name="msg">Message to send (delimiter will be added automatically)</param>
    public void Send(string msg)
    {
        lock (outBufferLock)
        {
            outgoingBuffer.Add(msg);
        }
    }
    
    /// <summary>
    /// Receive messages from buffer
    /// </summary>
    /// <param name="count">Number of messages to retrieve, -1 for all</param>
    /// <returns>List of received messages</returns>
    public List<string> Receive(int count = -1)
    {
        List<string> messages = new List<string>();
        
        lock (inBufferLock)
        {
            if (count == -1)
            {
                messages.AddRange(incomingBuffer);
                incomingBuffer.Clear();
            }
            else
            {
                int numMessages = Mathf.Min(count, incomingBuffer.Count);
                messages.AddRange(incomingBuffer.GetRange(0, numMessages));
                incomingBuffer.RemoveRange(0, numMessages);
            }
        }
        
        return messages;
    }
    
    /// <summary>
    /// Check if currently connected
    /// </summary>
    public bool IsConnected()
    {
        return connectedFlag;
    }
    
    /// <summary>
    /// Clear incoming message buffer
    /// </summary>
    public void ClearInBuffer()
    {
        lock (inBufferLock)
        {
            incomingBuffer.Clear();
        }
    }
    
    /// <summary>
    /// Clear outgoing message buffer
    /// </summary>
    public void ClearOutBuffer()
    {
        lock (outBufferLock)
        {
            outgoingBuffer.Clear();
        }
    }
    
    /// <summary>
    /// Flush socket (no-op for TCP/IP)
    /// </summary>
    public void FlushSocket()
    {
        // No need to flush for TCP/IP sockets
    }
    
    /// <summary>
    /// Manual synchronous update for non-async mode
    /// Call this regularly in your Update() loop if asyncronousFlag is false
    /// If called too fast without delay, may crash due to heartbeat would_block errors
    /// </summary>
    public void SynchronousUpdate()
    {
        if (asyncronousFlag) return;
        
        DoConnection();
        DoSerial();
    }
    
    // Private methods
    
    private void ConnectionThread()
    {
        if (!asyncronousFlag) return;
        
        bool connectionOneShot = true;
        
        while (!killFlag && (autoReconnect || connectionOneShot))
        {
            if (!connectedFlag)
            {
                if (serialThread != null && serialThread.IsAlive)
                {
                    serialThread.Join(1000);
                    if (serialThread.IsAlive)
                        serialThread.Abort();
                }
                
                DoConnection();
                
                if (stream != null)
                {
                    serialThread = new Thread(SerialThread);
                    serialThread.IsBackground = true;
                    serialThread.Start();
                    connectionOneShot = false;
                }
            }
            
            Thread.Sleep(1000);
        }
    }
    
    private void DoConnection()
    {
        try
        {
            if (!connectedFlag)
            {
                if (isServer)
                {
                    if (server == null)
                    {
                        IPAddress ipAddr = IPAddress.Parse(ipAddress);
                        server = new TcpListener(ipAddr, port);
                        server.Start();
                        Debug.Log($"Server listening on {ipAddress}:{port}");
                    }
                    
                    // Non-blocking check for pending connections
                    if (server.Pending())
                    {
                        client = server.AcceptTcpClient();
                        stream = client.GetStream();
                        client.NoDelay = true;
                        stream.ReadTimeout = 100;
                        stream.WriteTimeout = 100;
                    }
                }
                else
                {
                    client = new TcpClient();
                    client.Connect(ipAddress, port);
                    stream = client.GetStream();
                    client.NoDelay = true;
                    stream.ReadTimeout = 100;
                    stream.WriteTimeout = 100;
                }
                
                if (stream != null)
                {
                    Debug.Log("Socket connected");
                    missedHeartbeats = -5; // Grace period for initial connection
                    connectedFlag = true;
                }
            }
        }
        catch (Exception e)
        {
            if (!SuppressCatchPrints)
                Debug.Log($"Connection Exception: {e.Message}");
        }
    }
    
    private void CloseSocket()
    {
        if (connectedFlag)
        {
            Debug.Log("Connection Dropped");
        }
        
        try
        {
            if (stream != null)
            {
                stream.Close();
                stream = null;
            }
            
            if (client != null)
            {
                client.Close();
                client = null;
            }
        }
        catch (Exception e)
        {
            if (!SuppressCatchPrints)
                Debug.LogError($"Failed to close socket: {e.Message}");
        }
        
        connectedFlag = false;
    }
    
    private void SerialThread()
    {
        if (!asyncronousFlag) return;
        
        while (!killFlag && connectedFlag)
        {
            DoSerial();
            Thread.Sleep(periodMs);
        }
    }
    
    private void DoSerial()
    {
        SendMessages();
        ReadMessages();
    }
    
    private void SendMessages()
    {
        if (!connectedFlag || stream == null) return;
        
        lock (outBufferLock)
        {
            try
            {
                if (outgoingBuffer.Count > 0)
                {
                    foreach (string msg in outgoingBuffer)
                    {
                        byte[] data = Encoding.UTF8.GetBytes(msg);
                        stream.Write(data, 0, data.Length);
                        
                        byte[] delimiter = Encoding.UTF8.GetBytes(msgDelimiter);
                        stream.Write(delimiter, 0, delimiter.Length);
                    }
                    outgoingBuffer.Clear();
                }
                else
                {
                    // Send heartbeat
                    byte[] delimiter = Encoding.UTF8.GetBytes(msgDelimiter);
                    stream.Write(delimiter, 0, delimiter.Length);
                }
            }
            catch (Exception e)
            {
                if (!SuppressCatchPrints)
                    Debug.LogError($"Write error: {e.Message}");
                CloseSocket();
            }
        }
    }
    
    private void ReadMessages()
    {
        if (!connectedFlag || stream == null) return;
        
        try
        {
            if (stream.DataAvailable)
            {
                byte[] buffer = new byte[1024];
                int bytesRead = stream.Read(buffer, 0, buffer.Length);
                
                if (bytesRead > 0)
                {
                    string message = Encoding.UTF8.GetString(buffer, 0, bytesRead);
                    string tempRemainder = "";
                    List<string> msgs = SplitMessage(message, msgDelimiter, out tempRemainder);
                    
                    for (int i = 0; i < msgs.Count; i++)
                    {
                        string isolatedMessage = "";
                        if (i == 0 && inMessageRemainder.Length > 0)
                        {
                            isolatedMessage = inMessageRemainder + msgs[i];
                        }
                        else
                        {
                            isolatedMessage = msgs[i];
                        }
                        
                        if (isolatedMessage.Length > 0)
                        {
                            lock (inBufferLock)
                            {
                                incomingBuffer.Add(isolatedMessage);
                            }
                        }
                    }
                    
                    if (msgs.Count > 0)
                    {
                        inMessageRemainder = tempRemainder;
                    }
                    else
                    {
                        inMessageRemainder = inMessageRemainder + tempRemainder;
                    }
                    
                    missedHeartbeats = 0;
                }
            }
            else
            {
                missedHeartbeats++;
                if (missedHeartbeats >= MissedHeartbeatLimit)
                {
                    Debug.Log($"Heartbeat kill: {missedHeartbeats}");
                    CloseSocket();
                }
            }
        }
        catch (Exception e)
        {
            if (!SuppressCatchPrints)
                Debug.LogError($"Read error: {e.Message}");
            CloseSocket();
        }
    }
    
    /// <summary>
    /// Split a message by delimiter
    /// </summary>
    public static List<string> SplitMessage(string message, string delimiter, out string remainder)
    {
        List<string> messages = new List<string>();
        string inMsg = message;
        
        int pos;
        while ((pos = inMsg.IndexOf(delimiter)) != -1)
        {
            string token = inMsg.Substring(0, pos);
            messages.Add(token);
            inMsg = inMsg.Substring(pos + delimiter.Length);
        }
        
        remainder = inMsg;
        return messages;
    }
}
