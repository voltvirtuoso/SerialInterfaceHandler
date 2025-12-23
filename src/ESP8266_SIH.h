/**
 * @file ESP8266_SIH.h
 * @brief ESP8266 Serial Interface Handler - Header Declaration
 * 
 * This header file contains all class declarations, function prototypes, and
 * data structures for the ESP8266 Serial Interface Handler system. It provides
 * a comprehensive WiFi management solution with serial interface capabilities
 * specifically designed for ESP8266 microcontrollers.
 * 
 * @author ESP8266_SIH Development Team
 * @version 1.0.0
 * @date December 2025
 * 
 * @copyright MIT License
 */
#if defined(ESP8266)
#ifndef ESP8266_SIH_H
#define ESP8266_SIH_H

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <EEPROM.h>
#include <vector>
#include <functional>
#include <map>
#include <algorithm>

// Constants for EEPROM storage
#define EEPROM_SIZE 512
#define MAX_NETWORKS 5
#define SSID_MAX_LENGTH 32
#define PASSWORD_MAX_LENGTH 64
#define EEPROM_MAGIC 0x4557  // "EW" magic number for EEPROM validation

// WiFi Scan Result structure
struct WiFiScanResult {
    String ssid;
    int32_t rssi;
    uint8_t encryptionType;
    bool isHidden;
    
    bool isOpen() const;
};

// Network Credential structure for EEPROM storage
struct NetworkCredential {
    char ssid[SSID_MAX_LENGTH + 1];
    char password[PASSWORD_MAX_LENGTH + 1];
    bool isValid;
    uint32_t lastConnected;
    uint32_t connectionAttempts;
    uint32_t successfulConnections;
    
    NetworkCredential();
    void set(const String& ssidStr, const String& passwordStr);
    String getSSID() const;
    String getPassword() const;
};

// EEPROM Storage Manager - ESP8266 specific
class EEPROMStorage {
private:
    struct EEPROMHeader {
        uint16_t magic;
        uint8_t networkCount;
        uint8_t reserved;
    };
    
    EEPROMHeader _header;
    NetworkCredential _networks[MAX_NETWORKS];
    bool _initialized;
    
    // EEPROM address offsets
    const uint16_t HEADER_ADDR = 0;
    const uint16_t NETWORKS_ADDR = sizeof(EEPROMHeader);
    
    void loadHeader();
    void saveHeader();
    void loadNetworks();
    void saveNetworks();
    int findNetworkIndex(const String& ssid) const;
    void removeLeastSuccessfulNetwork();

public:
    EEPROMStorage();
    bool begin();
    bool saveNetwork(const String& ssid, const String& password);
    bool getNetwork(const String& ssid, NetworkCredential& credential);
    bool removeNetwork(const String& ssid);
    void clearAllNetworks();
    std::vector<NetworkCredential> getAllNetworks() const;
    size_t getNetworkCount() const;
    void incrementConnectionAttempts(const String& ssid);
    void markConnectionSuccessful(const String& ssid);
};

// Command Handler
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
                       const String& description = "",
                       const String& usage = "",
                       const String& category = "general");
    
    bool executeCommand(const String& commandLine);
    String getCommandHelp(const String& command) const;
    std::vector<String> getCategories() const;
    std::vector<String> getAvailableCommands(const String& category) const;

private:
    std::map<String, std::pair<CommandCallback, CommandInfo>> _commands;
};

// Help System
class HelpSystem {
private:
    CommandHandler* _commandHandler;
    HardwareSerial* _serial;

public:
    HelpSystem(CommandHandler* commandHandler, HardwareSerial* serial);
    void showHelp(const String& command = "");
    void showQuickHelp();
    void showCategoryHelp(const String& category);
    std::vector<String> getHelpCategories() const;
};

// WiFi Manager - ESP8266 specific
class WiFiManager {
private:
    EEPROMStorage _storage;
    bool _connected;
    String _connectedSSID;
    unsigned long _lastReconnectAttempt;
    const unsigned long RECONNECT_DELAY = 5000; // 5 seconds between attempts
    
    struct ConnectionStatus {
        bool connected;
        String ssid;
        String ip;
        int rssi;
    };

    void setupWiFi();
    void sortNetworksByPriority(std::vector<NetworkCredential>& networks);

public:
    WiFiManager();
    bool begin();
    bool scanNetworks(std::vector<WiFiScanResult>& results);
    bool connect(const String& ssid, const String& password, bool saveCredentials = true);
    bool autoConnect();
    bool verifyConnection();
    bool clearAllCredentials();
    std::vector<NetworkCredential> listStoredCredentials() const;
    bool removeCredential(const String& ssid);
    ConnectionStatus getStatus() const;
    bool isStorageInitialized();
};

// Menu System
class MenuSystem {
public:
    enum MenuType { MAIN, WIFI, SYSTEM, CREDENTIALS };
    
private:
    HardwareSerial* _serial;
    CommandHandler* _commandHandler;
    WiFiManager* _wifiManager;
    
    MenuType _currentMenu;
    bool _inMenuMode;
    unsigned long _lastActivity;
    unsigned long _menuTimeout;
    
    struct MenuItem {
        int id;
        String label;
        std::function<void()> action;
        String description;
        bool isEnabled;
    };
    
    std::vector<MenuItem> _mainMenuItems;
    std::vector<MenuItem> _wifiMenuItems;
    std::vector<MenuItem> _credentialsMenuItems;

    void initializeMenus();
    String getUserInput(const String& prompt, bool isSensitive = false);
    int getNumericInput(const String& prompt, int minVal = 0, int maxVal = 100);
    void clearScreen();
    void showHeader(const String& title);
    void showFooter();
    String getMenuTitle(MenuType type) const;
    
    // Menu actions
    void scanNetworks();
    void connectToNetwork(const String& presetSSID = "");
    void checkConnection();
    void showSystemStatus();
    void listStoredCredentials();
    void removeStoredCredential();
    void clearAllCredentials();

public:
    MenuSystem(HardwareSerial* serial, CommandHandler* commandHandler, WiFiManager* wifiManager);
    void showMenu(MenuType type);
    void process();
    void handleMenuInput(char inputChar);
    bool isInMenu() const;
    void exitMenu();
    void setTimeout(unsigned long timeoutMs);
};

// Main Serial Interface System
class ESP8266_SIH {
private:
    HardwareSerial* _serial;
    CommandHandler _commandHandler;
    HelpSystem _helpSystem;
    MenuSystem _menuSystem;
    WiFiManager _wifiManager;
    
    unsigned long _lastActivity;
    unsigned long _timeout;
    bool _initialized;
    unsigned long _lastReconnectCheck;
    unsigned long _lastWatchdogCheck;

    void autoReconnectWiFi();

public:
    explicit ESP8266_SIH(HardwareSerial* serial = &Serial);
    bool begin(uint32_t baudRate = 115200);
    void process();
    void registerCommand(const String& command, CommandHandler::CommandCallback handler,
                        const String& description = "", const String& usage = "",
                        const String& category = "general");
    void setTimeout(unsigned long timeoutMs);
    bool isConnected() const;
    String getConnectedSSID() const;
};

#endif // ESP8266_SIH_H
#endif // ESP8266 specific boards