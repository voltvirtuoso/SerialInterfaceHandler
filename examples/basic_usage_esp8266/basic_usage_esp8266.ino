/**
 * @file Basic_Usage.ino
 * @brief ESP_SIH Basic Usage Example
 * 
 * This example demonstrates the basic setup and usage of the ESP_SIH library.
 * It shows how to initialize the system, register custom commands, and
 * integrate it with your application logic.
 * 
 * @section Connections
 * - Connect ESP8266 to USB for serial communication
 * - No additional hardware required for basic operation
 * 
 * @section Usage
 * 1. Upload this sketch to your ESP8266
 * 2. Open serial monitor at 115200 baud
 * 3. Type 'help' to see available commands
 * 4. Type 'menu' to access configuration menus
 * 
 * @author ESP_SIH Development Team
 * @version 1.0.0
 * @date December 2025
 */
#include <ESP8266_SIH.h>

ESP8266_SIH serialSystem;

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n=== ESP_SIH Basic Usage Example ===");
    Serial.println("Initializing system...");
    
    // Initialize the serial interface system
    if (!serialSystem.begin(115200)) {
        Serial.println("❌ System initialization failed!");
        return;
    }
    
    Serial.println("✅ System initialized successfully!");
    
    // Register custom application commands
    registerCustomCommands();
    
    Serial.println("\nType 'help' for available commands");
    Serial.println("Type 'menu' to access configuration menus");
}

void registerCustomCommands() {
	
	/*
	registerCommand(
		command,
		callback (lamda function),
		description,
		usage,
		category
	)
	*/
	
    // Example: Read analog sensor
    serialSystem.registerCommand("sensor", [](const std::vector<String>& args) {
        int sensorValue = analogRead(A0);
        Serial.printf("Sensor value: %d\n", sensorValue);
    }, "Read analog sensor value", "sensor", "hardware");
    
    // Example: Toggle GPIO
    serialSystem.registerCommand("gpio", [](const std::vector<String>& args) {
        if (args.size() >= 2) {
            int pin = args[0].toInt();
            int state = args[1].toInt();
            pinMode(pin, OUTPUT);
            digitalWrite(pin, state);
            Serial.printf("GPIO %d set to %d\n", pin, state);
        } else {
            Serial.println("Usage: gpio <pin> <state>");
        }
    }, "Control GPIO pins", "gpio <pin> <state>", "hardware");
    
}

void loop() {
    // Process serial commands and maintain WiFi connection
    serialSystem.process();
    
    // Your application logic can go here
    // The system will handle WiFi reconnection automatically
    
    delay(10); // Prevent watchdog trigger
}
