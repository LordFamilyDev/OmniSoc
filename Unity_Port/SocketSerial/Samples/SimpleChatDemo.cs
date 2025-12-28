using System.Collections.Generic;
using TMPro;
using UnityEngine;
using UnityEngine.UI;

/// <summary>
/// Simple chat demo for testing SocketSerial
/// Create two builds or two instances in Editor to test client/server communication
/// </summary>
public class SimpleChatDemo : MonoBehaviour
{
    [Header("UI References")]
    public TMP_InputField ipInput;
    public TMP_InputField portInput;
    public Toggle serverToggle;
    public Button connectButton;
    public Button disconnectButton;
    public TMP_InputField messageInput;
    public Button sendButton;
    public TMP_Text chatLog;
    public TMP_Text statusText;
    
    [Header("Settings")]
    public int maxLogLines = 20;
    
    private SocketSerial socket;
    private List<string> logLines = new List<string>();
    
    void Start()
    {
        // Set default values
        if (ipInput != null) ipInput.text = "127.0.0.1";
        if (portInput != null) portInput.text = "12345";
        
        // Setup button listeners
        if (connectButton != null)
            connectButton.onClick.AddListener(OnConnectClicked);
        if (disconnectButton != null)
            disconnectButton.onClick.AddListener(OnDisconnectClicked);
        if (sendButton != null)
            sendButton.onClick.AddListener(OnSendClicked);
        if (messageInput != null)
            messageInput.onEndEdit.AddListener(OnMessageEndEdit);
        
        UpdateUI();
    }
    
    void Update()
    {
        if (socket != null && socket.IsConnected())
        {
            // Receive messages
            List<string> messages = socket.Receive();
            foreach (string msg in messages)
            {
                AddLog($"<color=cyan>Received: {msg}</color>");
            }
        }
        
        UpdateUI();
    }
    
    void OnDestroy()
    {
        socket?.Disconnect();
    }
    
    void OnApplicationQuit()
    {
        socket?.Disconnect();
    }
    
    void OnConnectClicked()
    {
        if (socket != null && socket.IsConnected())
        {
            AddLog("<color=yellow>Already connected!</color>");
            return;
        }
        
        string ip = ipInput != null ? ipInput.text : "127.0.0.1";
        int port = portInput != null ? int.Parse(portInput.text) : 12345;
        bool isServer = serverToggle != null && serverToggle.isOn;
        
        AddLog($"<color=green>Connecting as {(isServer ? "SERVER" : "CLIENT")} to {ip}:{port}...</color>");
        
        socket = new SocketSerial(ip, port, isServer);
        socket.SuppressCatchPrints = false;  // Show errors for debugging
        socket.Connect(blockingFlag: false, autoReconnect: true, _periodMs: 10);
    }
    
    void OnDisconnectClicked()
    {
        if (socket != null)
        {
            AddLog("<color=red>Disconnecting...</color>");
            socket.Disconnect();
            socket = null;
        }
    }
    
    void OnSendClicked()
    {
        SendMessage();
    }
    
    void OnMessageEndEdit(string text)
    {
        if (Input.GetKeyDown(KeyCode.Return) || Input.GetKeyDown(KeyCode.KeypadEnter))
        {
            SendMessage();
        }
    }
    
    void SendMessage()
    {
        if (socket == null || !socket.IsConnected())
        {
            AddLog("<color=yellow>Not connected!</color>");
            return;
        }
        
        if (messageInput == null || string.IsNullOrEmpty(messageInput.text))
            return;
        
        string msg = messageInput.text;
        socket.Send(msg);
        AddLog($"<color=lime>Sent: {msg}</color>");
        
        messageInput.text = "";
        messageInput.ActivateInputField();
    }
    
    void AddLog(string message)
    {
        logLines.Add($"[{System.DateTime.Now:HH:mm:ss}] {message}");
        
        // Keep only recent lines
        while (logLines.Count > maxLogLines)
        {
            logLines.RemoveAt(0);
        }
        
        // Update chat log
        if (chatLog != null)
        {
            chatLog.text = string.Join("\n", logLines);
        }
    }
    
    void UpdateUI()
    {
        bool connected = socket != null && socket.IsConnected();
        
        // Update status
        if (statusText != null)
        {
            if (connected)
            {
                bool isServer = serverToggle != null && serverToggle.isOn;
                statusText.text = $"Status: <color=green>Connected</color> ({(isServer ? "Server" : "Client")})";
            }
            else
            {
                statusText.text = "Status: <color=red>Disconnected</color>";
            }
        }
        
        // Update button states
        if (connectButton != null)
            connectButton.interactable = !connected;
        if (disconnectButton != null)
            disconnectButton.interactable = connected;
        if (sendButton != null)
            sendButton.interactable = connected;
        if (messageInput != null)
            messageInput.interactable = connected;
        if (ipInput != null)
            ipInput.interactable = !connected;
        if (portInput != null)
            portInput.interactable = !connected;
        if (serverToggle != null)
            serverToggle.interactable = !connected;
    }
}
