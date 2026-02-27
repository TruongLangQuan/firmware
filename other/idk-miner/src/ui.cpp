#include "ui.h"

#include "M5Unified.h"
#include <cstdio>

#include "config.h"

void UI::begin() {
  screen_ = SCREEN_LTC;
  lastSignature_ = 0;

  auto& d = M5.Display;
  d.setRotation(3);
  d.fillScreen(TFT_BLACK);
  d.setTextDatum(top_left);
  d.setTextSize(1);
  d.setTextColor(TFT_WHITE, TFT_BLACK);
}

void UI::setScreen(Screen screen) { screen_ = screen; }

Screen UI::screen() const { return screen_; }

void UI::nextScreen() { screen_ = (screen_ == SCREEN_LTC) ? SCREEN_BTC : SCREEN_LTC; }

void UI::prevScreen() { screen_ = (screen_ == SCREEN_BTC) ? SCREEN_LTC : SCREEN_BTC; }

void UI::render(const UIModel& model, bool force) {
  UIModel temp = model;
  temp.screen = screen_;

  const uint32_t sig = hashModel(temp);
  if (!force && sig == lastSignature_) {
    return;
  }
  lastSignature_ = sig;

  drawFrame();

  if (screen_ == SCREEN_LTC) {
    drawLtc(temp);
  } else {
    drawBtc(temp);
  }

  drawStatusLine(temp);
}

uint32_t UI::fnv1a(const uint8_t* data, size_t size, uint32_t seed) {
  uint32_t hash = seed;
  for (size_t i = 0; i < size; ++i) {
    hash ^= data[i];
    hash *= 16777619u;
  }
  return hash;
}

uint32_t UI::hashModel(const UIModel& model) {
  uint32_t hash = 2166136261u;

  hash = fnv1a(reinterpret_cast<const uint8_t*>(&model.screen), sizeof(model.screen), hash);
  hash = fnv1a(reinterpret_cast<const uint8_t*>(&model.wifiConnected), sizeof(model.wifiConnected), hash);
  hash = fnv1a(reinterpret_cast<const uint8_t*>(&model.fetching), sizeof(model.fetching), hash);
  hash = fnv1a(reinterpret_cast<const uint8_t*>(&model.ltcValid), sizeof(model.ltcValid), hash);
  hash = fnv1a(reinterpret_cast<const uint8_t*>(&model.btcValid), sizeof(model.btcValid), hash);
  hash = fnv1a(reinterpret_cast<const uint8_t*>(&model.ltc), sizeof(model.ltc), hash);

  const float btcPacked[] = {model.btc.hashrate, model.btc.bestDifficulty};
  hash = fnv1a(reinterpret_cast<const uint8_t*>(btcPacked), sizeof(btcPacked), hash);

  if (!model.btc.uptime.isEmpty()) {
    hash = fnv1a(reinterpret_cast<const uint8_t*>(model.btc.uptime.c_str()), model.btc.uptime.length(), hash);
  }
  if (model.status != nullptr) {
    hash = fnv1a(reinterpret_cast<const uint8_t*>(model.status), strlen(model.status), hash);
  }

  return hash;
}

void UI::drawFrame() {
  auto& d = M5.Display;
  d.fillScreen(TFT_BLACK);

  d.setTextSize(2);
  d.setTextColor(TFT_WHITE, TFT_BLACK);
  d.setCursor(8, 6);
  d.print(Config::kProjectName);

  d.drawFastHLine(0, 24, d.width(), TFT_DARKGREY);
  d.drawFastHLine(0, d.height() - 18, d.width(), TFT_DARKGREY);

  d.setTextSize(1);
  d.setTextColor(TFT_WHITE, TFT_BLACK);
  d.setCursor(6, d.height() - 14);
  d.print("BtnA: Next   BtnB: Prev");
}

void UI::drawLtc(const UIModel& model) {
  auto& d = M5.Display;
  d.setTextSize(2);
  d.setTextColor(TFT_YELLOW, TFT_BLACK);
  d.setCursor(8, 34);
  d.print("[LTC]");

  d.setTextSize(1);
  d.setTextColor(TFT_WHITE, TFT_BLACK);

  d.setCursor(8, 60);
  d.print("Balance:");
  d.setCursor(100, 60);
  if (model.ltcValid) {
    d.printf("%.8f", model.ltc.balance);
  } else {
    d.print("-");
  }

  d.setCursor(8, 78);
  d.print("Last mined:");
  d.setCursor(100, 78);
  if (model.ltcValid) {
    d.printf("%.8f", model.ltc.lastMined);
  } else {
    d.print("-");
  }
}

void UI::drawBtc(const UIModel& model) {
  auto& d = M5.Display;
  d.setTextSize(2);
  d.setTextColor(TFT_YELLOW, TFT_BLACK);
  d.setCursor(8, 34);
  d.print("[BTC]");

  d.setTextSize(1);
  d.setTextColor(TFT_WHITE, TFT_BLACK);

  d.setCursor(8, 56);
  d.print("Uptime:");
  d.setCursor(95, 56);
  if (model.btcValid) {
    d.print(model.btc.uptime);
  } else {
    d.print("-");
  }

  d.setCursor(8, 72);
  d.print("Hashrate:");
  d.setCursor(95, 72);
  if (model.btcValid) {
    d.printf("%.2f", model.btc.hashrate);
  } else {
    d.print("-");
  }

  d.setCursor(8, 88);
  d.print("Best Diff:");
  d.setCursor(95, 88);
  if (model.btcValid) {
    d.printf("%.2f", model.btc.bestDifficulty);
  } else {
    d.print("-");
  }

}

void UI::drawStatusLine(const UIModel& model) {
  auto& d = M5.Display;
  d.setTextSize(1);

  // Status beside title (top-right).
  d.setCursor(130, 10);
  if (model.wifiConnected) {
    d.setTextColor(TFT_GREENYELLOW, TFT_BLACK);
  } else {
    d.setTextColor(TFT_ORANGE, TFT_BLACK);
  }
  d.print("St: ");

  d.setTextColor(TFT_WHITE, TFT_BLACK);
  d.print(model.status == nullptr ? "-" : model.status);
}
