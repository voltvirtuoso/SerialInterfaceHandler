# Serial Interface Handler (SIH)

A comprehensive serial interface handler for ESP8266 and ESP32 that provides WiFi management, credential storage, and an interactive command system with menu navigation.

## Table of Contents
- [Features](#features)
- [Installation](#installation)
- [Getting Started](#getting-started)
- [Basic Usage](#basic-usage)
- [Available Commands](#available-commands)
- [Adding Custom Commands](#adding-custom-commands)
- [Creating New Menu Options](#creating-new-menu-options)
- [Configuration Options](#configuration-options)
- [Platform Support](#platform-support)
- [Examples](#examples)
- [API Documentation](#api-documentation)
- [Contributing](#contributing)
- [License](#license)

## Features

- **Unified Interface**: Works seamlessly on both ESP8266 and ESP32 platforms with automatic platform detection. 
- **WiFi Management**: Scan, connect, auto-connect, and manage WiFi networks with credential storage.
- **Interactive Menus**: User-friendly menu system with timeout protection and navigation.
- **Command System**: Extensible command handler with categories, descriptions, and usage information.
- **Help System**: Comprehensive help documentation with context-sensitive assistance.
- **Auto-Reconnect**: Intelligent reconnection logic that prioritizes successful networks.
- **Credential Storage**: Secure storage of WiFi credentials with success tracking (EEPROM for ESP8266, NVS for ESP32).
- **System Monitoring**: Real-time status monitoring with watchdog protection.
- **Timeout Management**: Inactivity detection with automatic return to command mode.

## Installation

### Using Arduino Library Manager
1. Open the Arduino IDE
2. Navigate to **Sketch** > **Include Library** > **Manage Libraries**
3. Search for "Serial Interface Handler"
4. Click **Install**

### Manual Installation
1. Download the repository as a ZIP file
2. Extract the contents to your Arduino libraries folder (typically `~/Arduino/libraries/`)
3. Rename the extracted folder to `SerialInterfaceHandler`
4. Restart the Arduino IDE

The primary header file name matches the library folder name, which is a best practice for Arduino libraries. 

## Getting Started

### Basic Setup

```cpp
#include <SIH.h>

// Create global instance
SIH sih(&Serial);

void setup() {
  // Initialize the Serial Interface Handler
  if (!sih.begin(115200)) {
    Serial.println("Failed to initialize SIH");
    while (true) {
      delay(1000);
    }
  }
  
  // Set custom timeout (3 minutes)
  sih.setTimeout(180000);
}

void loop() {
  // Process the serial interface handler
  sih.process();
  delay(10); // Small delay to prevent watchdog reset
}
```

## Basic Usage

After initialization, you can interact with the system through the serial monitor:

1. **Access Help System**:
   ```
   help
   ?           // Quick help summary
   help wifi   // Help for specific category
   help menu   // Help for specific command
   ```

2. **Navigate Menus**:
   ```
   menu        // Enter main menu
   1           // Select option 1 (WiFi Configuration)
   0           // Exit menu mode
   ```

3. **WiFi Operations**:
   ```
   wifi scan   // Scan for networks
   wifi auto   // Auto-connect to stored networks
   wifi status // Check connection status
   ```

4. **Credential Management**:
   ```
   creds list  // List stored networks
   creds clear // Clear all credentials
   ```

## Available Commands

### System Commands
- `help` - Show detailed help information
- `?` - Quick help summary
- `menu` - Access interactive menus
- `status` - Show system status
- `reconnect` - Force reconnection to stored networks
- `system info` - Display hardware information
- `system reset` - Restart the device

### WiFi Commands
- `wifi scan` - Scan for available networks
- `wifi connect <ssid> <password>` - Connect to a network
- `wifi status` - Check current WiFi status
- `wifi auto` - Auto-connect to stored networks
- `wifi disconnect` - Disconnect from current network

### Credentials Commands
- `creds list` - List stored networks
- `creds remove <ssid>` - Remove a specific network
- `creds clear` - Clear all stored credentials
- `creds stats` - Show storage statistics (ESP32 only)

## Adding Custom Commands

The SIH library provides a flexible command registration system. To add your own commands:

### Method 1: Using Lambda Functions (Recommended)

```cpp
void setup() {
  sih.begin();
  
  // Register a custom command
  sih.registerCommand("hello", 
    [](const std::vector<String>& args) {
      Serial.println("Hello, World!");
      if (!args.empty()) {
        Serial.print("Arguments received: ");
        for (const auto& arg : args) {
          Serial.print(arg + " ");
        }
      }
    },
    "Say hello to the world",
    "hello [name]",
    "system"  // Category for organization
  );
}
```

### Method 2: Using Member Functions

```cpp
class MyDevice {
public:
  void handleCustomCommand(const std::vector<String>& args) {
    Serial.println("Custom command executed!");
    // Your implementation here
  }
};

MyDevice device;

void setup() {
  sih.begin();
  
  // Register command with member function
  sih.registerCommand("custom", 
    [device](const std::vector<String>& args) {
      device.handleCustomCommand(args);
    },
    "Execute custom device function",
    "custom [options]",
    "device"
  );
}
```

### Command Registration Parameters

```cpp
void registerCommand(
  const String& command,          // Command name (e.g., "hello")
  CommandCallback handler,        // Function to execute
  const String& description,      // Brief description for help
  const String& usage,            // Usage example for help
  const String& category = "general"  // Category for organization
);
```

## Creating New Menu Options

To add custom options to the interactive menu system:

### Step 1: Define Your Menu Item Structure

```cpp
// Add this to your sketch or modify the library source
struct CustomMenuItem {
  int id;
  String label;
  std::function<void()> action;
  String description;
  bool isEnabled;
};
```

### Step 2: Create Your Custom Menu Function

```cpp
void showCustomMenu() {
  sih.clearScreen();
  sih.showHeader("Custom Device Control");
  
  std::vector<CustomMenuItem> customItems = {
    {1, "Control LED", []() { controlLED(); }, "Turn LED on/off", true},
    {2, "Read Sensor", []() { readSensor(); }, "Read sensor data", true},
    {3, "Back to Main Menu", []() { sih.showMenu(MenuSystem::MAIN); }, "Return to main menu", true}
  };
  
  // Display menu items
  for (const auto& item : customItems) {
    if (item.isEnabled) {
      Serial.printf("%d. %s\n", item.id, item.label.c_str());
      if (!item.description.isEmpty()) {
        Serial.printf("   - %s\n", item.description.c_str());
      }
    }
  }
  
  sih.showFooter();
  Serial.print("\nSelect option (1-" + String(customItems.size()) + "): ");
}
```

### Step 3: Add Menu Navigation to Existing System

```cpp
// Add a new menu option to the main menu
void setup() {
  sih.begin();
  
  // Register command to access custom menu
  sih.registerCommand("device", 
    [](const std::vector<String>& args) {
      showCustomMenu();
    },
    "Access custom device controls",
    "device",
    "system"
  );
  
  // Also add to main menu if desired
  sih.addMenuItem(MenuSystem::MAIN, 
    {5, "Device Control", []() { showCustomMenu(); }, "Custom device functions", true}
  );
}
```

### Advanced: Extending the Library

For more permanent additions, you can modify the library source:

1. **Add new menu type** to `MenuSystem::MenuType` enum in `ESP8266_SIH.h`/`ESP32_SIH.h`
2. **Add menu items vector** to `MenuSystem` class
3. **Implement menu handling** in `MenuSystem::showMenu()` and `MenuSystem::handleMenuInput()`
4. **Add initialization** in `MenuSystem::initializeMenus()`

## Configuration Options

### Timeout Settings
```cpp
sih.setTimeout(60000);  // Set global timeout to 60 seconds
```

### Storage Configuration (Edit in Header Files)
```cpp
// In ESP8266_SIH.h or ESP32_SIH.h
static const size_t MAX_NETWORKS = 10;        // Maximum stored networks
static const size_t SSID_MAX_LENGTH = 64;     // Maximum SSID length
static const size_t PASSWORD_MAX_LENGTH = 128; // Maximum password length
```

### WiFi Settings
```cpp
// Modify in WiFiManager::setupWiFi()
WiFi.setTxPower(WIFI_POWER_19_5dBm);  // Adjust transmit power
WiFi.setHostname("MyCustomDevice");   // Set custom hostname
```

## Platform Support

### ESP8266
- Uses EEPROM for credential storage
- Compatible with ESP8266 Arduino Core v2.7.0+
- Limited to 5 stored networks by default

### ESP32
- Uses NVS (Non-Volatile Storage) for credential storage
- Compatible with ESP32 Arduino Core v1.0.0+
- Better performance and memory management
- Additional system information available

The library automatically detects the platform and includes the appropriate implementation files. 

## Examples

The library includes several example sketches demonstrating different features:

### Basic Usage (ESP32)
```cpp
#include <SIH.h>

SIH sih(&Serial);

void setup() {
  if (!sih.begin(115200)) {
    Serial.println("Failed to initialize ESP32_SIH");
    while (true) delay(1000);
  }
  
  sih.setTimeout(180000); // 3 minute timeout
}

void loop() {
  sih.process();
  delay(10);
}
```

### Advanced Usage with Custom Commands
```cpp
#include <SIH.h>

SIH sih(&Serial);

void handleTemperatureCommand(const std::vector<String>& args) {
  float temp = 25.5; // Read from sensor
  Serial.printf("Current temperature: %.1f°C\n", temp);
}

void handleLEDCommand(const std::vector<String>& args) {
  if (args.empty()) {
    Serial.println("Usage: led <on|off>");
    return;
  }
  
  if (args[0] == "on") {
    digitalWrite(LED_BUILTIN, HIGH);
    Serial.println("LED turned ON");
  } else if (args[0] == "off") {
    digitalWrite(LED_BUILTIN, LOW);
    Serial.println("LED turned OFF");
  } else {
    Serial.println("Invalid argument. Use 'on' or 'off'");
  }
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  
  if (!sih.begin()) {
    Serial.println("SIH initialization failed");
    while (true) delay(1000);
  }
  
  // Register custom commands
  sih.registerCommand("temp", handleTemperatureCommand,
    "Read temperature sensor", "temp", "sensors");
    
  sih.registerCommand("led", handleLEDCommand,
    "Control built-in LED", "led <on|off>", "device");
}

void loop() {
  sih.process();
  delay(10);
}
```

## API Documentation

### Main SIH Class Methods

| Method | Description | Parameters |
|--------|-------------|------------|
| `begin(uint32_t baudRate = 115200)` | Initialize the SIH system | Baud rate for serial communication |
| `process()` | Process serial input and system tasks | None |
| `registerCommand()` | Register a new command | See command registration section |
| `setTimeout(unsigned long timeoutMs)` | Set inactivity timeout | Timeout in milliseconds |
| `isConnected()` | Check WiFi connection status | None |
| `getConnectedSSID()` | Get current SSID | None |

### Menu System Methods

| Method | Description |
|--------|-------------|
| `showMenu(MenuType type)` | Display specified menu |
| `exitMenu()` | Exit menu mode |
| `isInMenu()` | Check if in menu mode |
| `setTimeout(unsigned long timeoutMs)` | Set menu timeout |

## Contributing

Contributions are welcome! Please follow these guidelines:

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/your-feature`)
3. Make your changes
4. Ensure code follows the Arduino style guide for libraries 
5. Update documentation if needed
6. Submit a pull request

### Best Practices for Library Development
- Maintain compatibility with both ESP8266 and ESP32 platforms
- Follow the Arduino library structure format 
- Include comprehensive examples in the examples folder 
- Document all public APIs and configuration options
- Test on multiple hardware variants

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

Copyright (c) 2025 Haroon Raza
https://github.com/voltvirtuoso/SerialInterfaceHandler.git

