#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>

#include "config.h"

class WifiManager {
 public:
  using Credential = Config::WifiCredential;

  void begin(const Credential* credentials, size_t credentialCount, uint32_t reconnectIntervalMs);
  void loop(uint32_t nowMs);
  bool isConnected() const;
  const char* statusText() const;

 private:
  const Credential* credentials_ = nullptr;
  size_t credentialCount_ = 0;
  size_t nextCredentialIndex_ = 0;
  uint32_t reconnectIntervalMs_ = 5000;
  uint32_t lastAttemptMs_ = 0;
  bool beginCalled_ = false;

  void startConnect(uint32_t nowMs);
};

#endif  // WIFI_MANAGER_H
