#pragma once

/*************************************************************
 *                       USER CONFIG                         *
 *   All parameters below can be modified by the end-user.  *
 *************************************************************/

/* -----------------------------
 * Wi-Fi Configuration
 * ----------------------------- */
#define WIFI_SSID      "Pixel_2012"
#define WIFI_PASSWORD  "zaifi1234"

/* -----------------------------
 * FFT / Audio Processing
 * ----------------------------- */
// Use long-window FFT (4-second) or short-window FFT (1-second)
#define USE_LONG_WINDOW   0   // 1 = long-window, 0 = short-window

// Frequency range for fire alarm detection (in Hz)
#define Freq_START_HZ    2750  // Start frequency in Hz
#define Freq_END_HZ      3250  // End frequency in Hz

// Threshold for detection in decibels
#define THRESHOLD_DB     -50.0f  // dB threshold for detection

/*************************************************************
 *                      END OF CONFIG                         *
 *************************************************************/
