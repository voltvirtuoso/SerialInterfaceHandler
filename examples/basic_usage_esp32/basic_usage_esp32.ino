/**
 * @file ESP32_SIH_Example.ino
 * @brief ESP32 Serial Interface Handler - Example Usage
 * 
 * This example demonstrates how to use the ESP32 Serial Interface Handler
 * library to create a comprehensive serial interface system with WiFi,
 * Bluetooth, and system monitoring capabilities.
 * 
 * @author ESP32_SIH Development Team
 * @version 2.0.0
 * @date December 2025
 * 
 * @copyright MIT License
 */

#include <ESP32_SIH.h>

// Create global instance
ESP32_SIH sih;

// Custom command examples
void customHelloCommand(const std::vector<String>& args) {
    if (args.empty()) {
        Serial.println("Hello from ESP32!");
    } else {
        Serial.printf("Hello, %s!\n", args[0].c_str());
    }
}

void customSystemStatsCommand(const std::vector<String>& args) {
    Serial.println("\n=== Extended System Statistics ===");
    Serial.printf("CPU Frequency: %d MHz\n", getXtalFrequency());
    Serial.printf("Deep Sleep Available: %s\n", esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_UNDEFINED ? "Yes" : "No");
    Serial.printf("Reset Reason: %d\n", esp_reset_reason());
    Serial.printf("Chip Temperature: %.2f°C\n", (float)temperatureRead());
}

void customBluetoothTestCommand(const std::vector<String>& args) {
    auto btInfo = sih.getSystemMonitor().getBluetoothInfo();
    
    Serial.println("\n=== Bluetooth Test ===");
    if (btInfo.isEnabled) {
        Serial.println("✓ Bluetooth is enabled and functional");
        Serial.printf("MAC Address: %s\n", btInfo.macAddress.c_str());
        
        // Example of what could be done with Bluetooth
        Serial.println("\nNote: This is just information display.");
        Serial.println("To use actual Bluetooth functionality, you need to:");
        Serial.println("1. Include Bluetooth libraries in your sketch");
        Serial.println("2. Initialize Bluetooth in setup()");
        Serial.println("3. Use dedicated Bluetooth commands");
        
        Serial.println("\nExample initialization code:");
        Serial.println("#include <BluetoothSerial.h>");
        Serial.println("BluetoothSerial SerialBT;");
        Serial.println("void setup() {");
        Serial.println("  SerialBT.begin(\"ESP32_Device\");");
        Serial.println("}");
    } else {
        Serial.println("✗ Bluetooth is not enabled");
        Serial.println("Enable Bluetooth in your firmware setup() function");
    }
}

void setup() {
    // Initialize the Serial Interface Handler
    if (!sih.begin(115200)) {
        Serial.println("Failed to initialize ESP32_SIH");
        while (true) {
            delay(1000);
        }
    }
    
    // Register custom commands
    sih.registerCommand("hello", customHelloCommand, 
                       "Say hello with optional name", 
                       "hello [name]", 
                       "custom", 
                       false);
    
    sih.registerCommand("stats", customSystemStatsCommand, 
                       "Show extended system statistics", 
                       "stats", 
                       "system", 
                       false);
    
    sih.registerCommand("bttest", customBluetoothTestCommand, 
                       "Test Bluetooth functionality", 
                       "bttest", 
                       "bluetooth", 
                       false);
    
    // Set custom timeout (3 minutes)
    sih.setTimeout(180000);
    
    Serial.println("\n=== ESP32 Serial Interface Handler Example ===");
    Serial.println("Custom commands registered:");
    Serial.println("  hello [name] - Say hello");
    Serial.println("  stats        - Show extended statistics");
    Serial.println("  bttest       - Test Bluetooth capabilities");
    Serial.println("\nSystem is ready for commands...");
}

void loop() {
    // Process the serial interface handler
    sih.process();
    
    // Your other application code can go here
    // The SIH system runs in the background and handles serial commands
    
    // Example: Blink LED to show system is alive
    static unsigned long lastBlink = 0;
    static bool ledState = false;
    
    if (millis() - lastBlink > 1000) {
        lastBlink = millis();
        ledState = !ledState;
        digitalWrite(LED_BUILTIN, ledState ? HIGH : LOW);
    }
    
    delay(10); // Small delay to prevent watchdog reset
}
