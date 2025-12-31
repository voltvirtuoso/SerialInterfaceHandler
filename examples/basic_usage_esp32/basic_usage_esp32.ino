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

#include <SIH.h>

// Create global instance
SIH sih(&Serial);

void setup() {
    // Initialize the Serial Interface Handler
    if (!sih.begin(115200)) {
        Serial.println("Failed to initialize ESP32_SIH");
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
