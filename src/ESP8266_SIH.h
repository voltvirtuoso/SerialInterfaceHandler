/**
 * @file ESP8266_SIH.h
 * @brief ESP8266 Serial Interface Handler - Header Declaration
 * 
 * This header file declares all classes, structures, and constants used by the
 * ESP8266 Serial Interface Handler system, including WiFi management, EEPROM
 * storage, command handling, help system, menu system, and main SIH controller.
 * 
 * @author ESP8266_SIH Development Team
 * @version 1.0.0
 * @date December 2025
 * 
 * @copyright MIT License
 */

#ifndef ESP8266_SIH_H
#define ESP8266_SIH_H

#if defined(ESP8266)

// Standard includes
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <EEPROM.h>
#include <vector>
#include <functional>
#include <map>

// Configuration constants
static const uint16_t EEPROM_SIZE = 1024;
static const uint16_t HEADER_ADDR = 0;
static const uint16_t NETWORKS_ADDR = sizeof(uint32_t) + 2; // sizeof(EEPROMHeader)
static const uint32_t EEPROM_MAGIC = 0x5746434E; // 'WFCN' in ASCII
static const size_t SSID_MAX_LENGTH = 32;
static const size_t PASSWORD_MAX_LENGTH = 64;
static const size_t MAX_NETWORKS = 5;

// Forward declarations
class CommandHandler;
class HelpSystem;
class WiFiManager;
class MenuSystem;

// WiFiScanResult structure
struct WiFiScanResult {
    String ssid;
    int32_t rssi;
    uint8_t encryptionType;
    bool isHidden;

    bool isOpen() const;
};

// NetworkCredential structure
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

// EEPROM Storage Header
struct EEPROMHeader {
    uint32_t magic;
    uint8_t networkCount;
    uint8_t reserved;
};

// EEPROMStorage class
class EEPROMStorage {
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

private:
    void loadHeader();
    void saveHeader();
    void loadNetworks();
    void saveNetworks();
    void removeLeastSuccessfulNetwork();
    int findNetworkIndex(const String& ssid) const;

    bool _initialized;
    EEPROMHeader _header;
    NetworkCredential _networks[MAX_NETWORKS];
};

// CommandHandler class
class CommandHandler {
public:
    using CommandCallback = std::function<void(const std::vector<String>& args)>;

    bool registerCommand(const String& command,
                         CommandCallback callback,
                         const String& description,
                         const String& usage,
                         const String& category = "general");
    bool executeCommand(const String& commandLine);
    String getCommandHelp(const String& command) const;
    std::vector<String> getCategories() const;
    std::vector<String> getAvailableCommands(const String& category = "") const;

private:
    std::map<String, std::pair<CommandCallback, struct CommandInfo>> _commands;
};

// CommandInfo structure (used by CommandHandler)
struct CommandInfo {
    String description;
    String usage;
    String category;
};

// HelpSystem class
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

// WiFiManager class
class WiFiManager {
public:
    struct ConnectionStatus {
        bool connected;
        String ssid;
        String ip;
        int32_t rssi;
    };

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

private:
    void setupWiFi();
    void sortNetworksByPriority(std::vector<NetworkCredential>& networks);

    EEPROMStorage _storage;
    bool _connected;
    String _connectedSSID;
    unsigned long _lastReconnectAttempt;
};

// MenuSystem class
class MenuSystem {
public:
    enum MenuType {
        MAIN,
        WIFI,
        CREDENTIALS,
        SYSTEM
    };

    struct MenuItem {
        int id;
        String label;
        std::function<void()> action;
        String description;
        bool isEnabled;
    };

    MenuSystem(HardwareSerial* serial,
               CommandHandler* commandHandler,
               WiFiManager* wifiManager);
    void showMenu(MenuType type);
    void process();
    void handleMenuInput(char inputChar);
    void exitMenu();
    void setTimeout(unsigned long timeoutMs);
    bool isInMenu() const;
    String getMenuTitle(MenuType type) const;

private:
    void initializeMenus();
    void scanNetworks();
    void connectToNetwork(const String& presetSSID = "");
    void checkConnection();
    void listStoredCredentials();
    void removeStoredCredential();
    void clearAllCredentials();
    void showSystemStatus();

    String getUserInput(const String& prompt, bool isSensitive = false);
    int getNumericInput(const String& prompt, int minVal, int maxVal);
    void clearScreen();
    void showHeader(const String& title);
    void showFooter();

    HardwareSerial* _serial;
    CommandHandler* _commandHandler;
    WiFiManager* _wifiManager;
    MenuType _currentMenu;
    bool _inMenuMode;
    unsigned long _lastActivity;
    unsigned long _menuTimeout;

    std::vector<MenuItem> _mainMenuItems;
    std::vector<MenuItem> _wifiMenuItems;
    std::vector<MenuItem> _credentialsMenuItems;
};

// Main System Integration Class
class ESP8266_SIH {
public:
    ESP8266_SIH(HardwareSerial* serial);
    bool begin(uint32_t baudRate = 115200);
    void process();
    void registerCommand(const String& command,
                         CommandHandler::CommandCallback handler,
                         const String& description,
                         const String& usage,
                         const String& category = "general");
    void setTimeout(unsigned long timeoutMs);
    bool isConnected() const;
    String getConnectedSSID() const;

private:
    void autoReconnectWiFi();

    HardwareSerial* _serial;
    CommandHandler _commandHandler;
    HelpSystem _helpSystem;
    WiFiManager _wifiManager;
    MenuSystem _menuSystem;
    unsigned long _lastActivity;
    unsigned long _timeout;
    bool _initialized;
    unsigned long _lastReconnectCheck;
    unsigned long _lastWatchdogCheck;
};

#endif
#endif // ESP8266_SIH_H