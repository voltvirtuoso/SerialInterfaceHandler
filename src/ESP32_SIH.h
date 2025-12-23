/**
 * @file ESP32_SIH.h
 * @brief ESP32 Serial Interface Handler - Header File
 * 
 * This header file contains all class declarations and interfaces for the
 * ESP32 Serial Interface Handler system. It provides modern WiFi management,
 * Bluetooth integration, comprehensive system monitoring, and interactive
 * menu systems with auto-reconnect capabilities.
 * 
 * @author ESP32_SIH Development Team
 * @version 2.0.0
 * @date December 2025
 * 
 * @copyright MIT License
 */

#if defined(ESP32)
#ifndef ESP32_SIH_H
#define ESP32_SIH_H

#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include <esp_bt.h>
#include <esp_wifi.h>
#include <esp_system.h>
#include <esp_chip_info.h>
#include <esp_flash.h>
#include <vector>
#include <map>
#include <functional>

// Constants
#define SSID_MAX_LENGTH 32
#define PASSWORD_MAX_LENGTH 64
#define MAX_NETWORKS 10
#define PREFERENCES_MAGIC 0x45535033 // 'ESP3'

// Forward declarations
class CommandHandler;
class HelpSystem;
class MenuSystem;
class SystemMonitor;
class PreferencesStorage;

/**
 * @brief WiFi scan result structure
 */
struct WiFiScanResult {
    String ssid;
    int32_t rssi;
    wifi_auth_mode_t encryptionType;
    bool isHidden;
    uint8_t channel;
    String bssid;
    
    bool isOpen() const;
};

/**
 * @brief Network credential structure with connection statistics
 */
class NetworkCredential {
public:
    char ssid[SSID_MAX_LENGTH + 1];
    char password[PASSWORD_MAX_LENGTH + 1];
    bool isValid;
    uint32_t lastConnected;
    uint32_t connectionAttempts;
    uint32_t successfulConnections;
    uint8_t priority; // 1-100, higher = better
    
    NetworkCredential();
    void set(const String& ssidStr, const String& passwordStr);
    String getSSID() const;
    String getPassword() const;
};

/**
 * @brief Modern storage system using Preferences library
 */
class PreferencesStorage {
private:
    struct Header {
        uint32_t magic;
        uint8_t networkCount;
        bool autoReconnectEnabled;
        uint8_t maxReconnectAttempts;
        uint32_t reconnectTimeout; // milliseconds
        uint8_t reserved[3];
    };
    
    bool _initialized;
    Preferences* _prefs;
    Header _header;
    NetworkCredential _networks[MAX_NETWORKS];
    
    void loadHeader();
    void saveHeader();
    void loadNetworks();
    void saveNetworks();
    int findNetworkIndex(const String& ssid) const;
    void removeLeastUsefulNetwork();
    uint32_t calculateNetworkScore(const NetworkCredential& net) const;

public:
    PreferencesStorage();
    bool begin();
    
    bool saveNetwork(const String& ssid, const String& password, uint8_t priority = 50);
    bool getNetwork(const String& ssid, NetworkCredential& credential);
    bool removeNetwork(const String& ssid);
    void clearAllNetworks();
    std::vector<NetworkCredential> getAllNetworks() const;
    size_t getNetworkCount() const;
    
    void incrementConnectionAttempts(const String& ssid);
    void markConnectionSuccessful(const String& ssid);
    
    // Auto-reconnect configuration
    bool isAutoReconnectEnabled() const;
    void setAutoReconnectEnabled(bool enabled);
    uint8_t getMaxReconnectAttempts() const;
    void setMaxReconnectAttempts(uint8_t attempts);
    uint32_t getReconnectTimeout() const;
    void setReconnectTimeout(uint32_t timeoutMs);
};

/**
 * @brief Command handler system
 */
class CommandHandler {
public:
    using CommandCallback = std::function<void(const std::vector<String>& args)>;
    
    struct CommandInfo {
        String description;
        String usage;
        String category;
    };
    
    bool registerCommand(const String& command, 
                        CommandCallback callback,
                        const String& description,
                        const String& usage,
                        const String& category);
    
    bool executeCommand(const String& commandLine, bool isConnected);
    String getCommandHelp(const String& command) const;
    std::vector<String> getCategories() const;
    std::vector<String> getAvailableCommands(const String& category) const;

private:
    std::map<String, std::pair<CommandCallback, CommandInfo>> _commands;
};

/**
 * @brief Interactive help system
 */
class HelpSystem {
public:
    HelpSystem(CommandHandler* commandHandler, HardwareSerial* serial);
    
    void showHelp(const String& command = "");
    void showQuickHelp();
    void showCategoryHelp(const String& category);
    std::vector<String> getHelpCategories() const;

private:
    CommandHandler* _commandHandler;
    HardwareSerial* _serial;
};

/**
 * @brief Advanced WiFi management system
 */
class WiFiManager {
public:
    struct ConnectionStatus {
        bool connected;
        String ssid;
        String ip;
        int rssi;
        uint8_t channel;
        String macAddress;
    };
    
    WiFiManager();
    bool begin(PreferencesStorage* storage);
    bool scanNetworks(std::vector<WiFiScanResult>& results);
    bool connect(const String& ssid, const String& password, bool saveCredentials = true, uint8_t priority = 50);
    bool autoConnect();
    bool verifyConnection();
    bool clearAllCredentials();
    std::vector<NetworkCredential> listStoredCredentials() const;
    bool removeCredential(const String& ssid);
    ConnectionStatus getStatus() const;
    PreferencesStorage& getStorage() { return *_storage; }
    String getMACAddress() const { return WiFi.macAddress(); }

private:
    PreferencesStorage* _storage;
    bool _connected;
    String _connectedSSID;
    unsigned long _lastReconnectAttempt;
    uint8_t _reconnectFailures;
    unsigned long _lastSuccessfulConnection;
    
    void setupWiFi();
    void sortNetworksByPriority(std::vector<NetworkCredential>& networks);
};

/**
 * @brief Comprehensive system monitoring
 */
class SystemMonitor {
public:
    struct SystemInfo {
        String chipModel;
        uint8_t chipCores;
        uint8_t chipRevision;
        uint8_t flashSize; // MB
        bool hasPSRAM;
        uint8_t psrSize; // MB
        uint32_t freeHeap;
        uint32_t minFreeHeap;
        uint32_t maxAllocHeap;
        unsigned long uptime;
        String sdkVersion;
        String coreVersion;
    };
    
    struct BluetoothInfo {
        String macAddress;
        bool isEnabled;
    };
    
    struct WiFiInfo {
        String macAddress;
        bool isConnected;
        String ssid;
        String ipAddress;
        int rssi;
    };
    
    SystemMonitor(WiFiManager* wifiManager);
    
    SystemInfo getSystemInfo();
    BluetoothInfo getBluetoothInfo();
    WiFiInfo getWiFiInfo();

private:
    WiFiManager* _wifiManager;
};

/**
 * @brief Interactive menu system
 */
class MenuSystem {
public:
    enum MenuType {
        MAIN,
        WIFI,
        CREDENTIALS,
        SYSTEM,
        AUTORC
    };
    
    struct MenuItem {
        uint8_t id;
        String label;
        std::function<void()> action;
        String description;
        bool isEnabled;
    };
    
    MenuSystem(HardwareSerial* serial, CommandHandler* commandHandler,
              WiFiManager* wifiManager, SystemMonitor* systemMonitor);
    
    void showMenu(MenuType type);
    void process();
    void handleMenuInput(char inputChar);
    void exitMenu();
    void setTimeout(unsigned long timeoutMs);
    bool isInMenu() const;
    
    // Menu actions
    void scanNetworks();
    void connectToNetwork(const String& presetSSID = "");
    void autoConnect();
    void checkConnection();
    void listStoredCredentials();
    void removeStoredCredential();
    void setNetworkPriority();
    void clearAllCredentials();
    void showBasicSystemInfo();
    void showChipInfo();
    void showMemoryInfo();
    void showBluetoothInfo();
    void toggleAutoReconnect();
    void setMaxReconnectAttempts();
    void setReconnectTimeout();
    void viewAutoReconnectSettings();

    String getUserInput(const String& prompt, bool isSensitive = false);
    int getNumericInput(const String& prompt, int minVal, int maxVal);

private:
    HardwareSerial* _serial;
    CommandHandler* _commandHandler;
    WiFiManager* _wifiManager;
    SystemMonitor* _systemMonitor;
    
    MenuType _currentMenu;
    bool _inMenuMode;
    unsigned long _lastActivity;
    unsigned long _menuTimeout;
    
    std::vector<MenuItem> _mainMenuItems;
    std::vector<MenuItem> _wifiMenuItems;
    std::vector<MenuItem> _credentialsMenuItems;
    std::vector<MenuItem> _systemMenuItems;
    std::vector<MenuItem> _autorcMenuItems;
    
    void initializeMenus();
    std::vector<MenuItem>* getMenuItems(MenuType type);
    void clearScreen();
    void showHeader(const String& title);
    void showFooter();
    String getMenuTitle(MenuType type) const;
};

/**
 * @brief Main ESP32 Serial Interface Handler class
 */
class ESP32_SIH {
public:
    ESP32_SIH(HardwareSerial* serial = &Serial);
    
    bool begin(uint32_t baudRate = 115200);
    void process();
    
    void registerCommand(const String& command, 
                        CommandHandler::CommandCallback handler,
                        const String& description,
                        const String& usage,
                        const String& category,
                        bool requiresWiFi = false);
    
    void setTimeout(unsigned long timeoutMs);
    bool isConnected() const;
    String getConnectedSSID() const;
    String getMACAddress() const;
    
    // Public getters for system components
    SystemMonitor& getSystemMonitor() { return _systemMonitor; }
    WiFiManager& getWiFiManager() { return _wifiManager; }
    CommandHandler& getCommandHandler() { return _commandHandler; }
    
    // System information display methods
    void showSystemStatus();
    void showSystemInfo();
    void showChipInfo();
    void showMemoryInfo();
    void showBluetoothInfo();

private:
    HardwareSerial* _serial;
    CommandHandler _commandHandler;
    HelpSystem _helpSystem;
    PreferencesStorage _storage;
    WiFiManager _wifiManager;
    SystemMonitor _systemMonitor;
    MenuSystem _menuSystem;
    
    bool _initialized;
    unsigned long _lastActivity;
    unsigned long _timeout;
    
    // Reconnection management
    unsigned long _lastReconnectCheck;
    unsigned long _lastWatchdogCheck;
    unsigned long _lastSystemInfo;
    
    // Auto-reconnect settings
    bool _autoReconnectEnabled;
    uint8_t _maxReconnectAttempts;
    uint32_t _reconnectTimeout; // milliseconds
    
    void registerDefaultCommands();
    void autoReconnectWiFi();
    String getUserInput(const String& prompt);
};


#endif // ESP32_SIH_H
#endif // ESP32 based boards