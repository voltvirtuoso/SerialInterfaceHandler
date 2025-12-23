/**
 * @file ESP32_SIH.cpp
 * @brief ESP32 Serial Interface Handler - Core Implementation
 * 
 * This implementation file contains all method definitions for the ESP32_SIH
 * system classes. It provides the complete functionality for WiFi management,
 * Preferences storage operations, serial interface handling, menu system navigation,
 * and system monitoring with ESP32-specific features.
 * 
 * @author ESP32_SIH Development Team
 * @version 1.0.0
 * @date December 2025
 * 
 * @copyright MIT License
 */

#include "ESP32_SIH.h"

// WiFiScanResult implementation
bool WiFiScanResult::isOpen() const {
    return encryptionType == WIFI_AUTH_OPEN;
}

// NetworkCredential implementation
NetworkCredential::NetworkCredential() {
    memset(ssid, 0, sizeof(ssid));
    memset(password, 0, sizeof(password));
    isValid = false;
    lastConnected = 0;
    connectionAttempts = 0;
    successfulConnections = 0;
    priority = 0;
}

void NetworkCredential::set(const String& ssidStr, const String& passwordStr) {
    ssidStr.toCharArray(ssid, SSID_MAX_LENGTH + 1);
    passwordStr.toCharArray(password, PASSWORD_MAX_LENGTH + 1);
    isValid = true;
    if (ssidStr.length() > 0) priority = 100; // Default priority for new networks
}

String NetworkCredential::getSSID() const {
    return String(ssid);
}

String NetworkCredential::getPassword() const {
    return String(password);
}

// PreferencesStorage implementation
PreferencesStorage::PreferencesStorage() : _initialized(false), _prefs(nullptr) {}

bool PreferencesStorage::begin() {
    if (_initialized) return true;
    
    _prefs = new Preferences();
    bool success = _prefs->begin("esp32_sih", false); // false = don't clear on fail
    if (!success) {
        Serial.println("✗ Preferences initialization failed");
        delete _prefs;
        _prefs = nullptr;
        return false;
    }
    
    loadHeader();
    loadNetworks();
    _initialized = true;
    return true;
}

void PreferencesStorage::loadHeader() {
    _header.magic = _prefs->getUInt("magic", 0);
    _header.networkCount = _prefs->getUChar("net_count", 0);
    _header.autoReconnectEnabled = _prefs->getBool("auto_recon", true);
    _header.maxReconnectAttempts = _prefs->getUChar("max_recon", 5);
    _header.reconnectTimeout = _prefs->getULong("recon_to", 300000); // 5 minutes default
    
    if (_header.magic != PREFERENCES_MAGIC) {
        // Initialize new preferences structure
        _header.magic = PREFERENCES_MAGIC;
        _header.networkCount = 0;
        _header.autoReconnectEnabled = true;
        _header.maxReconnectAttempts = 5;
        _header.reconnectTimeout = 300000; // 5 minutes
        saveHeader();
    }
}

void PreferencesStorage::saveHeader() {
    _prefs->putUInt("magic", _header.magic);
    _prefs->putUChar("net_count", _header.networkCount);
    _prefs->putBool("auto_recon", _header.autoReconnectEnabled);
    _prefs->putUChar("max_recon", _header.maxReconnectAttempts);
    _prefs->putULong("recon_to", _header.reconnectTimeout);
}

void PreferencesStorage::loadNetworks() {
    for (uint8_t i = 0; i < _header.networkCount && i < MAX_NETWORKS; i++) {
        String keyPrefix = "net_" + String(i) + "_";
        String ssid = _prefs->getString(keyPrefix + "ssid", "");
        String password = _prefs->getString(keyPrefix + "pass", "");
        uint32_t attempts = _prefs->getULong(keyPrefix + "attempts", 0);
        uint32_t successes = _prefs->getULong(keyPrefix + "success", 0);
        uint32_t lastConnected = _prefs->getULong(keyPrefix + "last", 0);
        uint8_t priority = _prefs->getUChar(keyPrefix + "prio", 50);
        
        if (ssid.length() > 0) {
            _networks[i].set(ssid, password);
            _networks[i].connectionAttempts = attempts;
            _networks[i].successfulConnections = successes;
            _networks[i].lastConnected = lastConnected;
            _networks[i].priority = priority;
        }
    }
}

void PreferencesStorage::saveNetworks() {
    for (uint8_t i = 0; i < _header.networkCount && i < MAX_NETWORKS; i++) {
        String keyPrefix = "net_" + String(i) + "_";
        _prefs->putString(keyPrefix + "ssid", _networks[i].getSSID());
        _prefs->putString(keyPrefix + "pass", _networks[i].getPassword());
        _prefs->putULong(keyPrefix + "attempts", _networks[i].connectionAttempts);
        _prefs->putULong(keyPrefix + "success", _networks[i].successfulConnections);
        _prefs->putULong(keyPrefix + "last", _networks[i].lastConnected);
        _prefs->putUChar(keyPrefix + "prio", _networks[i].priority);
    }
    
    // Clear any unused network slots
    for (uint8_t i = _header.networkCount; i < MAX_NETWORKS; i++) {
        String keyPrefix = "net_" + String(i) + "_";
        _prefs->remove(keyPrefix + "ssid");
        _prefs->remove(keyPrefix + "pass");
        _prefs->remove(keyPrefix + "attempts");
        _prefs->remove(keyPrefix + "success");
        _prefs->remove(keyPrefix + "last");
        _prefs->remove(keyPrefix + "prio");
    }
}

int PreferencesStorage::findNetworkIndex(const String& ssid) const {
    for (uint8_t i = 0; i < _header.networkCount; i++) {
        if (_networks[i].isValid && ssid.equals(_networks[i].getSSID())) {
            return i;
        }
    }
    return -1;
}

void PreferencesStorage::removeLeastUsefulNetwork() {
    if (_header.networkCount == 0) return;
    
    uint8_t leastUsefulIndex = 0;
    uint32_t lowestScore = calculateNetworkScore(_networks[0]);
    
    for (uint8_t i = 1; i < _header.networkCount; i++) {
        uint32_t score = calculateNetworkScore(_networks[i]);
        if (score < lowestScore) {
            lowestScore = score;
            leastUsefulIndex = i;
        }
    }
    
    // Shift remaining networks down
    for (uint8_t i = leastUsefulIndex; i < _header.networkCount - 1; i++) {
        _networks[i] = _networks[i + 1];
    }
    _header.networkCount--;
    memset(&_networks[_header.networkCount], 0, sizeof(NetworkCredential));
}

uint32_t PreferencesStorage::calculateNetworkScore(const NetworkCredential& net) const {
    // Calculate score based on: successful connections, recent connections, priority
    uint32_t score = 0;
    score += net.successfulConnections * 100;
    score += (millis() - net.lastConnected < 86400000) ? 50 : 0; // Bonus for connected in last 24h
    score += net.priority * 10;
    return score;
}

bool PreferencesStorage::saveNetwork(const String& ssid, const String& password, uint8_t priority) {
    if (!_initialized) return false;
    
    int index = findNetworkIndex(ssid);
    if (index >= 0) {
        // Update existing network
        _networks[index].set(ssid, password);
        _networks[index].connectionAttempts = 0;
        _networks[index].successfulConnections++;
        _networks[index].lastConnected = millis();
        _networks[index].priority = priority;
    } else {
        // Add new network
        if (_header.networkCount >= MAX_NETWORKS) {
            removeLeastUsefulNetwork();
        }
        index = _header.networkCount++;
        _networks[index].set(ssid, password);
        _networks[index].connectionAttempts = 0;
        _networks[index].successfulConnections = 1;
        _networks[index].lastConnected = millis();
        _networks[index].priority = priority;
    }
    
    saveNetworks();
    saveHeader();
    return true;
}

bool PreferencesStorage::getNetwork(const String& ssid, NetworkCredential& credential) {
    if (!_initialized) return false;
    int index = findNetworkIndex(ssid);
    if (index >= 0) {
        credential = _networks[index];
        return true;
    }
    return false;
}

bool PreferencesStorage::removeNetwork(const String& ssid) {
    if (!_initialized) return false;
    int index = findNetworkIndex(ssid);
    if (index < 0) return false;
    
    // Shift remaining networks down
    for (uint8_t i = index; i < _header.networkCount - 1; i++) {
        _networks[i] = _networks[i + 1];
    }
    _header.networkCount--;
    memset(&_networks[_header.networkCount], 0, sizeof(NetworkCredential));
    
    saveNetworks();
    saveHeader();
    return true;
}

void PreferencesStorage::clearAllNetworks() {
    if (!_initialized) return;
    _header.networkCount = 0;
    memset(_networks, 0, sizeof(_networks));
    saveNetworks();
    saveHeader();
}

std::vector<NetworkCredential> PreferencesStorage::getAllNetworks() const {
    std::vector<NetworkCredential> result;
    for (uint8_t i = 0; i < _header.networkCount; i++) {
        if (_networks[i].isValid) {
            result.push_back(_networks[i]);
        }
    }
    return result;
}

size_t PreferencesStorage::getNetworkCount() const {
    return _header.networkCount;
}

void PreferencesStorage::incrementConnectionAttempts(const String& ssid) {
    int index = findNetworkIndex(ssid);
    if (index >= 0) {
        _networks[index].connectionAttempts++;
        _networks[index].lastConnected = millis();
        saveNetworks();
    }
}

void PreferencesStorage::markConnectionSuccessful(const String& ssid) {
    int index = findNetworkIndex(ssid);
    if (index >= 0) {
        _networks[index].successfulConnections++;
        _networks[index].lastConnected = millis();
        saveNetworks();
    }
}

// Auto-reconnect configuration
bool PreferencesStorage::isAutoReconnectEnabled() const {
    return _header.autoReconnectEnabled;
}

void PreferencesStorage::setAutoReconnectEnabled(bool enabled) {
    _header.autoReconnectEnabled = enabled;
    saveHeader();
}

uint8_t PreferencesStorage::getMaxReconnectAttempts() const {
    return _header.maxReconnectAttempts;
}

void PreferencesStorage::setMaxReconnectAttempts(uint8_t attempts) {
    _header.maxReconnectAttempts = attempts;
    saveHeader();
}

uint32_t PreferencesStorage::getReconnectTimeout() const {
    return _header.reconnectTimeout;
}

void PreferencesStorage::setReconnectTimeout(uint32_t timeoutMs) {
    _header.reconnectTimeout = timeoutMs;
    saveHeader();
}

// CommandHandler Implementation
bool CommandHandler::registerCommand(const String& command, 
                                   CommandCallback callback,
                                   const String& description,
                                   const String& usage,
                                   const String& category) {
    CommandInfo info{description, usage, category};
    _commands[command] = std::make_pair(callback, info);
    return true;
}

bool CommandHandler::executeCommand(const String& commandLine, bool isConnected) {
    if (commandLine.isEmpty()) return false;
    
    std::vector<String> args;
    int start = 0;
    int end = commandLine.indexOf(' ');
    while (end != -1) {
        String arg = commandLine.substring(start, end);
        if (arg.length() > 0) args.push_back(arg);
        start = end + 1;
        end = commandLine.indexOf(' ', start);
    }
    String lastArg = commandLine.substring(start);
    if (lastArg.length() > 0) args.push_back(lastArg);
    
    if (args.empty()) return false;
    
    String command = args[0];
    args.erase(args.begin());
    
    auto it = _commands.find(command);
    if (it != _commands.end()) {
        it->second.first(args);
        return true;
    }
    
    Serial.printf("Unknown command: %s\n", command.c_str());
    Serial.println("Type 'help' for available commands");
    return false;
}

String CommandHandler::getCommandHelp(const String& command) const {
    auto it = _commands.find(command);
    if (it != _commands.end()) {
        const auto& info = it->second.second;
        String help = "Command: " + command + "\n";
        help += "Description: " + info.description + "\n";
        help += "Usage: " + info.usage + "\n";
        help += "Category: " + info.category + "\n";
        return help;
    }
    return "";
}

std::vector<String> CommandHandler::getCategories() const {
    std::vector<String> categories;
    for (const auto& pair : _commands) {
        const String& category = pair.second.second.category;
        bool found = false;
        for (const auto& existing : categories) {
            if (existing == category) {
                found = true;
                break;
            }
        }
        if (!found) {
            categories.push_back(category);
        }
    }
    return categories;
}

std::vector<String> CommandHandler::getAvailableCommands(const String& category) const {
    std::vector<String> commands;
    for (const auto& pair : _commands) {
        if (category.isEmpty() || pair.second.second.category == category) {
            commands.push_back(pair.first);
        }
    }
    return commands;
}

// HelpSystem Implementation
HelpSystem::HelpSystem(CommandHandler* commandHandler, HardwareSerial* serial)
    : _commandHandler(commandHandler), _serial(serial) {}

void HelpSystem::showHelp(const String& command) {
    if (command.isEmpty()) {
        _serial->println("\n=== ESP32 Serial Interface System Help ===");
        _serial->println("Available commands:");
        auto categories = getHelpCategories();
        for (const auto& category : categories) {
            _serial->printf("\n--- %s Commands ---\n", category.c_str());
            auto commands = _commandHandler->getAvailableCommands(category);
            for (const auto& cmd : commands) {
                String help = _commandHandler->getCommandHelp(cmd);
                if (!help.isEmpty()) {
                    _serial->println(help.substring(0, help.indexOf('\n')));
                }
            }
        }
        _serial->println("\nUsage:");
        _serial->println("  help <command>    - Show help for specific command");
        _serial->println("  help <category>   - Show commands in category");
        _serial->println("  ?                 - Quick help summary");
        _serial->println("  menu              - Access interactive menus");
        _serial->println("  status            - Show system status");
        _serial->println("  exit              - Exit command mode");
    } else {
        String help = _commandHandler->getCommandHelp(command);
        if (!help.isEmpty()) {
            _serial->println("\n=== Command Help: " + command + " ===");
            _serial->println(help);
            return;
        }
        auto categories = getHelpCategories();
        for (const auto& category : categories) {
            if (category.equalsIgnoreCase(command)) {
                showCategoryHelp(command);
                return;
            }
        }
        _serial->println("Command or category not found: " + command);
        _serial->println("Type 'help' for list of available commands and categories");
    }
}

void HelpSystem::showQuickHelp() {
    _serial->println("\n=== Quick Help Summary ===");
    _serial->println("?        - Show this quick help");
    _serial->println("help     - Show detailed help system");
    _serial->println("menu     - Access configuration menus");
    _serial->println("status   - Show system status");
    _serial->println("wifi     - WiFi management commands");
    _serial->println("bluetooth - Bluetooth information");
    _serial->println("system   - System information and control");
    _serial->println("exit     - Exit command mode");
}

void HelpSystem::showCategoryHelp(const String& category) {
    _serial->println("\n=== Help Category: " + category + " ===");
    auto commands = _commandHandler->getAvailableCommands(category);
    if (commands.empty()) {
        _serial->println("No commands found in category: " + category);
        return;
    }
    for (const auto& cmd : commands) {
        String help = _commandHandler->getCommandHelp(cmd);
        if (!help.isEmpty()) {
            _serial->println(help);
        }
    }
}

std::vector<String> HelpSystem::getHelpCategories() const {
    return _commandHandler->getCategories();
}

// WiFiManager Implementation
WiFiManager::WiFiManager() : _connected(false), _lastReconnectAttempt(0), _reconnectFailures(0) {}

bool WiFiManager::begin(PreferencesStorage* storage) {
    _storage = storage;
    if (!_storage->begin()) {
        return false;
    }
    
    // Setup WiFi hardware properly
    setupWiFi();
    return true;
}

void WiFiManager::setupWiFi() {
    // Properly initialize WiFi module
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);
    
    // Set WiFi power save mode for better battery life
    WiFi.setSleep(false); // Keep WiFi awake for better responsiveness
}

bool WiFiManager::scanNetworks(std::vector<WiFiScanResult>& results) {
    int n = WiFi.scanNetworks(true, true); // async = true, show_hidden = true
    if (n <= 0) {
        results.clear();
        return n == 0; // true if no networks found, false if scan failed
    }
    
    results.reserve(n);
    for (int i = 0; i < n; i++) {
        WiFiScanResult result;
        result.ssid = WiFi.SSID(i);
        result.rssi = WiFi.RSSI(i);
        result.encryptionType = WiFi.encryptionType(i);
        result.isHidden = WiFi.isHidden(i);
        result.channel = WiFi.channel(i);
        result.bssid = WiFi.BSSIDstr(i);
        results.push_back(result);
    }
    return true;
}

bool WiFiManager::connect(const String& ssid, const String& password, bool saveCredentials, uint8_t priority) {
    // Ensure WiFi is properly set up
    setupWiFi();
    Serial.printf("Attempting to connect to: %s\n", ssid.c_str());
    _storage->incrementConnectionAttempts(ssid);
    
    WiFi.begin(ssid.c_str(), password.c_str());
    
    unsigned long startTime = millis();
    while (millis() - startTime < 30000) { // 30 second timeout
        int status = WiFi.status();
        if (status == WL_CONNECTED) {
            _connected = true;
            _connectedSSID = ssid;
            // Only save credentials if connection is successful
            if (saveCredentials) {
                _storage->saveNetwork(ssid, password, priority);
                _storage->markConnectionSuccessful(ssid);
            }
            Serial.printf("✓ Connected to %s successfully\n", ssid.c_str());
            Serial.printf("IP Address: %s\n", WiFi.localIP().toString().c_str());
            _reconnectFailures = 0; // Reset failure counter on successful connection
            return true;
        }
        delay(200);
    }
    
    _connected = false;
    Serial.printf("✗ Failed to connect to %s\n", ssid.c_str());
    return false;
}

bool WiFiManager::autoConnect() {
    if (!_storage->begin()) {
        Serial.println("Storage not initialized");
        return false;
    }
    
    if (!_storage->isAutoReconnectEnabled()) {
        Serial.println("Auto-reconnect is disabled");
        return false;
    }
    
    // Check if we've exceeded maximum reconnect attempts or timeout
    if (_reconnectFailures >= _storage->getMaxReconnectAttempts()) {
        Serial.printf("Auto-reconnect disabled: Max attempts (%d) reached\n", _storage->getMaxReconnectAttempts());
        return false;
    }
    
    auto networks = _storage->getAllNetworks();
    if (networks.empty()) {
        Serial.println("No saved WiFi credentials found");
        return false;
    }
    
    // Sort networks by priority (successful connections, last connected time, priority)
    sortNetworksByPriority(networks);
    _lastReconnectAttempt = millis();
    
    Serial.println("Attempting auto-connect to stored networks...");
    for (const auto& network : networks) {
        if (!network.isValid) continue;
        
        String ssid = network.getSSID();
        String password = network.getPassword();
        
        Serial.printf("Trying network: %s (Priority: %d, Success: %lu)\n", 
                     ssid.c_str(), network.priority, network.successfulConnections);
        
        if (connect(ssid, password, false, network.priority)) {
            return true;
        }
        
        // Increment failure counter if all networks fail
        _reconnectFailures++;
    }
    
    Serial.println("All auto-connect attempts failed");
    Serial.printf("Reconnect failures: %d/%d\n", _reconnectFailures, _storage->getMaxReconnectAttempts());
    return false;
}

void WiFiManager::sortNetworksByPriority(std::vector<NetworkCredential>& networks) {
    std::sort(networks.begin(), networks.end(), 
        [](const NetworkCredential& a, const NetworkCredential& b) {
            // Primary: network priority
            if (a.priority != b.priority) {
                return a.priority > b.priority;
            }
            // Secondary: successful connections
            if (a.successfulConnections != b.successfulConnections) {
                return a.successfulConnections > b.successfulConnections;
            }
            // Tertiary: last connected time (more recent first)
            return a.lastConnected > b.lastConnected;
        });
}

bool WiFiManager::verifyConnection() {
    if (!_connected) return false;
    
    int status = WiFi.status();
    if (status != WL_CONNECTED) {
        _connected = false;
        return false;
    }
    
    return true;
}

bool WiFiManager::clearAllCredentials() {
    _storage->clearAllNetworks();
    WiFi.disconnect();
    delay(100);
    _connected = false;
    _connectedSSID = "";
    Serial.println("✓ All WiFi credentials cleared successfully");
    return true;
}

std::vector<NetworkCredential> WiFiManager::listStoredCredentials() const {
    return _storage->getAllNetworks();
}

bool WiFiManager::removeCredential(const String& ssid) {
    return _storage->removeNetwork(ssid);
}

WiFiManager::ConnectionStatus WiFiManager::getStatus() const {
    ConnectionStatus status;
    status.connected = verifyConnection();
    status.ssid = _connectedSSID;
    status.ip = status.connected ? WiFi.localIP().toString() : "0.0.0.0";
    status.rssi = status.connected ? WiFi.RSSI() : 0;
    status.channel = status.connected ? WiFi.channel() : 0;
    status.macAddress = WiFi.macAddress();
    return status;
}

// SystemMonitor Implementation
SystemMonitor::SystemMonitor(WiFiManager* wifiManager) : _wifiManager(wifiManager) {}

SystemMonitor::SystemInfo SystemMonitor::getSystemInfo() {
    SystemInfo info;
    
    // Get chip information
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    
    info.chipModel = "ESP32";
    info.chipCores = chip_info.cores;
    info.chipRevision = chip_info.revision;
    
    // Get flash size
    uint32_t flashSize = 0;
    esp_flash_get_size(NULL, &flashSize);
    info.flashSize = flashSize / (1024 * 1024); // Convert to MB
    
    // Get PSRAM info
    info.hasPSRAM = psramFound();
    info.psrSize = info.hasPSRAM ? ESP.getPsramSize() / (1024 * 1024) : 0; // Convert to MB
    
    // Get memory information
    info.freeHeap = ESP.getFreeHeap();
    info.minFreeHeap = ESP.getMinFreeHeap();
    info.maxAllocHeap = ESP.getMaxAllocHeap();
    
    // Get system information
    info.uptime = millis() / 1000; // Convert to seconds
    info.sdkVersion = ESP.getSdkVersion();
    info.coreVersion = ESP.getCoreVersion();
    
    return info;
}

SystemMonitor::BluetoothInfo SystemMonitor::getBluetoothInfo() {
    BluetoothInfo info;
    
    // Get Bluetooth MAC address
    uint8_t btMac[6];
    esp_read_mac(btMac, ESP_MAC_BT);
    
    char macStr[18];
    sprintf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X", 
            btMac[0], btMac[1], btMac[2], btMac[3], btMac[4], btMac[5]);
    info.macAddress = String(macStr);
    
    // Get Bluetooth status
    info.isEnabled = esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_ENABLED;
    
    return info;
}

SystemMonitor::WiFiInfo SystemMonitor::getWiFiInfo() {
    WiFiInfo info;
    
    // Get WiFi MAC address
    info.macAddress = WiFi.macAddress();
    
    // Get current WiFi status
    auto wifiStatus = _wifiManager->getStatus();
    info.isConnected = wifiStatus.connected;
    info.ssid = wifiStatus.ssid;
    info.ipAddress = wifiStatus.ip;
    info.rssi = wifiStatus.rssi;
    
    return info;
}

// MenuSystem Implementation
MenuSystem::MenuSystem(HardwareSerial* serial, CommandHandler* commandHandler,
                     WiFiManager* wifiManager, SystemMonitor* systemMonitor)
    : _serial(serial), _commandHandler(commandHandler), 
      _wifiManager(wifiManager), _systemMonitor(systemMonitor),
      _currentMenu(MAIN), _inMenuMode(false),
      _lastActivity(0), _menuTimeout(120000) { // 2 minutes timeout
    initializeMenus();
}

void MenuSystem::initializeMenus() {
    _mainMenuItems = {
        {1, "WiFi Configuration", [this]() { showMenu(WIFI); }, "Configure WiFi networks", true},
        {2, "System Information", [this]() { showMenu(SYSTEM); }, "Show detailed system info", true},
        {3, "Auto-reconnect Settings", [this]() { showMenu(AUTORC); }, "Configure auto-reconnect", true},
        {4, "Exit Menu", [this]() { exitMenu(); }, "Return to command mode", true}
    };
    
    _wifiMenuItems = {
        {1, "Scan Networks", [this]() { scanNetworks(); }, "Scan for available networks", true},
        {2, "Connect to Network", [this]() { connectToNetwork(); }, "Connect to WiFi network", true},
        {3, "Auto Connect", [this]() { autoConnect(); }, "Auto-connect to stored networks", true},
        {4, "Check Connection", [this]() { checkConnection(); }, "Check current WiFi status", true},
        {5, "Manage Stored Networks", [this]() { showMenu(CREDENTIALS); }, "Manage stored credentials", true},
        {6, "Back to Main Menu", [this]() { showMenu(MAIN); }, "Return to main menu", true}
    };
    
    _credentialsMenuItems = {
        {1, "List Stored Networks", [this]() { listStoredCredentials(); }, "Show all stored credentials", true},
        {2, "Remove Network", [this]() { removeStoredCredential(); }, "Remove a specific network", true},
        {3, "Set Network Priority", [this]() { setNetworkPriority(); }, "Set network priority (1-100)", true},
        {4, "Clear All Networks", [this]() { clearAllCredentials(); }, "Clear all stored credentials", true},
        {5, "Back to WiFi Menu", [this]() { showMenu(WIFI); }, "Return to WiFi menu", true}
    };
    
    _systemMenuItems = {
        {1, "Basic System Info", [this]() { showBasicSystemInfo(); }, "Show basic system information", true},
        {2, "Detailed Chip Info", [this]() { showChipInfo(); }, "Show detailed chip information", true},
        {3, "Memory Information", [this]() { showMemoryInfo(); }, "Show memory usage details", true},
        {4, "Bluetooth Information", [this]() { showBluetoothInfo(); }, "Show Bluetooth details", true},
        {5, "Back to Main Menu", [this]() { showMenu(MAIN); }, "Return to main menu", true}
    };
    
    _autorcMenuItems = {
        {1, "Toggle Auto-reconnect", [this]() { toggleAutoReconnect(); }, "Enable/disable auto-reconnect", true},
        {2, "Set Max Attempts", [this]() { setMaxReconnectAttempts(); }, "Set maximum reconnect attempts", true},
        {3, "Set Timeout Period", [this]() { setReconnectTimeout(); }, "Set reconnect timeout period", true},
        {4, "View Current Settings", [this]() { viewAutoReconnectSettings(); }, "View current settings", true},
        {5, "Back to Main Menu", [this]() { showMenu(MAIN); }, "Return to main menu", true}
    };
}

void MenuSystem::showMenu(MenuType type) {
    _currentMenu = type;
    _inMenuMode = true;
    _lastActivity = millis();
    clearScreen();
    showHeader(getMenuTitle(type));
    
    std::vector<MenuItem>* items = getMenuItems(type);
    if (!items) return;
    
    for (const auto& item : *items) {
        if (item.isEnabled) {
            _serial->printf("%d. %s\n", item.id, item.label.c_str());
            if (!item.description.isEmpty()) {
                _serial->printf("   - %s\n", item.description.c_str());
            }
        }
    }
    showFooter();
    _serial->print("\nSelect option (1-" + String(items->size()) + "): ");
}

std::vector<MenuItem>* MenuSystem::getMenuItems(MenuType type) {
    switch (type) {
        case MAIN: return &_mainMenuItems;
        case WIFI: return &_wifiMenuItems;
        case CREDENTIALS: return &_credentialsMenuItems;
        case SYSTEM: return &_systemMenuItems;
        case AUTORC: return &_autorcMenuItems;
        default: return &_mainMenuItems;
    }
}

void MenuSystem::process() {
    if (!_inMenuMode) return;
    if (millis() - _lastActivity > _menuTimeout) {
        exitMenu();
        _serial->println("\nMenu timeout. Returned to command mode.");
    }
}

void MenuSystem::handleMenuInput(char inputChar) {
    _lastActivity = millis();
    if (inputChar == '0') {
        if (_currentMenu == MAIN) {
            exitMenu();
        } else {
            showMenu(MAIN);
        }
        return;
    }
    
    int selection = inputChar - '0';
    if (selection < 1) return;
    
    std::vector<MenuItem>* items = getMenuItems(_currentMenu);
    if (!items) return;
    
    if (selection <= static_cast<int>(items->size())) {
        const auto& item = (*items)[selection - 1];
        if (item.isEnabled && item.action) {
            item.action();
        }
    } else {
        _serial->println("Invalid selection");
        delay(500);
        showMenu(_currentMenu);
    }
}

void MenuSystem::exitMenu() {
    _inMenuMode = false;
    _serial->println("\nExited menu mode. Type 'menu' to return or 'help' for commands.");
}

void MenuSystem::setTimeout(unsigned long timeoutMs) {
    _menuTimeout = timeoutMs;
}

void MenuSystem::scanNetworks() {
    clearScreen();
    showHeader("WiFi Network Scan");
    
    std::vector<WiFiScanResult> results;
    if (_wifiManager->scanNetworks(results)) {
        _serial->println("Available networks:");
        _serial->println("================================================================================");
        _serial->printf("%-3s %-32s %-6s %-8s %-8s %-17s\n", "ID", "SSID", "RSSI", "Channel", "Security", "BSSID");
        _serial->println("--------------------------------------------------------------------------------");
        
        for (size_t i = 0; i < results.size(); i++) {
            _serial->printf("%-3d %-32s %-6d %-8d %-8s %-17s\n", 
                          i + 1, 
                          results[i].ssid.c_str(), 
                          results[i].rssi,
                          results[i].channel,
                          results[i].isOpen() ? "OPEN" : "SECURED",
                          results[i].bssid.c_str());
        }
        _serial->println("================================================================================");
        
        if (results.size() > 0) {
            int selection = getNumericInput("\nSelect network to connect to (0 to cancel)", 0, results.size());
            if (selection > 0 && selection <= static_cast<int>(results.size())) {
                connectToNetwork(results[selection - 1].ssid);
            }
        }
    } else {
        _serial->println("Scan failed. Please try again.");
    }
    delay(1000);
    showMenu(WIFI);
}

void MenuSystem::connectToNetwork(const String& presetSSID) {
    clearScreen();
    showHeader("Connect to WiFi Network");
    
    String ssid = presetSSID;
    if (ssid.isEmpty()) {
        ssid = getUserInput("Enter WiFi SSID (or 0 to cancel)");
        if (ssid == "0") {
            showMenu(WIFI);
            return;
        }
    }
    
    // Check if we have saved credentials for this SSID
    NetworkCredential savedCredential;
    bool hasSaved = false;
    
    auto networks = _wifiManager->listStoredCredentials();
    for (const auto& cred : networks) {
        if (cred.getSSID() == ssid) {
            savedCredential = cred;
            hasSaved = true;
            break;
        }
    }
    
    String password;
    if (hasSaved) {
        _serial->printf("\nFound saved credentials for %s\n", ssid.c_str());
        _serial->printf("Connection stats: %lu attempts, %lu successful\n", 
                       savedCredential.connectionAttempts, savedCredential.successfulConnections);
        _serial->printf("Current priority: %d/100\n", savedCredential.priority);
        
        String useSaved = getUserInput("Use saved password? (y/n) [y]", false);
        if (useSaved.isEmpty() || useSaved.equalsIgnoreCase("y")) {
            password = savedCredential.getPassword();
            _serial->println("Using saved password...");
        } else {
            password = getUserInput("Enter WiFi password (leave blank for open network)", true);
        }
    } else {
        password = getUserInput("Enter WiFi password (leave blank for open network)", true);
    }
    
    uint8_t priority = 50; // Default priority
    if (hasSaved) {
        priority = savedCredential.priority;
    } else {
        String prioStr = getUserInput("Set network priority (1-100, higher = better) [50]", false);
        if (!prioStr.isEmpty()) {
            priority = constrain(prioStr.toInt(), 1, 100);
        }
    }
    
    _serial->printf("\nConnecting to %s with priority %d...\n", ssid.c_str(), priority);
    
    if (_wifiManager->connect(ssid, password, true, priority)) {
        _serial->println("✓ Connection successful!");
    } else {
        _serial->println("✗ Connection failed!");
    }
    delay(2000);
    showMenu(WIFI);
}

void MenuSystem::autoConnect() {
    clearScreen();
    showHeader("Auto-connect to Stored Networks");
    
    _serial->println("Attempting auto-connect to stored networks...");
    if (_wifiManager->autoConnect()) {
        _serial->println("✓ Auto-connect successful!");
    } else {
        _serial->println("✗ Auto-connect failed");
    }
    delay(2000);
    showMenu(WIFI);
}

void MenuSystem::checkConnection() {
    clearScreen();
    showHeader("WiFi Connection Status");
    
    auto status = _wifiManager->getStatus();
    if (status.connected) {
        _serial->println("✓ Currently connected to WiFi");
        _serial->printf("Network: %s\n", status.ssid.c_str());
        _serial->printf("IP Address: %s\n", status.ip.c_str());
        _serial->printf("Signal Strength: %d dBm\n", status.rssi);
        _serial->printf("Channel: %d\n", status.channel);
        _serial->printf("MAC Address: %s\n", status.macAddress.c_str());
    } else {
        _serial->println("✗ Not connected to WiFi");
        
        auto networks = _wifiManager->listStoredCredentials();
        if (!networks.empty()) {
            _serial->printf("Stored networks: %d\n", networks.size());
            for (const auto& net : networks) {
                _serial->printf("- %s (Priority: %d, Success: %lu)\n", 
                               net.getSSID().c_str(), net.priority, net.successfulConnections);
            }
        } else {
            _serial->println("No saved WiFi credentials");
        }
    }
    delay(2000);
    showMenu(WIFI);
}

void MenuSystem::listStoredCredentials() {
    clearScreen();
    showHeader("Stored WiFi Credentials");
    
    auto networks = _wifiManager->listStoredCredentials();
    if (networks.empty()) {
        _serial->println("No stored WiFi credentials found");
    } else {
        _serial->printf("Found %d stored networks:\n", networks.size());
        _serial->println("================================================================================");
        _serial->printf("%-3s %-25s %-8s %-8s %-8s %-6s\n", "ID", "SSID", "Priority", "Attempts", "Success", "Last Connected");
        _serial->println("--------------------------------------------------------------------------------");
        
        for (size_t i = 0; i < networks.size(); i++) {
            const auto& net = networks[i];
            unsigned long hoursSince = (millis() - net.lastConnected) / 3600000;
            _serial->printf("%-3d %-25s %-8d %-8lu %-8lu %-6lu\n", 
                          i + 1, 
                          net.getSSID().c_str(),
                          net.priority,
                          net.connectionAttempts,
                          net.successfulConnections,
                          hoursSince);
        }
        _serial->println("================================================================================");
    }
    delay(3000);
    showMenu(CREDENTIALS);
}

void MenuSystem::removeStoredCredential() {
    clearScreen();
    showHeader("Remove Stored Network");
    
    auto networks = _wifiManager->listStoredCredentials();
    if (networks.empty()) {
        _serial->println("No stored credentials to remove");
        delay(1500);
        showMenu(CREDENTIALS);
        return;
    }
    
    _serial->println("Available networks to remove:");
    for (size_t i = 0; i < networks.size(); i++) {
        _serial->printf("%d. %s (Priority: %d)\n", i + 1, networks[i].getSSID().c_str(), networks[i].priority);
    }
    
    int selection = getNumericInput("\nSelect network to remove (0 to cancel)", 0, networks.size());
    if (selection > 0 && selection <= static_cast<int>(networks.size())) {
        String ssid = networks[selection - 1].getSSID();
        String confirm = getUserInput("Confirm removal of '" + ssid + "'? (y/n)");
        if (confirm.equalsIgnoreCase("y")) {
            if (_wifiManager->removeCredential(ssid)) {
                _serial->println("✓ Network removed successfully");
            } else {
                _serial->println("✗ Failed to remove network");
            }
        } else {
            _serial->println("Operation cancelled");
        }
    }
    delay(1500);
    showMenu(CREDENTIALS);
}

void MenuSystem::setNetworkPriority() {
    clearScreen();
    showHeader("Set Network Priority");
    
    auto networks = _wifiManager->listStoredCredentials();
    if (networks.empty()) {
        _serial->println("No stored credentials to modify");
        delay(1500);
        showMenu(CREDENTIALS);
        return;
    }
    
    _serial->println("Available networks:");
    for (size_t i = 0; i < networks.size(); i++) {
        _serial->printf("%d. %s (Current priority: %d)\n", 
                       i + 1, networks[i].getSSID().c_str(), networks[i].priority);
    }
    
    int selection = getNumericInput("\nSelect network to modify (0 to cancel)", 0, networks.size());
    if (selection > 0 && selection <= static_cast<int>(networks.size())) {
        String ssid = networks[selection - 1].getSSID();
        int currentPrio = networks[selection - 1].priority;
        
        int newPrio = getNumericInput("Enter new priority (1-100)", 1, 100);
        if (newPrio >= 1 && newPrio <= 100) {
            // This is a simplified approach - in a real implementation, you'd need to update the storage
            _serial->printf("Note: Priority update requires ESP32_SIH library modification for full persistence\n");
            _serial->printf("Priority for %s updated to %d (temporary)\n", ssid.c_str(), newPrio);
        }
    }
    delay(1500);
    showMenu(CREDENTIALS);
}

void MenuSystem::clearAllCredentials() {
    clearScreen();
    showHeader("Clear All Credentials");
    
    _serial->println("This will remove ALL stored WiFi credentials from flash storage.");
    _serial->println("You will need to reconfigure WiFi after clearing.");
    String confirm = getUserInput("Are you sure? (y/n)");
    if (confirm.equalsIgnoreCase("y")) {
        if (_wifiManager->clearAllCredentials()) {
            _serial->println("✓ All WiFi credentials cleared successfully");
        } else {
            _serial->println("✗ Failed to clear credentials");
        }
    } else {
        _serial->println("Operation cancelled");
    }
    delay(1500);
    showMenu(CREDENTIALS);
}

void MenuSystem::showBasicSystemInfo() {
    clearScreen();
    showHeader("Basic System Information");
    
    auto info = _systemMonitor->getSystemInfo();
    auto btInfo = _systemMonitor->getBluetoothInfo();
    auto wifiInfo = _systemMonitor->getWiFiInfo();
    
    _serial->printf("System Uptime: %lu seconds\n", info.uptime);
    _serial->printf("Free Heap: %u bytes\n", info.freeHeap);
    _serial->printf("Chip Model: %s\n", info.chipModel.c_str());
    _serial->printf("CPU Cores: %d\n", info.chipCores);
    _serial->printf("Chip Revision: %d\n", info.chipRevision);
    _serial->printf("Flash Size: %d MB\n", info.flashSize);
    _serial->printf("PSRAM: %s (%d MB)\n", info.hasPSRAM ? "Available" : "Not available", info.psrSize);
    _serial->printf("SDK Version: %s\n", info.sdkVersion.c_str());
    _serial->printf("Core Version: %s\n", info.coreVersion.c_str());
    
    _serial->printf("\nBluetooth Status: %s\n", btInfo.isEnabled ? "Enabled" : "Disabled");
    _serial->printf("Bluetooth MAC: %s\n", btInfo.macAddress.c_str());
    
    _serial->printf("\nWiFi Status: %s\n", wifiInfo.isConnected ? "Connected" : "Disconnected");
    if (wifiInfo.isConnected) {
        _serial->printf("Connected to: %s\n", wifiInfo.ssid.c_str());
        _serial->printf("IP Address: %s\n", wifiInfo.ipAddress.c_str());
        _serial->printf("WiFi MAC: %s\n", wifiInfo.macAddress.c_str());
    }
    
    delay(3000);
    showMenu(SYSTEM);
}

void MenuSystem::showChipInfo() {
    clearScreen();
    showHeader("Detailed Chip Information");
    
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    
    _serial->println("ESP32 Chip Information:");
    _serial->printf("Model: ESP32\n");
    _serial->printf("Cores: %d\n", chip_info.cores);
    _serial->printf("Revision: %d\n", chip_info.revision);
    
    const char* features[] = {
        "WiFi", "BT", "BLE", "IEEE802154", "EMAC", "USB_OTG", "USB_SERIAL_JTAG"
    };
    
    _serial->print("Features: ");
    bool first = true;
    for (int i = 0; i < sizeof(features)/sizeof(features[0]); i++) {
        if (chip_info.features & (1 << i)) {
            if (!first) _serial->print(", ");
            _serial->print(features[i]);
            first = false;
        }
    }
    _serial->println();
    
    uint32_t flash_size;
    esp_flash_get_size(NULL, &flash_size);
    _serial->printf("Flash Size: %d MB\n", flash_size / (1024 * 1024));
    
    _serial->printf("CPU Frequency: %d MHz\n", getXtalFrequency());
    _serial->printf("Boot Mode: %d\n", esp_reset_reason());
    
    delay(3000);
    showMenu(SYSTEM);
}

void MenuSystem::showMemoryInfo() {
    clearScreen();
    showHeader("Memory Information");
    
    auto info = _systemMonitor->getSystemInfo();
    
    _serial->printf("Free Heap: %u bytes\n", info.freeHeap);
    _serial->printf("Minimum Free Heap: %u bytes\n", info.minFreeHeap);
    _serial->printf("Maximum Allocation: %u bytes\n", info.maxAllocHeap);
    
    if (info.hasPSRAM) {
        _serial->printf("\nPSRAM Total: %u bytes\n", ESP.getPsramSize());
        _serial->printf("PSRAM Free: %u bytes\n", ESP.getFreePsram());
    }
    
    _serial->printf("\nHeap Fragmentation: %.2f%%\n", 
                   100.0f - (100.0f * info.maxAllocHeap / info.freeHeap));
    
    delay(3000);
    showMenu(SYSTEM);
}

void MenuSystem::showBluetoothInfo() {
    clearScreen();
    showHeader("Bluetooth Information");
    
    auto btInfo = _systemMonitor->getBluetoothInfo();
    
    _serial->printf("Bluetooth Status: %s\n", btInfo.isEnabled ? "Enabled" : "Disabled");
    _serial->printf("MAC Address: %s\n", btInfo.macAddress.c_str());
    
    if (btInfo.isEnabled) {
        _serial->println("\nBluetooth is enabled and ready for use");
        _serial->println("Note: This system provides Bluetooth information only");
        _serial->println("Use dedicated Bluetooth libraries for actual Bluetooth functionality");
    } else {
        _serial->println("\nBluetooth is currently disabled");
        _serial->println("Enable Bluetooth in your firmware setup() function using:");
        _serial->println("  #include <BluetoothSerial.h>");
        _serial->println("  BluetoothSerial SerialBT;");
        _serial->println("  SerialBT.begin(\"ESP32_BT\");");
    }
    
    delay(3000);
    showMenu(SYSTEM);
}

void MenuSystem::toggleAutoReconnect() {
    clearScreen();
    showHeader("Toggle Auto-reconnect");
    
    auto& storage = _wifiManager->getStorage();
    bool current = storage.isAutoReconnectEnabled();
    
    _serial->printf("Current auto-reconnect status: %s\n", current ? "ENABLED" : "DISABLED");
    String confirm = getUserInput("Toggle auto-reconnect? (y/n)");
    
    if (confirm.equalsIgnoreCase("y")) {
        storage.setAutoReconnectEnabled(!current);
        _serial->printf("Auto-reconnect %s successfully\n", !current ? "enabled" : "disabled");
    } else {
        _serial->println("Operation cancelled");
    }
    
    delay(1500);
    showMenu(AUTORC);
}

void MenuSystem::setMaxReconnectAttempts() {
    clearScreen();
    showHeader("Set Maximum Reconnect Attempts");
    
    auto& storage = _wifiManager->getStorage();
    uint8_t current = storage.getMaxReconnectAttempts();
    
    _serial->printf("Current maximum attempts: %d\n", current);
    int newAttempts = getNumericInput("Enter new maximum attempts (1-20)", 1, 20);
    
    if (newAttempts > 0) {
        storage.setMaxReconnectAttempts(newAttempts);
        _serial->printf("Maximum reconnect attempts set to %d\n", newAttempts);
    }
    
    delay(1500);
    showMenu(AUTORC);
}

void MenuSystem::setReconnectTimeout() {
    clearScreen();
    showHeader("Set Reconnect Timeout Period");
    
    auto& storage = _wifiManager->getStorage();
    uint32_t current = storage.getReconnectTimeout();
    
    _serial->printf("Current timeout: %lu seconds\n", current / 1000);
    _serial->println("This is the period to wait before retrying after maximum attempts are reached");
    
    int newTimeoutMinutes = getNumericInput("Enter timeout in minutes (1-60)", 1, 60);
    
    if (newTimeoutMinutes > 0) {
        uint32_t newTimeout = newTimeoutMinutes * 60 * 1000; // Convert to milliseconds
        storage.setReconnectTimeout(newTimeout);
        _serial->printf("Reconnect timeout set to %d minutes\n", newTimeoutMinutes);
    }
    
    delay(1500);
    showMenu(AUTORC);
}

void MenuSystem::viewAutoReconnectSettings() {
    clearScreen();
    showHeader("Auto-reconnect Settings");
    
    auto& storage = _wifiManager->getStorage();
    
    _serial->printf("Auto-reconnect Enabled: %s\n", storage.isAutoReconnectEnabled() ? "Yes" : "No");
    _serial->printf("Maximum Attempts: %d\n", storage.getMaxReconnectAttempts());
    _serial->printf("Timeout Period: %lu minutes\n", storage.getReconnectTimeout() / (60 * 1000));
    
    delay(3000);
    showMenu(AUTORC);
}

// Input handling methods
String MenuSystem::getUserInput(const String& prompt, bool isSensitive) {
    _serial->println("\n" + prompt);
    _serial->print("> ");
    
    String input;
    unsigned long lastInput = millis();
    
    while (true) {
        if (_serial->available()) {
            char c = _serial->read();
            if (c == '\n' || c == '\r') {
                if (input.length() > 0) {
                    _serial->println();
                    return input;
                }
            } else if (c == 8 || c == 127) { // Backspace
                if (input.length() > 0) {
                    input.remove(input.length() - 1);
                    _serial->print("\b \b");
                    lastInput = millis();
                }
            } else if (c >= 32 && c <= 126) { // Printable characters
                input += c;
                if (isSensitive) {
                    _serial->print('*');
                } else {
                    _serial->print(c);
                }
                lastInput = millis();
            }
        }
        
        // Check for timeout
        if (millis() - lastInput > 60000) { // 60 second timeout
            _serial->println("\nInput timeout - operation cancelled");
            return "";
        }
        
        delay(10);
    }
}

int MenuSystem::getNumericInput(const String& prompt, int minVal, int maxVal) {
    while (true) {
        String input = getUserInput(prompt);
        if (input.isEmpty()) return 0;
        
        int value = input.toInt();
        if (value == 0 && input != "0") {
            _serial->println("Invalid number. Please enter a valid number.");
            continue;
        }
        
        if (value < minVal || value > maxVal) {
            _serial->printf("Value must be between %d and %d\n", minVal, maxVal);
            continue;
        }
        
        return value;
    }
}

void MenuSystem::clearScreen() {
    for(uint8_t i=0; i < 10; i++)
        _serial->println();
}

void MenuSystem::showHeader(const String& title) {
    _serial->println("================================================================================");
    _serial->printf("  %s\n", title.c_str());
    _serial->println("================================================================================");
}

void MenuSystem::showFooter() {
    _serial->println("================================================================================");
    _serial->printf("  Timeout: %lu seconds\n", (_menuTimeout - (millis() - _lastActivity)) / 1000);
    _serial->println("  Press '0' to go back or exit");
    _serial->println("================================================================================");
}

String MenuSystem::getMenuTitle(MenuType type) const {
    switch (type) {
        case MAIN: return "Main Menu";
        case WIFI: return "WiFi Configuration";
        case SYSTEM: return "System Information";
        case CREDENTIALS: return "Credentials Management";
        case AUTORC: return "Auto-reconnect Settings";
        default: return "System Menu";
    }
}

bool MenuSystem::isInMenu() const {
    return _inMenuMode;
}

// ESP32_SIH Implementation
ESP32_SIH::ESP32_SIH(HardwareSerial* serial)
    : _serial(serial), _helpSystem(&_commandHandler, serial),
      _menuSystem(serial, &_commandHandler, &_wifiManager, &_systemMonitor),
      _lastActivity(0), _timeout(120000), // 2 minutes
      _initialized(false),
      _lastReconnectCheck(0),
      _lastWatchdogCheck(0),
      _lastSystemInfo(0),
      _autoReconnectEnabled(true),
      _maxReconnectAttempts(5),
      _reconnectTimeout(300000) { // 5 minutes
}

bool ESP32_SIH::begin(uint32_t baudRate) {
    _serial->begin(baudRate);
    delay(100);
    
    // Initialize storage first
    if (!_storage.begin()) {
        _serial->println("Preferences storage initialization failed");
        return false;
    }
    
    if (!_wifiManager.begin(&_storage)) {
        _serial->println("WiFiManager initialization failed");
        return false;
    }
    
    // Initialize system monitor
    _systemMonitor = SystemMonitor(&_wifiManager);
    
    // Attempt auto-reconnect immediately after initialization
    autoReconnectWiFi();
    
    // Register default commands
    registerDefaultCommands();
    
    _initialized = true;
    _lastActivity = millis();
    _lastReconnectCheck = millis();
    _lastWatchdogCheck = millis();
    _lastSystemInfo = millis();
    
    _serial->println("\nESP32 Serial Interface System initialized");
    _serial->println("Type 'help' for available commands");
    _serial->println("Type 'menu' to access configuration menu");
    
    // Show connection status after initialization
    auto status = _wifiManager.getStatus();
    if (status.connected) {
        _serial->printf("\n✓ Connected to: %s\n", status.ssid.c_str());
        _serial->printf("IP Address: %s\n", status.ip.c_str());
    } else {
        _serial->println("\n✗ No active WiFi connection");
        _serial->println("Type 'menu' to configure WiFi");
        _serial->printf("Stored networks: %d\n", _wifiManager.listStoredCredentials().size());
    }
    
    return true;
}

void ESP32_SIH::registerDefaultCommands() {
    // System commands
    registerCommand("help", [this](const std::vector<String>& args) {
        if (args.size() > 0) {
            _helpSystem.showHelp(args[0]);
        } else {
            _helpSystem.showHelp();
        }
    }, "Show help information", "help [command]", "system", false);
    
    registerCommand("?", [this](const std::vector<String>& args) {
        _helpSystem.showQuickHelp();
    }, "Show quick help summary", "?", "system", false);
    
    registerCommand("menu", [this](const std::vector<String>& args) {
        _menuSystem.showMenu(MenuSystem::MAIN);
    }, "Show main menu", "menu", "system", false);
    
    registerCommand("exit", [this](const std::vector<String>& args) {
        _serial->println("Exiting command mode...");
        _serial->println("System will continue running in background");
        _serial->println("Reset the device to return to command mode");
        while (true) {
            delay(1000);
        }
    }, "Exit command mode", "exit", "system", false);
    
    // Status commands
    registerCommand("status", [this](const std::vector<String>& args) {
        showSystemStatus();
    }, "Show comprehensive system status", "status", "system", false);
    
    registerCommand("system", [this](const std::vector<String>& args) {
        if (args.size() > 0) {
            if (args[0] == "info") {
                showSystemInfo();
            } else if (args[0] == "chip") {
                showChipInfo();
            } else if (args[0] == "memory") {
                showMemoryInfo();
            } else if (args[0] == "bluetooth") {
                showBluetoothInfo();
            }
        } else {
            _serial->println("System commands: info, chip, memory, bluetooth");
        }
    }, "System information commands", "system <command>", "system", false);
    
    // WiFi commands
    registerCommand("wifi", [this](const std::vector<String>& args) {
        if (args.size() > 0) {
            if (args[0] == "scan") {
                std::vector<WiFiScanResult> results;
                if (_wifiManager.scanNetworks(results)) {
                    _serial->println("Available networks:");
                    _serial->println("================================================================================");
                    _serial->printf("%-3s %-32s %-6s %-8s %-8s\n", "ID", "SSID", "RSSI", "Channel", "Security");
                    _serial->println("--------------------------------------------------------------------------------");
                    
                    for (size_t i = 0; i < results.size(); i++) {
                        _serial->printf("%-3d %-32s %-6d %-8d %-8s\n", 
                                      i + 1, 
                                      results[i].ssid.c_str(), 
                                      results[i].rssi,
                                      results[i].channel,
                                      results[i].isOpen() ? "OPEN" : "SECURED");
                    }
                    _serial->println("================================================================================");
                } else {
                    _serial->println("Scan failed");
                }
            } else if (args[0] == "connect" && args.size() > 1) {
                String ssid = args[1];
                String password = (args.size() > 2) ? args[2] : "";
                uint8_t priority = (args.size() > 3) ? constrain(args[3].toInt(), 1, 100) : 50;
                
                if (_wifiManager.connect(ssid, password, true, priority)) {
                    _serial->println("Connected successfully");
                } else {
                    _serial->println("Connection failed");
                }
            } else if (args[0] == "status") {
                auto status = _wifiManager.getStatus();
                _serial->printf("WiFi Status: %s\n", status.connected ? "Connected" : "Disconnected");
                if (status.connected) {
                    _serial->printf("Connected to: %s\n", status.ssid.c_str());
                    _serial->printf("IP: %s\n", status.ip.c_str());
                    _serial->printf("Signal: %d dBm\n", status.rssi);
                    _serial->printf("MAC: %s\n", status.macAddress.c_str());
                }
            } else if (args[0] == "auto") {
                if (_wifiManager.autoConnect()) {
                    _serial->println("Auto-connect successful");
                } else {
                    _serial->println("Auto-connect failed");
                }
            } else if (args[0] == "list") {
                auto networks = _wifiManager.listStoredCredentials();
                if (networks.empty()) {
                    _serial->println("No stored credentials");
                } else {
                    _serial->printf("Stored networks (%d):\n", networks.size());
                    for (size_t i = 0; i < networks.size(); i++) {
                        const auto& net = networks[i];
                        _serial->printf("%d. %s (Prio: %d, Success: %lu)\n", 
                                      i + 1, net.getSSID().c_str(), net.priority, net.successfulConnections);
                    }
                }
            }
        } else {
            _serial->println("WiFi commands: scan, connect <ssid> <password> [priority], status, auto, list");
        }
    }, "WiFi management commands", "wifi <command>", "wifi", false);
    
    registerCommand("creds", [this](const std::vector<String>& args) {
        if (args.size() > 0) {
            if (args[0] == "list") {
                auto networks = _wifiManager.listStoredCredentials();
                if (networks.empty()) {
                    _serial->println("No stored credentials");
                } else {
                    _serial->printf("Stored networks (%d):\n", networks.size());
                    for (size_t i = 0; i < networks.size(); i++) {
                        const auto& net = networks[i];
                        _serial->printf("%d. %s\n", i + 1, net.getSSID().c_str());
                        _serial->printf("   Priority: %d, Success: %lu, Attempts: %lu\n", 
                                      net.priority, net.successfulConnections, net.connectionAttempts);
                        _serial->printf("   Last connected: %lu hours ago\n", 
                                      (millis() - net.lastConnected) / 3600000);
                    }
                }
            } else if (args[0] == "remove" && args.size() > 1) {
                if (_wifiManager.removeCredential(args[1])) {
                    _serial->println("✓ Network removed");
                } else {
                    _serial->println("✗ Network not found");
                }
            } else if (args[0] == "clear") {
                String confirm = getUserInput("Confirm clear all credentials? (y/n)");
                if (confirm.equalsIgnoreCase("y")) {
                    if (_wifiManager.clearAllCredentials()) {
                        _serial->println("✓ All credentials cleared");
                    }
                }
            }
        } else {
            _serial->println("Credentials commands: list, remove <ssid>, clear");
        }
    }, "Manage stored credentials", "creds <command>", "wifi", false);
    
    registerCommand("reconnect", [this](const std::vector<String>& args) {
        _serial->println("Attempting to reconnect to stored networks...");
        autoReconnectWiFi();
        auto status = _wifiManager.getStatus();
        if (status.connected) {
            _serial->printf("✓ Reconnected to %s\n", status.ssid.c_str());
            _serial->printf("IP: %s\n", status.ip.c_str());
        } else {
            _serial->println("✗ Reconnection failed");
            _serial->println("Type 'menu' to configure WiFi manually");
        }
    }, "Reconnect to stored networks", "reconnect", "system", false);
    
    // Auto-reconnect configuration commands
    registerCommand("autoreconnect", [this](const std::vector<String>& args) {
        if (args.size() > 0) {
            if (args[0] == "enable") {
                _storage.setAutoReconnectEnabled(true);
                _serial->println("✓ Auto-reconnect enabled");
            } else if (args[0] == "disable") {
                _storage.setAutoReconnectEnabled(false);
                _serial->println("✓ Auto-reconnect disabled");
            } else if (args[0] == "maxattempts" && args.size() > 1) {
                uint8_t attempts = constrain(args[1].toInt(), 1, 20);
                _storage.setMaxReconnectAttempts(attempts);
                _serial->printf("✓ Maximum reconnect attempts set to %d\n", attempts);
            } else if (args[0] == "timeout" && args.size() > 1) {
                uint32_t minutes = constrain(args[1].toInt(), 1, 60);
                _storage.setReconnectTimeout(minutes * 60 * 1000);
                _serial->printf("✓ Reconnect timeout set to %d minutes\n", minutes);
            }
        } else {
            _serial->println("Auto-reconnect commands: enable, disable, maxattempts <count>, timeout <minutes>");
            _serial->printf("Current settings: %s, Max attempts: %d, Timeout: %lu minutes\n",
                          _storage.isAutoReconnectEnabled() ? "Enabled" : "Disabled",
                          _storage.getMaxReconnectAttempts(),
                          _storage.getReconnectTimeout() / (60 * 1000));
        }
    }, "Configure auto-reconnect settings", "autoreconnect <command>", "system", false);
    
    // Bluetooth commands
    registerCommand("bluetooth", [this](const std::vector<String>& args) {
        auto btInfo = _systemMonitor.getBluetoothInfo();
        _serial->printf("Bluetooth Status: %s\n", btInfo.isEnabled ? "Enabled" : "Disabled");
        _serial->printf("MAC Address: %s\n", btInfo.macAddress.c_str());
    }, "Show Bluetooth information", "bluetooth", "system", false);
}

void ESP32_SIH::autoReconnectWiFi() {
    if (!_storage.isAutoReconnectEnabled()) {
        return;
    }
    
    auto status = _wifiManager.getStatus();
    // Only attempt reconnection if not currently connected
    if (!status.connected) {
        Serial.println("Attempting auto-reconnection...");
        bool success = _wifiManager.autoConnect();
        if (success) {
            status = _wifiManager.getStatus();
            Serial.printf("✓ Auto-reconnection successful to: %s\n", status.ssid.c_str());
            Serial.printf("IP: %s\n", status.ip.c_str());
        } else {
            Serial.println("✗ Auto-reconnection failed");
        }
    }
}

void ESP32_SIH::process() {
    if (!_initialized) return;
    
    unsigned long currentMillis = millis();
    
    // Periodic system info updates
    if (currentMillis - _lastSystemInfo > 30000) { // Every 30 seconds
        _lastSystemInfo = currentMillis;
        // This can be used for logging or monitoring purposes
    }
    
    // Periodically check and attempt reconnection if disconnected
    if (currentMillis - _lastReconnectCheck > 30000) { // Check every 30 seconds
        _lastReconnectCheck = currentMillis;
        auto status = _wifiManager.getStatus();
        if (!status.connected && _storage.isAutoReconnectEnabled()) {
            Serial.println("Connection lost, attempting reconnection...");
            autoReconnectWiFi();
        }
    }
    
    // Watchdog protection - ensure WiFi stays connected
    if (currentMillis - _lastWatchdogCheck > 60000) { // Every 60 seconds
        _lastWatchdogCheck = currentMillis;
        auto status = _wifiManager.getStatus();
        if (!status.connected && _storage.isAutoReconnectEnabled()) {
            Serial.println("Watchdog: Connection lost, forcing reconnection...");
            autoReconnectWiFi();
        }
    }
    
    // Handle serial input
    if (_serial->available()) {
        static String inputLine;
        while (_serial->available()) {
            char c = _serial->read();
            if (c == '\n' || c == '\r') {
                if (inputLine.length() > 0) {
                    _lastActivity = currentMillis;
                    if (_menuSystem.isInMenu() && inputLine.length() == 1) {
                        _menuSystem.handleMenuInput(inputLine[0]);
                    } else {
                        // Special handling for exit command
                        if (inputLine.equalsIgnoreCase("exit")) {
                            _commandHandler.executeCommand(inputLine, _wifiManager.getStatus().connected);
                        } else {
                            _commandHandler.executeCommand(inputLine, _wifiManager.getStatus().connected);
                        }
                    }
                    inputLine = "";
                }
            } else if (c == 8 || c == 127) { // Backspace
                if (inputLine.length() > 0) {
                    inputLine.remove(inputLine.length() - 1);
                    _serial->print("\b \b");
                }
            } else if (c >= 32 && c <= 126) { // Printable characters
                inputLine += c;
                _serial->print(c);
            }
        }
    }
    
    _menuSystem.process();
    
    // Handle inactivity timeout - return to command mode from menu
    if (currentMillis - _lastActivity > _timeout && _menuSystem.isInMenu()) {
        _menuSystem.exitMenu();
        _serial->println("\nInactivity timeout. Returned to command mode.");
    }
}

void ESP32_SIH::registerCommand(const String& command, 
                             CommandHandler::CommandCallback handler,
                             const String& description,
                             const String& usage,
                             const String& category,
                             bool requiresWiFi) {
    _commandHandler.registerCommand(command, handler, description, usage, category);
}

void ESP32_SIH::setTimeout(unsigned long timeoutMs) {
    _timeout = timeoutMs;
    _menuSystem.setTimeout(timeoutMs);
}

bool ESP32_SIH::isConnected() const {
    auto status = _wifiManager.getStatus();
    return status.connected;
}

String ESP32_SIH::getConnectedSSID() const {
    auto status = _wifiManager.getStatus();
    return status.ssid;
}

String ESP32_SIH::getMACAddress() const {
    return _wifiManager.getMACAddress();
}

void ESP32_SIH::showSystemStatus() {
    auto info = _systemMonitor.getSystemInfo();
    auto btInfo = _systemMonitor.getBluetoothInfo();
    auto wifiInfo = _systemMonitor.getWiFiInfo();
    
    _serial->println("\n=== System Status ===");
    _serial->printf("Uptime: %lu seconds\n", info.uptime);
    _serial->printf("Free Heap: %u bytes\n", info.freeHeap);
    _serial->printf("Min Free Heap: %u bytes\n", info.minFreeHeap);
    _serial->printf("Max Alloc Heap: %u bytes\n", info.maxAllocHeap);
    
    _serial->printf("\nChip: ESP32, Cores: %d, Revision: %d\n", info.chipCores, info.chipRevision);
    _serial->printf("Flash: %d MB, PSRAM: %s (%d MB)\n", info.flashSize, 
                   info.hasPSRAM ? "Yes" : "No", info.psrSize);
    
    _serial->printf("\nBluetooth: %s, MAC: %s\n", btInfo.isEnabled ? "Enabled" : "Disabled", btInfo.macAddress.c_str());
    
    _serial->printf("\nWiFi: %s\n", wifiInfo.isConnected ? "Connected" : "Disconnected");
    if (wifiInfo.isConnected) {
        _serial->printf("Network: %s, IP: %s\n", wifiInfo.ssid.c_str(), wifiInfo.ipAddress.c_str());
        _serial->printf("Signal: %d dBm, MAC: %s\n", wifiInfo.rssi, wifiInfo.macAddress.c_str());
    }
    
    _serial->printf("\nStored Networks: %d\n", _wifiManager.listStoredCredentials().size());
    
    _serial->printf("\nAuto-reconnect: %s\n", _storage.isAutoReconnectEnabled() ? "Enabled" : "Disabled");
    if (_storage.isAutoReconnectEnabled()) {
        _serial->printf("Max Attempts: %d, Timeout: %lu minutes\n", 
                      _storage.getMaxReconnectAttempts(), 
                      _storage.getReconnectTimeout() / (60 * 1000));
    }
}

void ESP32_SIH::showSystemInfo() {
    auto info = _systemMonitor.getSystemInfo();
    _serial->println("\n=== System Information ===");
    _serial->printf("Uptime: %lu seconds\n", info.uptime);
    _serial->printf("SDK Version: %s\n", info.sdkVersion.c_str());
    _serial->printf("Core Version: %s\n", info.coreVersion.c_str());
    _serial->printf("Free Heap: %u bytes\n", info.freeHeap);
    _serial->printf("Minimum Free Heap: %u bytes\n", info.minFreeHeap);
    _serial->printf("Maximum Allocation: %u bytes\n", info.maxAllocHeap);
}

void ESP32_SIH::showChipInfo() {
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    
    _serial->println("\n=== Chip Information ===");
    _serial->printf("Model: ESP32\n");
    _serial->printf("CPU Cores: %d\n", chip_info.cores);
    _serial->printf("Revision: %d\n", chip_info.revision);
    
    uint32_t flash_size;
    esp_flash_get_size(NULL, &flash_size);
    _serial->printf("Flash Size: %d MB\n", flash_size / (1024 * 1024));
    
    _serial->printf("PSRAM: %s\n", psramFound() ? "Available" : "Not available");
    if (psramFound()) {
        _serial->printf("PSRAM Size: %u bytes\n", ESP.getPsramSize());
    }
}

void ESP32_SIH::showMemoryInfo() {
    auto info = _systemMonitor.getSystemInfo();
    
    _serial->println("\n=== Memory Information ===");
    _serial->printf("Free Heap: %u bytes\n", info.freeHeap);
    _serial->printf("Minimum Free Heap: %u bytes\n", info.minFreeHeap);
    _serial->printf("Maximum Allocation: %u bytes\n", info.maxAllocHeap);
    
    if (info.hasPSRAM) {
        _serial->printf("PSRAM Total: %u bytes\n", ESP.getPsramSize());
        _serial->printf("PSRAM Free: %u bytes\n", ESP.getFreePsram());
    }
    
    float fragmentation = 100.0f - (100.0f * info.maxAllocHeap / info.freeHeap);
    _serial->printf("Heap Fragmentation: %.2f%%\n", fragmentation);
}

void ESP32_SIH::showBluetoothInfo() {
    auto btInfo = _systemMonitor.getBluetoothInfo();
    
    _serial->println("\n=== Bluetooth Information ===");
    _serial->printf("Status: %s\n", btInfo.isEnabled ? "Enabled" : "Disabled");
    _serial->printf("MAC Address: %s\n", btInfo.macAddress.c_str());
    
    if (!btInfo.isEnabled) {
        _serial->println("\nNote: Bluetooth must be enabled in your firmware setup() function");
        _serial->println("Example:");
        _serial->println("  #include <BluetoothSerial.h>");
        _serial->println("  BluetoothSerial SerialBT;");
        _serial->println("  SerialBT.begin(\"ESP32_BT\");");
    }
}

String ESP32_SIH::getUserInput(const String& prompt) {
    return _menuSystem.getUserInput(prompt, false);
}