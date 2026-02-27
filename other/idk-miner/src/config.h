#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

namespace Config {

static constexpr char kProjectName[] = "idk-miner";

// WiFi credentials
struct WifiCredential {
  const char* ssid;
  const char* password;
};

static constexpr WifiCredential kWifiCredentials[] = {
    {"Quan Le", "15032011"},
    {"NTD-THCS", "GIAOVIEN2425@"},
};
static constexpr size_t kWifiCredentialCount = sizeof(kWifiCredentials) / sizeof(kWifiCredentials[0]);

// Wallets
static constexpr char kLtcAddress[] = "ltc1qcl9h4kffh8w8nr77u8la5ykn4cg59ms2lteply";
static constexpr char kBtcAddress[] = "bc1qul2pvt0l5deykfznzkzhfjf3yj3cla2cssd54e";

// API endpoints
static constexpr char kLtcApiUrlFmt[] = "https://api.unminable.com/v5/address/%s?coin=LTC";
static constexpr char kBtcClientApiUrlFmt[] = "https://public-pool.io:40557/api/client/%s";
static constexpr char kBtcNetworkApiUrl[] = "https://public-pool.io:40557/api/network";

// Timings
static constexpr uint32_t kRefreshIntervalMs = 10000;
static constexpr uint32_t kWifiReconnectIntervalMs = 5000;
static constexpr uint32_t kUiRenderIntervalMs = 100;
static constexpr uint32_t kHttpTimeoutMs = 6000;

// Security
static constexpr bool kAllowInsecureTls = true;

// Buffers
static constexpr size_t kHttpBufferSize = 8192;
static constexpr size_t kStatusBufferSize = 96;

}  // namespace Config

#endif  // CONFIG_H
