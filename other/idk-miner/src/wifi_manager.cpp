#include "wifi_manager.h"

void WifiManager::begin(const Credential* credentials, size_t credentialCount,
                        uint32_t reconnectIntervalMs) {
  credentials_ = credentials;
  credentialCount_ = credentialCount;
  nextCredentialIndex_ = 0;
  reconnectIntervalMs_ = reconnectIntervalMs;

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);

  beginCalled_ = true;
  startConnect(millis());
}

void WifiManager::loop(uint32_t nowMs) {
  if (!beginCalled_) {
    return;
  }

  if (WiFi.status() == WL_CONNECTED) {
    return;
  }

  if (nowMs - lastAttemptMs_ >= reconnectIntervalMs_) {
    startConnect(nowMs);
  }
}

bool WifiManager::isConnected() const { return WiFi.status() == WL_CONNECTED; }

const char* WifiManager::statusText() const {
  switch (WiFi.status()) {
    case WL_CONNECTED:
      return "Connected";
    case WL_CONNECT_FAILED:
      return "Connect failed";
    case WL_CONNECTION_LOST:
      return "Connection lost";
    case WL_DISCONNECTED:
      return "Disconnected";
    default:
      return "Connecting...";
  }
}

void WifiManager::startConnect(uint32_t nowMs) {
  lastAttemptMs_ = nowMs;
  if (credentials_ == nullptr || credentialCount_ == 0) {
    return;
  }

  for (size_t i = 0; i < credentialCount_; ++i) {
    const size_t idx = (nextCredentialIndex_ + i) % credentialCount_;
    const char* ssid = credentials_[idx].ssid;
    if (ssid != nullptr && ssid[0] != '\0') {
      const char* password = credentials_[idx].password;
      WiFi.begin(ssid, (password == nullptr) ? "" : password);
      nextCredentialIndex_ = (idx + 1) % credentialCount_;
      return;
    }
  }
}
