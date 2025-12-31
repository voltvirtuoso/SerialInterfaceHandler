/**
 * @file SIH.h
 * @brief Serial Interface Handler - Platform-agnostic header
 * 
 * This header file provides conditional compilation to select the correct
 * implementation based on the target platform (ESP8266 or ESP32).
 * 
 * @author SIH Development Team
 * @version 1.0.0
 * @date December 2025
 * 
 * @copyright MIT License
 */

#ifndef SIH_H
#define SIH_H

// Platform detection
#if defined(ESP8266)
    #include "ESP8266_SIH.h"
    #define SIH ESP8266_SIH
    #define SIH_STORAGE EEPROMStorage
#elif defined(ESP32)
    #include "ESP32_SIH.h"
    #define SIH ESP32_SIH
    #define SIH_STORAGE NVSStorage
#else
    #error "Unsupported platform. Only ESP8266 and ESP32 are supported."
#endif

#endif // SIH_H