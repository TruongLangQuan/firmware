#ifndef UI_H
#define UI_H

#include <Arduino.h>

#include "api_client.h"

enum Screen {
  SCREEN_LTC = 0,
  SCREEN_BTC = 1,
};

struct UIModel {
  Screen screen = SCREEN_LTC;
  bool wifiConnected = false;
  bool fetching = false;
  bool ltcValid = false;
  bool btcValid = false;
  LTCData ltc;
  BTCData btc;
  const char* status = "Idle";
};

class UI {
 public:
  void begin();
  void setScreen(Screen screen);
  Screen screen() const;
  void nextScreen();
  void prevScreen();
  void render(const UIModel& model, bool force = false);

 private:
  Screen screen_ = SCREEN_LTC;
  uint32_t lastSignature_ = 0;

  static uint32_t fnv1a(const uint8_t* data, size_t size, uint32_t seed = 2166136261u);
  static uint32_t hashModel(const UIModel& model);

  void drawFrame();
  void drawLtc(const UIModel& model);
  void drawBtc(const UIModel& model);
  void drawStatusLine(const UIModel& model);
};

#endif  // UI_H
