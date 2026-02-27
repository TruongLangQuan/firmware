#include "api_client.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <cstdio>
#include <cstdlib>

#include "config.h"

namespace {

float parseFloatSafe(const char* s, float fallback = 0.0f) {
  if (s == nullptr || *s == '\0') {
    return fallback;
  }
  char* end = nullptr;
  const float value = strtof(s, &end);
  if (end == s) {
    return fallback;
  }
  return value;
}

float variantToFloat(const JsonVariantConst& v, float fallback = 0.0f) {
  if (v.is<float>()) {
    return v.as<float>();
  }
  if (v.is<double>()) {
    return static_cast<float>(v.as<double>());
  }
  if (v.is<int>()) {
    return static_cast<float>(v.as<int>());
  }
  if (v.is<const char*>()) {
    return parseFloatSafe(v.as<const char*>(), fallback);
  }
  return fallback;
}

static char gResponseA[Config::kHttpBufferSize];

}  // namespace

ApiClient::ApiClient(uint32_t timeoutMs, bool insecureTls, size_t responseBufferSize)
    : timeoutMs_(timeoutMs), insecureTls_(insecureTls), responseBufferSize_(responseBufferSize) {}

bool ApiClient::fetchLTC(const char* address, LTCData& out, char* errMsg, size_t errMsgSize) {
  char urlV5[220];
  snprintf(urlV5, sizeof(urlV5), Config::kLtcApiUrlFmt, address);
  char urlV4[220];
  snprintf(urlV4, sizeof(urlV4), "https://api.unminable.com/v4/address/%s?coin=LTC", address);

  const char* urls[] = {urlV5, urlV4};
  int httpCode = -1;
  char lastErr[64] = "LTC fetch failed";

  for (size_t u = 0; u < 2; ++u) {
    for (int attempt = 0; attempt < 2; ++attempt) {
      if (httpGet(urls[u], gResponseA, responseBufferSize_, httpCode, errMsg, errMsgSize)) {
        if (parseLtcJson(gResponseA, out, errMsg, errMsgSize)) {
          return true;
        }
        snprintf(lastErr, sizeof(lastErr), "%s", errMsg);
      } else {
        snprintf(lastErr, sizeof(lastErr), "%s", errMsg);
      }
      delay(60);
    }
  }

  snprintf(errMsg, errMsgSize, "%s", lastErr);
  return false;
}

bool ApiClient::fetchBTC(const char* address, BTCData& out, char* errMsg, size_t errMsgSize) {
  char clientUrl[220];
  snprintf(clientUrl, sizeof(clientUrl), Config::kBtcClientApiUrlFmt, address);

  int clientCode = -1;
  if (!httpGet(clientUrl, gResponseA, responseBufferSize_, clientCode, errMsg, errMsgSize)) {
    return false;
  }
  if (!parseBtcClientJson(gResponseA, out, errMsg, errMsgSize)) {
    return false;
  }

  return true;
}

bool ApiClient::httpGet(const char* url, char* outBuf, size_t outBufSize, int& httpCode, char* errMsg,
                        size_t errMsgSize) {
  if (outBuf == nullptr || outBufSize < 2) {
    snprintf(errMsg, errMsgSize, "HTTP buffer invalid");
    return false;
  }

  WiFiClientSecure client;
  if (insecureTls_) {
    client.setInsecure();
  }

  HTTPClient http;
  http.setTimeout(timeoutMs_);

  if (!http.begin(client, url)) {
    snprintf(errMsg, errMsgSize, "HTTP begin failed");
    return false;
  }

  httpCode = http.GET();
  if (httpCode < 200 || httpCode >= 300) {
    snprintf(errMsg, errMsgSize, "HTTP %d", httpCode);
    http.end();
    return false;
  }

  String payload = http.getString();
  http.end();

  if (payload.isEmpty()) {
    snprintf(errMsg, errMsgSize, "HTTP empty payload");
    return false;
  }
  if (payload.length() + 1 > outBufSize) {
    snprintf(errMsg, errMsgSize, "HTTP payload too large");
    return false;
  }

  memcpy(outBuf, payload.c_str(), payload.length());
  outBuf[payload.length()] = '\0';
  return true;
}

bool ApiClient::parseLtcJson(const char* json, LTCData& out, char* errMsg, size_t errMsgSize) {
  JsonDocument doc;
  const DeserializationError err = deserializeJson(doc, json);
  if (err) {
    snprintf(errMsg, errMsgSize, "LTC JSON parse failed");
    return false;
  }

  if (!doc["success"].isNull() && !doc["success"].as<bool>()) {
    const char* msg = doc["msg"] | "LTC API error";
    snprintf(errMsg, errMsgSize, "%s", msg);
    return false;
  }

  JsonVariantConst data = doc["data"];
  if (data.isNull()) {
    snprintf(errMsg, errMsgSize, "LTC missing data");
    return false;
  }

  out.balance = variantToFloat(data["balance"], 0.0f);
  // Last mined amount is derived in main loop from balance delta between refresh cycles.
  out.lastMined = 0.0f;

  return true;
}

bool ApiClient::parseBtcClientJson(const char* json, BTCData& out, char* errMsg, size_t errMsgSize) {
  JsonDocument doc;
  const DeserializationError err = deserializeJson(doc, json);
  if (err) {
    snprintf(errMsg, errMsgSize, "BTC JSON parse failed");
    return false;
  }

  out.hashrate = 0.0f;
  out.bestDifficulty = variantToFloat(doc["bestDifficulty"], 0.0f);

  uint32_t earliestStart = 0;
  uint32_t latestSeen = 0;

  JsonArrayConst workers = doc["workers"].as<JsonArrayConst>();
  for (JsonVariantConst worker : workers) {
    out.hashrate += variantToFloat(worker["hashRate"], 0.0f);

    const float workerBest = variantToFloat(worker["bestDifficulty"], 0.0f);
    if (workerBest > out.bestDifficulty) {
      out.bestDifficulty = workerBest;
    }

    const char* startTime = worker["startTime"] | "";
    const char* lastSeen = worker["lastSeen"] | "";

    uint32_t startEpoch = 0;
    uint32_t seenEpoch = 0;

    if (parseIso8601Utc(startTime, startEpoch)) {
      if (earliestStart == 0 || startEpoch < earliestStart) {
        earliestStart = startEpoch;
      }
    }

    if (parseIso8601Utc(lastSeen, seenEpoch)) {
      if (seenEpoch > latestSeen) {
        latestSeen = seenEpoch;
      }
    }
  }

  if (earliestStart > 0 && latestSeen >= earliestStart) {
    char uptimeBuf[24];
    formatDuration(latestSeen - earliestStart, uptimeBuf, sizeof(uptimeBuf));
    out.uptime = uptimeBuf;
  } else {
    out.uptime = "N/A";
  }

  return true;
}

bool ApiClient::parseIso8601Utc(const char* ts, uint32_t& epochSeconds) {
  if (ts == nullptr || strlen(ts) < 19) {
    return false;
  }

  int year = 0;
  int month = 0;
  int day = 0;
  int hour = 0;
  int minute = 0;
  int second = 0;

  if (sscanf(ts, "%4d-%2d-%2dT%2d:%2d:%2d", &year, &month, &day, &hour, &minute, &second) != 6) {
    return false;
  }

  const uint32_t days = daysFromCivil(year, static_cast<unsigned>(month), static_cast<unsigned>(day));
  epochSeconds = days * 86400UL + static_cast<uint32_t>(hour * 3600 + minute * 60 + second);
  return true;
}

uint32_t ApiClient::daysFromCivil(int year, unsigned month, unsigned day) {
  year -= month <= 2;
  const int era = (year >= 0 ? year : year - 399) / 400;
  const unsigned yoe = static_cast<unsigned>(year - era * 400);
  const unsigned doy = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return static_cast<uint32_t>(era * 146097 + static_cast<int>(doe) - 719468);
}

void ApiClient::formatDuration(uint32_t seconds, char* out, size_t outSize) {
  const uint32_t days = seconds / 86400UL;
  seconds %= 86400UL;
  const uint32_t hours = seconds / 3600UL;
  seconds %= 3600UL;
  const uint32_t minutes = seconds / 60UL;
  const uint32_t secs = seconds % 60UL;

  if (days > 0) {
    snprintf(out, outSize, "%lud %02lu:%02lu", static_cast<unsigned long>(days),
             static_cast<unsigned long>(hours), static_cast<unsigned long>(minutes));
  } else {
    snprintf(out, outSize, "%02lu:%02lu:%02lu", static_cast<unsigned long>(hours),
             static_cast<unsigned long>(minutes), static_cast<unsigned long>(secs));
  }
}
