#ifndef API_CLIENT_H
#define API_CLIENT_H

#include <Arduino.h>

struct LTCData {
  float balance = 0.0f;
  float lastMined = 0.0f;
};

struct BTCData {
  String uptime;
  float hashrate = 0.0f;
  float bestDifficulty = 0.0f;
};

class ApiClient {
 public:
  ApiClient(uint32_t timeoutMs, bool insecureTls, size_t responseBufferSize);

  bool fetchLTC(const char* address, LTCData& out, char* errMsg, size_t errMsgSize);
  bool fetchBTC(const char* address, BTCData& out, char* errMsg, size_t errMsgSize);

 private:
  uint32_t timeoutMs_;
  bool insecureTls_;
  size_t responseBufferSize_;

  bool httpGet(const char* url, char* outBuf, size_t outBufSize, int& httpCode, char* errMsg,
               size_t errMsgSize);

  bool parseLtcJson(const char* json, LTCData& out, char* errMsg, size_t errMsgSize);
  bool parseBtcClientJson(const char* json, BTCData& out, char* errMsg, size_t errMsgSize);

  static bool parseIso8601Utc(const char* ts, uint32_t& epochSeconds);
  static uint32_t daysFromCivil(int year, unsigned month, unsigned day);
  static void formatDuration(uint32_t seconds, char* out, size_t outSize);
};

#endif  // API_CLIENT_H
