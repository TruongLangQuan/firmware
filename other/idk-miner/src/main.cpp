#include "M5Unified.h"

#include "api_client.h"
#include "config.h"
#include "ui.h"
#include "wifi_manager.h"

namespace {

WifiManager gWifi;
ApiClient gApi(Config::kHttpTimeoutMs, Config::kAllowInsecureTls, Config::kHttpBufferSize);
UI gUi;
UIModel gModel;

uint32_t gLastRefreshMs = 0;
uint32_t gLastRenderMs = 0;
bool gHasFetchedAtLeastOnce = false;
bool gHasPrevLtcBalance = false;
float gPrevLtcBalance = 0.0f;

char gStatusBuf[Config::kStatusBufferSize] = "Connecting...";
char gErrBufA[Config::kStatusBufferSize];
char gErrBufB[Config::kStatusBufferSize];
char gLtcStatus[Config::kStatusBufferSize] = "Waiting LTC...";
char gBtcStatus[Config::kStatusBufferSize] = "Waiting BTC...";

void setStatus(const char* text) {
  snprintf(gStatusBuf, sizeof(gStatusBuf), "%s", (text == nullptr) ? "-" : text);
  gModel.status = gStatusBuf;
}

void refreshActiveStatus() {
  if (!gModel.wifiConnected) {
    setStatus("Connecting...");
    return;
  }
  if (gModel.screen == SCREEN_LTC) {
    setStatus(gLtcStatus);
  } else {
    setStatus(gBtcStatus);
  }
}

void fetchData() {
  gModel.fetching = true;
  setStatus("Fetching...");
  gUi.render(gModel, true);

  const bool hadLtc = gModel.ltcValid;
  const bool hadBtc = gModel.btcValid;

  bool ltcOk = gApi.fetchLTC(Config::kLtcAddress, gModel.ltc, gErrBufA, sizeof(gErrBufA));
  bool btcOk = gApi.fetchBTC(Config::kBtcAddress, gModel.btc, gErrBufB, sizeof(gErrBufB));

  if (ltcOk) {
    if (gHasPrevLtcBalance) {
      const float delta = gModel.ltc.balance - gPrevLtcBalance;
      gModel.ltc.lastMined = (delta > 0.0f) ? delta : 0.0f;
    } else {
      gModel.ltc.lastMined = 0.0f;
    }
    gPrevLtcBalance = gModel.ltc.balance;
    gHasPrevLtcBalance = true;
    gModel.ltcValid = true;
    snprintf(gLtcStatus, sizeof(gLtcStatus), "LTC Connected");
  } else if (hadLtc) {
    snprintf(gLtcStatus, sizeof(gLtcStatus), "LTC cached");
  } else {
    snprintf(gLtcStatus, sizeof(gLtcStatus), "LTC retry...");
  }

  if (btcOk) {
    gModel.btcValid = true;
    snprintf(gBtcStatus, sizeof(gBtcStatus), "BTC Connected");
  } else if (hadBtc) {
    snprintf(gBtcStatus, sizeof(gBtcStatus), "BTC cached");
  } else {
    snprintf(gBtcStatus, sizeof(gBtcStatus), "BTC retry...");
  }
  gModel.fetching = false;

  refreshActiveStatus();

  gHasFetchedAtLeastOnce = true;
  gLastRefreshMs = millis();
}

void setupModelDefaults() {
  gModel.screen = SCREEN_LTC;
  gModel.wifiConnected = false;
  gModel.fetching = false;
  gModel.ltcValid = false;
  gModel.btcValid = false;
  gModel.status = gStatusBuf;
}

}  // namespace

void idk_miner_setup() {
  auto cfg = M5.config();
  cfg.clear_display = true;
  cfg.output_power = true;
  M5.begin(cfg);

  setupModelDefaults();
  gUi.begin();

  gWifi.begin(Config::kWifiCredentials, Config::kWifiCredentialCount, Config::kWifiReconnectIntervalMs);
  setStatus("Connecting...");
  gUi.render(gModel, true);
}

void idk_miner_loop() {
  const uint32_t now = millis();

  M5.update();
  gWifi.loop(now);
  gModel.wifiConnected = gWifi.isConnected();

  if (M5.BtnA.wasClicked()) {
    gUi.nextScreen();
  }
  if (M5.BtnB.wasClicked()) {
    gUi.prevScreen();
  }
  gModel.screen = gUi.screen();
  refreshActiveStatus();

  if (!gModel.wifiConnected) {
    gModel.fetching = false;
    setStatus("Connecting...");
  } else {
    const bool refreshDue = (!gHasFetchedAtLeastOnce) || (now - gLastRefreshMs >= Config::kRefreshIntervalMs);
    if (refreshDue) {
      fetchData();
    }
  }

  if (now - gLastRenderMs >= Config::kUiRenderIntervalMs) {
    gUi.render(gModel, false);
    gLastRenderMs = now;
  }
}
