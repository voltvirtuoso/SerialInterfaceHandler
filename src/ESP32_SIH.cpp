/**
 * @file ESP32_SIH.cpp
 * @brief ESP32 Serial Interface Handler - Core Implementation
 * 
 * This implementation file contains all method definitions for the ESP32_SIH
 * system classes. It provides the complete functionality for WiFi management,
 * NVS storage operations, serial interface handling, menu system navigation,
 * and system monitoring.
 * 
 * @author ESP32_SIH Development Team
 * @version 1.0.0
 * @date December 2025
 * 
 * @copyright MIT License
 */
#if defined(ESP32)

#include "ESP32_SIH.h"

// WiFiScanResult implementation (unchanged)
bool WiFiScanResult::isOpen() const {
    return encryptionType == WIFI_AUTH_OPEN;
}

// NetworkCredential implementation (unchanged)
NetworkCredential::NetworkCredential() {
    memset(ssid, 0, sizeof(ssid));
    memset(password, 0, sizeof(password));
    isValid = false;
    lastConnected = 0;
    connectionAttempts = 0;
    successfulConnections = 0;
}

void NetworkCredential::set(const String& ssidStr, const String& passwordStr) {
    ssidStr.toCharArray(ssid, SSID_MAX_LENGTH + 1);
    passwordStr.toCharArray(password, PASSWORD_MAX_LENGTH + 1);
    isValid = true;
}

String NetworkCredential::getSSID() const {
    return String(ssid);
}

String NetworkCredential::getPassword() const {
    return String(password);
}

// NVSStorage implementation (replaces EEPROMStorage)
NVSStorage::NVSStorage() : _initialized(false), _networkCount(0) {}

bool NVSStorage::begin() {
    if (_initialized) return true;
    
    bool success = _prefs.begin(NVS_NAMESPACE, false);
    if (!success) {
        Serial.println("NVS init failed, attempting reset");
        _prefs.end();
        delay(100);
        success = _prefs.begin(NVS_NAMESPACE, true); // Read-only mode
        if (!success) {
            Serial.println("NVS reset failed, creating new namespace");
            _prefs.begin(NVS_NAMESPACE, false);
        }
    }
    
    loadHeader();
    loadNetworks();
    _initialized = true;
    return true;
}

void NVSStorage::loadHeader() {
    _networkCount = _prefs.getUChar("network_count", 0);
    if (_networkCount > MAX_NETWORKS) {
        _networkCount = 0;
        clearAllNetworks();
    }
}

void NVSStorage::saveHeader() {
    _prefs.putUChar("network_count", _networkCount);
}

void NVSStorage::loadNetworks() {
    for (uint8_t i = 0; i < _networkCount && i < MAX_NETWORKS; i++) {
        char key[20];
        snprintf(key, sizeof(key), "network_%d", i);
        size_t size = sizeof(NetworkCredential);
        if (_prefs.getBytes(key, &_networks[i], size) != size) {
            memset(&_networks[i], 0, size);
        }
    }
}

void NVSStorage::saveNetworks() {
    for (uint8_t i = 0; i < _networkCount && i < MAX_NETWORKS; i++) {
        char key[20];
        snprintf(key, sizeof(key), "network_%d", i);
        _prefs.putBytes(key, &_networks[i], sizeof(NetworkCredential));
    }
    saveHeader();
}

int NVSStorage::findNetworkIndex(const String& ssid) const {
    for (uint8_t i = 0; i < _networkCount; i++) {
        if (_networks[i].isValid && ssid.equals(_networks[i].getSSID())) {
            return i;
        }
    }
    return -1;
}

void NVSStorage::removeLeastSuccessfulNetwork() {
    if (_networkCount == 0) return;
    
    uint8_t leastSuccessfulIndex = 0;
    uint32_t minSuccess = _networks[0].successfulConnections;
    
    for (uint8_t i = 1; i < _networkCount; i++) {
        if (_networks[i].successfulConnections < minSuccess) {
            minSuccess = _networks[i].successfulConnections;
            leastSuccessfulIndex = i;
        }
    }
    
    // Shift remaining networks down
    for (uint8_t i = leastSuccessfulIndex; i < _networkCount - 1; i++) {
        _networks[i] = _networks[i + 1];
    }
    
    _networkCount--;
    memset(&_networks[_networkCount], 0, sizeof(NetworkCredential));
}

bool NVSStorage::saveNetwork(const String& ssid, const String& password) {
    if (!_initialized) return false;
    
    // Check if network already exists
    int index = findNetworkIndex(ssid);
    if (index >= 0) {
        // Update existing network
        _networks[index].set(ssid, password);
        _networks[index].connectionAttempts = 0;
        _networks[index].successfulConnections++;
        _networks[index].lastConnected = millis();
    } else {
        // Add new network
        if (_networkCount >= MAX_NETWORKS) {
            // Remove least successful network
            removeLeastSuccessfulNetwork();
        }
        index = _networkCount++;
        _networks[index].set(ssid, password);
        _networks[index].connectionAttempts = 0;
        _networks[index].successfulConnections = 1;
        _networks[index].lastConnected = millis();
    }
    
    saveNetworks();
    return true;
}

bool NVSStorage::getNetwork(const String& ssid, NetworkCredential& credential) {
    if (!_initialized) return false;
    int index = findNetworkIndex(ssid);
    if (index >= 0) {
        credential = _networks[index];
        return true;
    }
    return false;
}

bool NVSStorage::removeNetwork(const String& ssid) {
    if (!_initialized) return false;
    int index = findNetworkIndex(ssid);
    if (index < 0) return false;
    
    // Shift remaining networks down
    for (uint8_t i = index; i < _networkCount - 1; i++) {
        _networks[i] = _networks[i + 1];
    }
    
    _networkCount--;
    memset(&_networks[_networkCount], 0, sizeof(NetworkCredential));
    saveNetworks();
    return true;
}

void NVSStorage::clearAllNetworks() {
    if (!_initialized) return;
    
    // Clear all network entries
    for (uint8_t i = 0; i < MAX_NETWORKS; i++) {
        char key[20];
        snprintf(key, sizeof(key), "network_%d", i);
        _prefs.remove(key);
    }
    
    _networkCount = 0;
    memset(_networks, 0, sizeof(_networks));
    saveHeader();
}

std::vector<NetworkCredential> NVSStorage::getAllNetworks() const {
    std::vector<NetworkCredential> result;
    for (uint8_t i = 0; i < _networkCount; i++) {
        if (_networks[i].isValid) {
            result.push_back(_networks[i]);
        }
    }
    return result;
}

size_t NVSStorage::getNetworkCount() const {
    return _networkCount;
}

void NVSStorage::incrementConnectionAttempts(const String& ssid) {
    int index = findNetworkIndex(ssid);
    if (index >= 0) {
        _networks[index].connectionAttempts++;
        _networks[index].lastConnected = millis();
        saveNetworks();
    }
}

void NVSStorage::markConnectionSuccessful(const String& ssid) {
    int index = findNetworkIndex(ssid);
    if (index >= 0) {
        _networks[index].successfulConnections++;
        _networks[index].lastConnected = millis();
        saveNetworks();
    }
}

// CommandHandler Implementation (unchanged)
bool CommandHandler::registerCommand(const String& command,
                                     CommandCallback callback,
                                     const String& description,
                                     const String& usage,
                                     const String& category) {
    CommandInfo info{description, usage, category};
    _commands[command] = std::make_pair(callback, info);
    return true;
}

bool CommandHandler::executeCommand(const String& commandLine) {
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
        _serial->println("\n=== ESP8266 Serial Interface System Help ===");
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
    _serial->println("creds    - Manage stored credentials");
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

bool WiFiManager::scanNetworks(std::vector<WiFiScanResult>& results) {
    int n = WiFi.scanNetworks();
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
        result.isHidden = false; // ESP32 doesn't provide hidden network info
        results.push_back(result);
    }
    return true;
}

bool WiFiManager::connect(const String& ssid, const String& password, bool saveCredentials) {
    // Ensure WiFi is properly set up
    setupWiFi();
    
    Serial.printf("Attempting to connect to: %s\n", ssid.c_str());
    _storage.incrementConnectionAttempts(ssid);
    
    WiFi.begin(ssid.c_str(), password.c_str());
    
    unsigned long startTime = millis();
    while (millis() - startTime < 15000) { // 15 second timeout
        int status = WiFi.status();
        if (status == WL_CONNECTED) {
            _connected = true;
            _connectedSSID = ssid;
            
            // Only save credentials if connection is successful
            if (saveCredentials) {
                _storage.saveNetwork(ssid, password);
                _storage.markConnectionSuccessful(ssid);
            }
            
            Serial.printf("✓ Connected to %s successfully\n", ssid.c_str());
            Serial.printf("IP Address: %s\n", WiFi.localIP().toString().c_str());
            return true;
        }
        delay(200);
    }
    
    _connected = false;
    Serial.printf("✗ Failed to connect to %s (status: %d)\n", ssid.c_str(), WiFi.status());
    return false;
}

void WiFiManager::disconnect() {
    WiFi.disconnect();
    delay(100);
    _connected = false;
    _connectedSSID = "";
}

bool WiFiManager::autoConnect() {
    if (!_storage.begin()) {
        Serial.println("EEPROM storage not initialized");
        return false;
    }
    
    auto networks = _storage.getAllNetworks();
    if (networks.empty()) {
        Serial.println("No saved WiFi credentials found");
        return false;
    }
    
    // Sort networks by priority (successful connections, last connected time)
    sortNetworksByPriority(networks);
    
    Serial.println("Attempting auto-connect to stored networks...");
    
    for (const auto& network : networks) {
        if (!network.isValid) continue;
        
        String ssid = network.getSSID();
        String password = network.getPassword();
        
        Serial.printf("Trying network: %s\n", ssid.c_str());
        
        if (connect(ssid, password, false)) {
            return true;
        }
    }
    
    Serial.println("All auto-connect attempts failed");
    return false;
}

void WiFiManager::sortNetworksByPriority(std::vector<NetworkCredential>& networks) {
    std::sort(networks.begin(), networks.end(), 
        [](const NetworkCredential& a, const NetworkCredential& b) {
            // Primary: successful connections
            if (a.successfulConnections != b.successfulConnections) {
                return a.successfulConnections > b.successfulConnections;
            }
            // Secondary: last connected time (more recent first)
            return a.lastConnected > b.lastConnected;
        });
}

bool WiFiManager::verifyConnection() {
    if (!_connected) return false;
    return WiFi.status() == WL_CONNECTED;
}

bool WiFiManager::clearAllCredentials() {
    _storage.clearAllNetworks();
    WiFi.disconnect();
    delay(100);
    _connected = false;
    _connectedSSID = "";
    Serial.println("✓ All WiFi credentials cleared successfully");
    return true;
}

std::vector<NetworkCredential> WiFiManager::listStoredCredentials() const {
    return _storage.getAllNetworks();
}

bool WiFiManager::removeCredential(const String& ssid) {
    return _storage.removeNetwork(ssid);
}

WiFiManager::ConnectionStatus WiFiManager::getStatus() const {
    ConnectionStatus status;
    status.connected = _connected && (WiFi.status() == WL_CONNECTED);
    status.ssid = _connectedSSID;
    status.ip = status.connected ? WiFi.localIP().toString() : "0.0.0.0";
    status.rssi = status.connected ? WiFi.RSSI() : 0;
    return status;
}

bool WiFiManager::isStorageInitialized() {
    return _storage.begin();
}

// MenuSystem Implementation
MenuSystem::MenuSystem(HardwareSerial* serial, CommandHandler* commandHandler,
                     WiFiManager* wifiManager)
    : _serial(serial), _commandHandler(commandHandler), 
      _wifiManager(wifiManager), _currentMenu(MAIN), _inMenuMode(false),
      _lastActivity(0), _menuTimeout(60000) {
    initializeMenus();
}

void MenuSystem::initializeMenus() {
    _mainMenuItems = {
        {1, "WiFi Configuration", [this]() { showMenu(WIFI); }, "Configure WiFi networks", true},
        {2, "Credentials Management", [this]() { showMenu(CREDENTIALS); }, "Manage stored networks", true},
        {3, "System Status", [this]() { showSystemStatus(); }, "Show system information", true},
        {4, "Exit Menu", [this]() { exitMenu(); }, "Return to command mode", true}
    };
    
    _wifiMenuItems = {
        {1, "Scan Networks", [this]() { scanNetworks(); }, "Scan for available networks", true},
        {2, "Connect to Network", [this]() { connectToNetwork(); }, "Connect to WiFi network", true},
        {3, "Auto Connect", [this]() { 
            _serial->println("Attempting auto-connect to stored networks...");
            if (_wifiManager->autoConnect()) {
                _serial->println("✓ Auto-connect successful!");
            } else {
                _serial->println("✗ Auto-connect failed");
            }
            delay(2000);
            showMenu(WIFI);
        }, "Auto-connect to stored networks", true},
        {4, "Check Connection", [this]() { checkConnection(); }, "Check current WiFi status", true},
        {5, "Back to Main Menu", [this]() { showMenu(MAIN); }, "Return to main menu", true}
    };
    
    _credentialsMenuItems = {
        {1, "List Stored Networks", [this]() { listStoredCredentials(); }, "Show all stored credentials", true},
        {2, "Remove Network", [this]() { removeStoredCredential(); }, "Remove a specific network", true},
        {3, "Clear All Networks", [this]() { clearAllCredentials(); }, "Clear all stored credentials", true},
        {4, "Back to Main Menu", [this]() { showMenu(MAIN); }, "Return to main menu", true}
    };
}

void MenuSystem::showMenu(MenuType type) {
    _currentMenu = type;
    _inMenuMode = true;
    _lastActivity = millis();
    
    clearScreen();
    showHeader(getMenuTitle(type));
    
    std::vector<MenuItem>* items = nullptr;
    switch (type) {
        case MAIN: items = &_mainMenuItems; break;
        case WIFI: items = &_wifiMenuItems; break;
        case CREDENTIALS: items = &_credentialsMenuItems; break;
        default: items = &_mainMenuItems; break;
    }
    
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
        exitMenu();
        return;
    }
    
    int selection = inputChar - '0';
    if (selection < 1) return;
    
    std::vector<MenuItem>* items = nullptr;
    switch (_currentMenu) {
        case MAIN: items = &_mainMenuItems; break;
        case WIFI: items = &_wifiMenuItems; break;
        case CREDENTIALS: items = &_credentialsMenuItems; break;
        default: items = &_mainMenuItems; break;
    }
    
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
        _serial->println("================================");
        for (size_t i = 0; i < results.size(); i++) {
            _serial->printf("%d. %-20s RSSI: %-4d %s\n", 
                          i + 1, results[i].ssid.c_str(), results[i].rssi,
                          results[i].isOpen() ? "[OPEN]" : "[SECURED]");
        }
        _serial->println("================================");
        
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
    if (_wifiManager->isStorageInitialized() && _wifiManager->listStoredCredentials().size() > 0) {
        for (const auto& cred : _wifiManager->listStoredCredentials()) {
            if (cred.getSSID() == ssid) {
                savedCredential = cred;
                break;
            }
        }
    }
    
    String password;
    
    if (!savedCredential.ssid[0]) {
        password = getUserInput("Enter WiFi password (leave blank for open network)", true);
    } else {
        _serial->printf("\nFound saved credentials for %s\n", ssid.c_str());
        _serial->printf("Connection stats: %lu attempts, %lu successful\n", 
                       savedCredential.connectionAttempts, savedCredential.successfulConnections);
        String useSaved = getUserInput("Use saved password? (y/n) [y]", false);
        if (useSaved.isEmpty() || useSaved.equalsIgnoreCase("y")) {
            password = savedCredential.getPassword();
            _serial->println("Using saved password...");
        } else {
            password = getUserInput("Enter WiFi password (leave blank for open network)", true);
        }
    }
    
    _serial->printf("\nConnecting to %s...\n", ssid.c_str());
    
    // Only save credentials if connection succeeds
    if (_wifiManager->connect(ssid, password, true)) {
        _serial->println("✓ Connection successful!");
    } else {
        _serial->println("✗ Connection failed!");
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
    } else {
        _serial->println("✗ Not connected to WiFi");
        auto networks = _wifiManager->listStoredCredentials();
        if (!networks.empty()) {
            _serial->printf("Stored networks: %d\n", networks.size());
            for (const auto& net : networks) {
                _serial->printf("- %s (%lu successful)\n", net.getSSID().c_str(), net.successfulConnections);
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
        _serial->println("================================");
        
        for (size_t i = 0; i < networks.size(); i++) {
            const auto& net = networks[i];
            _serial->printf("%d. SSID: %s\n", i + 1, net.getSSID().c_str());
            _serial->printf("   Attempts: %lu, Successful: %lu\n", 
                          net.connectionAttempts, net.successfulConnections);
            _serial->printf("   Last connected: %lu ms ago\n", 
                          millis() - net.lastConnected);
            _serial->println("--------------------------------");
        }
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
        _serial->printf("%d. %s\n", i + 1, networks[i].getSSID().c_str());
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

void MenuSystem::clearAllCredentials() {
    clearScreen();
    showHeader("Clear All Credentials");
    
    _serial->println("This will remove ALL stored WiFi credentials from EEPROM.");
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

void MenuSystem::showSystemStatus() {
    
    clearScreen();
    showHeader("System Status");
    _serial->printf("System Uptime: %lu seconds\n", millis() / 1000);
    _serial->printf("Free Heap: %u bytes\n", ESP.getFreeHeap());
    _serial->printf("Chip Model: ESP32\n");
    _serial->println("Storage: NVS (Preferences)"); // Fixed line
    _serial->print("MAC Address: ");
    _serial->println(WiFi.macAddress());
    
    auto wifiStatus = _wifiManager->getStatus();
    _serial->printf("\nWiFi Status: %s\n", wifiStatus.connected ? "Connected" : "Disconnected");
    if (wifiStatus.connected) {
        _serial->printf("Connected to: %s\n", wifiStatus.ssid.c_str());
        _serial->printf("IP Address: %s\n", wifiStatus.ip.c_str());
        _serial->printf("Signal Strength: %d dBm\n", wifiStatus.rssi);
    }
    
    auto networks = _wifiManager->listStoredCredentials();
    _serial->printf("\nStored Networks: %d\n", networks.size());
    
    delay(3000);
    showMenu(MAIN);
}

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
        
        if (millis() - lastInput > 30000) { // 30 second timeout
            _serial->println("\nInput timeout");
            return "";
        }
        
        delay(10);
    }
}

int MenuSystem::getNumericInput(const String& prompt, int minVal, int maxVal) {
    while (true) {
        String input = getUserInput(prompt);
        if (input.isEmpty()) continue;
        
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
	for(uint8_t i=0; i < 5; i++)
		_serial->println();
}

void MenuSystem::showHeader(const String& title) {
    _serial->println("================================");
    _serial->printf("  %s\n", title.c_str());
    _serial->println("================================");
}

void MenuSystem::showFooter() {
    _serial->println("================================");
    _serial->printf("  Timeout: %lu seconds\n", (_menuTimeout - (millis() - _lastActivity)) / 1000);
    _serial->println("  Press '0' to exit menu");
    _serial->println("================================");
}

String MenuSystem::getMenuTitle(MenuType type) const {
    switch (type) {
        case MAIN: return "Main Menu";
        case WIFI: return "WiFi Configuration";
        case SYSTEM: return "System Management";
        case CREDENTIALS: return "Credentials Management";
        default: return "Menu";
    }
}

bool MenuSystem::isInMenu() const {
    return _inMenuMode;
}


// Add this to WiFiManager class implementation
void WiFiManager::setupWiFi() {
    WiFi.mode(WIFI_STA);
    WiFi.setTxPower(WIFI_POWER_19_5dBm); // Optimal power for ESP32
    WiFi.setHostname("ESP32-SIH");
    WiFi.setAutoReconnect(true);
    // WiFi.setAutoConnect(true);
    WiFi.disconnect();
    delay(100);
}

WiFiManager::WiFiManager() : _connected(false), _lastReconnectAttempt(0) {}

bool WiFiManager::begin() {
    if (!_storage.begin()) {
        return false;
    }
    // Setup WiFi hardware properly
    setupWiFi();
    return true;
}

// ESP32_SIH Implementation
ESP32_SIH::ESP32_SIH(HardwareSerial* serial)
    : _serial(serial), _helpSystem(&_commandHandler, serial),
      _menuSystem(serial, &_commandHandler, &_wifiManager),
      _lastActivity(0), _timeout(30000), _initialized(false),
      _lastReconnectCheck(0), _lastWatchdogCheck(0) {}

bool ESP32_SIH::begin(uint32_t baudRate) {
    _serial->begin(baudRate);
    delay(100);
    
    if (!_wifiManager.begin()) {
        _serial->println("WiFiManager initialization failed");
        return false;
    }
    
    // Attempt auto-reconnect immediately after initialization
    autoReconnectWiFi();
    
    // Register default commands
    registerCommand("help", [this](const std::vector<String>& args) {
        if (args.size() > 0) {
            _helpSystem.showHelp(args[0]);
        } else {
            _helpSystem.showHelp();
        }
    }, "Show help information", "help [command]", "system");
    
    registerCommand("?", [this](const std::vector<String>& args) {
        _helpSystem.showQuickHelp();
    }, "Show quick help summary", "?", "system");
    
    registerCommand("menu", [this](const std::vector<String>& args) {
        _menuSystem.showMenu(MenuSystem::MAIN);
    }, "Show main menu", "menu", "system");
    
    registerCommand("status", [this](const std::vector<String>& args) {
        auto wifiStatus = _wifiManager.getStatus();
        _serial->printf("System Uptime: %lu seconds\n", millis() / 1000);
        _serial->printf("Free Heap: %u bytes\n", ESP.getFreeHeap());
        _serial->printf("Chip Model: ESP32\n");
        _serial->printf("NVS Storage: wifi_creds namespace\n");
        _serial->printf("WiFi Status: %s\n", wifiStatus.connected ? "Connected" : "Disconnected");
        _serial->print("MAC Address: ");
        _serial->println(WiFi.macAddress());
        if (wifiStatus.connected) {
            _serial->printf("Connected to: %s\n", wifiStatus.ssid.c_str());
            _serial->printf("IP Address: %s\n", wifiStatus.ip.c_str());
            _serial->printf("Signal Strength: %d dBm\n", wifiStatus.rssi);
            _serial->printf("WiFi Channel: %d\n", WiFi.channel());
        }
        
        auto networks = _wifiManager.listStoredCredentials();
        _serial->printf("\nStored Networks: %d\n", networks.size());
        for (const auto& net : networks) {
            _serial->printf("- %s (Success: %lu)\n", net.getSSID().c_str(), net.successfulConnections);
        }
    }, "Show system status", "status", "system");
    
    registerCommand("wifi", [this](const std::vector<String>& args) {
        if (args.size() > 0) {
            if (args[0] == "scan") {
                std::vector<WiFiScanResult> results;
                if (_wifiManager.scanNetworks(results)) {
                    _serial->println("Available networks:");
                    for (size_t i = 0; i < results.size(); i++) {
                        _serial->printf("%d. %s (RSSI: %d, %s)\n", i + 1, 
                                      results[i].ssid.c_str(), results[i].rssi,
                                      results[i].isOpen() ? "Open" : "Secured");
                    }
                } else {
                    _serial->println("Scan failed");
                }
            } else if (args[0] == "connect" && args.size() > 1) {
                String ssid = args[1];
                String password = (args.size() > 2) ? args[2] : "";
                if (_wifiManager.connect(ssid, password, true)) {
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
                    _serial->printf("RSSI: %d dBm\n", status.rssi);
                }
            } else if (args[0] == "auto") {
                if (_wifiManager.autoConnect()) {
                    _serial->println("Auto-connect successful");
                } else {
                    _serial->println("Auto-connect failed");
                }
            } else if (args[0] == "disconnect") {
                _wifiManager.disconnect(); // Use the proper disconnect method
                _serial->println("Disconnected from WiFi");
            }
        } else {
            _serial->println("WiFi commands: scan, connect <ssid> <password>, status, auto, disconnect");
        }
    }, "WiFi management commands", "wifi <command>", "wifi");
    
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
                        _serial->printf("   Success: %lu, Attempts: %lu\n", 
                                      net.successfulConnections, net.connectionAttempts);
                        _serial->printf("   Last connected: %lu ms ago\n",
                                      millis() - net.lastConnected);
                    }
                }
            } else if (args[0] == "remove" && args.size() > 1) {
                if (_wifiManager.removeCredential(args[1])) {
                    _serial->println("✓ Network removed");
                } else {
                    _serial->println("✗ Network not found");
                }
            } else if (args[0] == "clear") {
                if (_wifiManager.clearAllCredentials()) {
                    _serial->println("✓ All credentials cleared");
                }
            } else if (args[0] == "stats") {
                _serial->println("NVS Storage Statistics:");
                // ESP32-specific NVS stats
                _serial->printf("Namespace: %s\n", NVS_NAMESPACE);
            }
        } else {
            _serial->println("Credentials commands: list, remove <ssid>, clear, stats");
        }
    }, "Manage stored credentials", "creds <command>", "wifi");
    
    registerCommand("reconnect", [this](const std::vector<String>& args) {
        _serial->println("Attempting to reconnect to stored networks...");
        autoReconnectWiFi();
        auto status = _wifiManager.getStatus();
        if (status.connected) {
            _serial->printf("✓ Reconnected to %s\n", status.ssid.c_str());
            _serial->printf("IP: %s\n", status.ip.c_str());
            _serial->printf("RSSI: %d dBm\n", status.rssi);
        } else {
            _serial->println("✗ Reconnection failed");
            _serial->println("Type 'menu' to configure WiFi manually");
            _serial->printf("Available stored networks: %d\n", _wifiManager.listStoredCredentials().size());
        }
    }, "Reconnect to stored networks", "reconnect", "system");
    
    registerCommand("system", [this](const std::vector<String>& args) {
        if (args.size() > 0) {
            if (args[0] == "info") {
                _serial->printf("ESP32 Chip ID: %04X%08X\n", 
                            (uint16_t)(ESP.getEfuseMac() >> 32), 
                            (uint32_t)ESP.getEfuseMac());
                _serial->printf("Flash Size: %u MB\n", ESP.getFlashChipSize() / (1024 * 1024));
                _serial->printf("PSRAM: %s\n", ESP.getPsramSize() > 0 ? "Available" : "Not available");
                _serial->printf("Cores: 2 (WiFi on core %d)\n", xPortGetCoreID());
            } else if (args[0] == "reset") {
                _serial->println("System reset initiated...");
                delay(500);
                ESP.restart();
            }
        } else {
            _serial->println("System commands: info, reset");
        }
    }, "System management commands", "system <command>", "system");
    
    _initialized = true;
    _lastActivity = millis();
    _lastReconnectCheck = millis();
    _lastWatchdogCheck = millis();
    
    _serial->println("\nESP32 Serial Interface System initialized");
    _serial->println("Type 'help' for available commands");
    _serial->println("Type 'menu' to access configuration menu");
    
    // Show connection status after initialization
    auto status = _wifiManager.getStatus();
    if (status.connected) {
        _serial->printf("\n✓ Connected to: %s\n", status.ssid.c_str());
        _serial->printf("IP Address: %s\n", status.ip.c_str());
        _serial->printf("Signal Strength: %d dBm\n", status.rssi);
    } else {
        _serial->println("\n✗ No active WiFi connection");
        _serial->println("Type 'menu' to configure WiFi");
        _serial->printf("Stored networks: %d\n", _wifiManager.listStoredCredentials().size());
        
        // ESP32-specific: Scan for networks on startup if not connected
        if (_wifiManager.listStoredCredentials().empty()) {
            _serial->println("No saved networks found. Scanning for available networks...");
            std::vector<WiFiScanResult> results;
            if (_wifiManager.scanNetworks(results) && !results.empty()) {
                _serial->println("Available networks:");
                for (size_t i = 0; i < results.size() && i < 5; i++) {
                    _serial->printf("%d. %s (RSSI: %d, %s)\n", i + 1,
                                  results[i].ssid.c_str(), results[i].rssi,
                                  results[i].isOpen() ? "Open" : "Secured");
                }
                if (results.size() > 5) {
                    _serial->printf("... and %d more networks\n", results.size() - 5);
                }
            }
        }
    }
    
    return true;
}

void ESP32_SIH::autoReconnectWiFi() {
    auto status = _wifiManager.getStatus();
    
    // Only attempt reconnection if not currently connected
    if (!status.connected) {
        Serial.println("Attempting auto-reconnection...");
        bool success = _wifiManager.autoConnect();
        
        if (success) {
            status = _wifiManager.getStatus();
            Serial.printf("✓ Auto-reconnection successful to: %s\n", status.ssid.c_str());
            Serial.printf("IP: %s\n", status.ip.c_str());
            Serial.printf("RSSI: %d dBm\n", status.rssi);
        } else {
            Serial.println("✗ Auto-reconnection failed");
            
            // ESP32-specific: Try to scan for networks if auto-connect fails
            Serial.println("Scanning for available networks...");
            std::vector<WiFiScanResult> results;
            if (_wifiManager.scanNetworks(results)) {
                Serial.printf("Found %d networks\n", results.size());
            }
        }
    }
}

void ESP32_SIH::process() {
    if (!_initialized) return;
    
    // Periodically check and attempt reconnection if disconnected
    if (millis() - _lastReconnectCheck > 30000) { // Check every 30 seconds
        _lastReconnectCheck = millis();
        auto status = _wifiManager.getStatus();
        if (!status.connected) {
            Serial.println("Connection lost, attempting reconnection...");
            autoReconnectWiFi();
        }
    }
    
    // Watchdog protection - ensure WiFi stays connected
    if (millis() - _lastWatchdogCheck > 60000) { // Every 60 seconds
        _lastWatchdogCheck = millis();
        auto status = _wifiManager.getStatus();
        if (!status.connected) {
            Serial.println("Watchdog: Connection lost, forcing reconnection...");
            autoReconnectWiFi();
        }
    }
    
    // ESP32-specific: Monitor WiFi events
    if (WiFi.status() != WL_CONNECTED && millis() - _lastReconnectCheck > 5000) {
        // If we've been disconnected for more than 5 seconds, trigger reconnection check
        _lastReconnectCheck = millis() - 25000; // Force reconnection check soon
    }
    
    if (_serial->available()) {
        static String inputLine;
        while (_serial->available()) {
            char c = _serial->read();
            if (c == '\n' || c == '\r') {
                if (inputLine.length() > 0) {
                    _lastActivity = millis();
                    
                    if (_menuSystem.isInMenu() && inputLine.length() == 1) {
                        _menuSystem.handleMenuInput(inputLine[0]);
                    } else {
                        _commandHandler.executeCommand(inputLine);
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
    
    if (millis() - _lastActivity > _timeout && _menuSystem.isInMenu()) {
        _menuSystem.exitMenu();
        _serial->println("\nInactivity timeout. Returned to command mode.");
    }
}

void ESP32_SIH::registerCommand(const String& command, 
                             CommandHandler::CommandCallback handler,
                             const String& description,
                             const String& usage,
                             const String& category) {
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

#endif 