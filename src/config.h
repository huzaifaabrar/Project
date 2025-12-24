#pragma once

/*************************************************************
 *                       USER CONFIG                         *
 *   All parameters below can be modified by the end-user.  *
 *************************************************************/

/* -----------------------------
 * Wi-Fi Configuration
 * ----------------------------- */
#define WIFI_SSID      "xxxxxxxxx" // Your Wi-Fi SSID
#define WIFI_PASSWORD  "xxxxxxxxx"  // Your Wi-Fi Password

/* -----------------------------
 * FFT / Audio Processing
 * ----------------------------- */
// Use long-window FFT (4-second) or short-window FFT
#define USE_LONG_WINDOW   0   // 1 = long-window, 0 = short-window

// Frequency range for fire alarm detection (in Hz)
#define Freq_START_HZ    2750  // Start frequency in Hz
#define Freq_END_HZ      3250  // End frequency in Hz

// Threshold for detection in decibels
#define THRESHOLD_DB     -40.0f  // dB threshold for detection

/*************************************************************
 *                      END OF CONFIG                         *
 *************************************************************/
