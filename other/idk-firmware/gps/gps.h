/**
 * GPS Tracker App for M5StickC Plus 2
 * GPS tracking and wardriving functionality
 */

#ifndef __IDK_GPS_H__
#define __IDK_GPS_H__

#include <Arduino.h>

// Use i18n for translations
extern const char* overlay_translate(const char* key);

namespace idk_firmware {

class GpsApp {
public:
    GpsApp() : isTracking(false), latitude(0), longitude(0), altitude(0), satellites(0) {}

    void begin() {
        isTracking = false;
        log_i("GPS app initialized");
    }

    bool startTracking() {
        // In real implementation, initialize GPS hardware
        isTracking = true;
        log_i("GPS tracking started");
        return true;
    }

    void stopTracking() {
        isTracking = false;
        log_i("GPS tracking stopped");
    }

    void update() {
        // Update GPS coordinates
        // In real implementation, read from GPS module
    }

    double getLatitude() const { return latitude; }
    double getLongitude() const { return longitude; }
    double getAltitude() const { return altitude; }
    int getSatellites() const { return satellites; }
    bool isActive() const { return isTracking; }

    const char* getTitle() {
        return "GPS Tracker";
    }

private:
    bool isTracking;
    double latitude;
    double longitude;
    double altitude;
    int satellites;
};

// Global instance
static GpsApp gpsApp;

void gpsBegin() {
    gpsApp.begin();
}

void gpsLoop() {
    if (gpsApp.isActive()) {
        gpsApp.update();
    }
}

} // namespace idk_firmware

#endif // __IDK_GPS_H__
