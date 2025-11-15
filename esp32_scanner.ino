/* --------------------------------------------------------------
   Wi-Fi RSSI finder for multiple APs (3 targets)
   - Monitors distance to 3 SSIDs
   - Shows RSSI, distance and ON/OFF state each scan
   -------------------------------------------------------------- */
#include <WiFi.h>

// ────────  USER-CONFIGURABLE SEARCH CRITERIA  ─────────────────
const int N_TARGETS = 4;

// global / static


const char* TARGET_SSIDS[N_TARGETS] = {
  "ESP32-Access-Point_1",    // Target 0
  "ESP32-Access-Point_2",    // Target 1
  "ESP32-Access-Point_3",  // Target 2
  "ESP32-Access-Point_4"     // Target 3
};

// Optional: if you know a specific BSSID for 1st target, else leave empty.
const String TARGET_BSSID = "";          // e.g. "AA:BB:CC:DD:EE:FF"
const int    TARGET_CH     = 0;          // 0 = any channel

// How long to wait between scans (ms)
const unsigned long SCAN_INTERVAL = 100;

// Per-target runtime data
int   targetRSSI[N_TARGETS] = {0, 0, 0, 0};
bool  foundTarget[N_TARGETS];
char  foundSSID[N_TARGETS][33];          // store actual SSID (up to 32 chars)
float filteredRSSI[N_TARGETS] = {0.0, 0.0, 0.0, 0.0};
bool  haveFiltered[N_TARGETS] = {false, false, false, false};


// ----------------------------------------------------------------

float rssiToMeters(float rssi, float rssi0 = -45.0, float n = 3.0) {
  // Convert RSSI (dBm) → estimated distance (m)
  // rssi0 = RSSI at 1 meter 
  // n = path-loss exponent

  float exponent = (rssi0 - rssi) / (10.0f * n);
  float distance = pow(10.0f, exponent);
  return distance;
}


// Exponential smoothing of RSSI for anchor i, with outlier rejection
float filterRSSI(int i, float newRSSI) {
  const float alpha     = 0.3f;  // 0..1, smaller = smoother
  const float maxJumpDb = 10.0f; // max allowed change in dB between updates

  if (!haveFiltered[i]) {
    // First valid measurement: just take it
    filteredRSSI[i] = newRSSI;
    haveFiltered[i] = true;
  } else {
    float prev = filteredRSSI[i];
    float diff = fabs(newRSSI - prev);

    if (diff > maxJumpDb) {
      // Outlier: ignore this sudden jump, keep previous
      // (you can Serial.printf here for debugging if you want)
      return filteredRSSI[i];
    }

    // Normal case: smooth
    filteredRSSI[i] = alpha * newRSSI + (1.0f - alpha) * prev;
  }

  return filteredRSSI[i];
}



void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();  // make sure we are not connected
  delay(100);
  Serial.println("\n=== Wi-Fi RSSI finder (%d targets) started ===");
}

void loop() {
  // 1. Perform a scan
  int n = WiFi.scanNetworks();
  if (n == 0) {
    Serial.println("No networks found – retrying…");
    delay(SCAN_INTERVAL);
    return;
  }

  // 2. Reset search flags for this scan
  for (int t = 0; t < N_TARGETS; ++t) {
    targetRSSI[t]   = -127;
    foundTarget[t]  = false;
    foundSSID[t][0] = '\0';
  }

  // 3. (Optional) list all networks for debugging
  Serial.printf("%d networks found:\n", n);

  // 4. Go through all visible networks and see if any match our targets
  for (int i = 0; i < n; ++i) {
    String ssid  = WiFi.SSID(i);
    String bssid = WiFi.BSSIDstr(i);
    int    ch    = WiFi.channel(i);
    int    rssi  = WiFi.RSSI(i);

    // For each target, check if SSID matches
    for (int t = 0; t < N_TARGETS; ++t) {
      bool ssidMatch    = (ssid == TARGET_SSIDS[t]);
      bool bssidMatch   = (TARGET_BSSID.length() == 0 || bssid.equalsIgnoreCase(TARGET_BSSID));
      bool channelMatch = (TARGET_CH == 0 || ch == TARGET_CH);

      if (ssidMatch && bssidMatch && channelMatch) {
        foundTarget[t]  = true;
        targetRSSI[t]   = rssi;
        strncpy(foundSSID[t], ssid.c_str(), 32);
        foundSSID[t][32] = '\0';  // ensure null-termination
        // no break here: same SSID could appear multiple times, last one wins
      }
    }
  }

  // 5. Report a summary of all targets
  Serial.println("─────────────────────────────────────────────────────");
  for (int t = 0; t < N_TARGETS; ++t) {
    if (foundTarget[t]) {
      float smoothedRSSI = filterRSSI(t, targetRSSI[t]);
      float d = rssiToMeters(smoothedRSSI);
      Serial.printf(
        "[ON ] Target %d: \"%s\"  RSSI = %d dBm  Distance ≈ %.2f m\n",
        t, foundSSID[t], targetRSSI[t], d
      );
    } else {
      Serial.printf(
        "[OFF] Target %d: \"%s\"  (not visible this scan)\n",
        t, TARGET_SSIDS[t]
      );
    }
  }
  Serial.println("─────────────────────────────────────────────────────\n");

  // 6. Wait before the next scan
  delay(SCAN_INTERVAL);
}
