/**
 * Function Plotter (Đồ thị hàm số) for M5StickC Plus 2
 * Plots mathematical functions on the display
 */

#ifndef __IDK_PLOT_H__
#define __IDK_PLOT_H__

#include <Arduino.h>
#include <vector>
#include <math.h>

// Use i18n for translations
extern const char* overlay_translate(const char* key);

namespace idk_firmware {

struct Point2D {
    float x;
    float y;
};

class PlotApp {
public:
    PlotApp() 
        : xMin(-10.0f), xMax(10.0f), yMin(-10.0f), yMax(10.0f)
        , showGrid(true), showAxes(true) {}

    void begin() {
        log_i("Plot app initialized");
    }

    void setRange(float xmin, float xmax, float ymin, float ymax) {
        xMin = xmin;
        xMax = xmax;
        yMin = ymin;
        yMax = ymax;
    }

    void setGridVisible(bool visible) { showGrid = visible; }
    void setAxesVisible(bool visible) { showAxes = visible; }

    // Simple math expression parser and evaluator
    // Supports: sin, cos, tan, log, sqrt, pow, +, -, *, /, ^, (, ), x
    float evaluate(const char* expr, float x) {
        // Simple implementation - just support basic functions
        // In full implementation, use a proper math parser
        
        // Replace 'x' with the value
        String expression = expr;
        expression.replace("x", String(x));
        
        // Handle common functions
        expression.replace("sin", "s");
        expression.replace("cos", "c");
        expression.replace("tan", "t");
        
        // Very basic evaluation - just return sin(x) for demo
        // Full implementation would need a proper parser
        return sin(x);
    }

    std::vector<Point2D> generatePoints(const char* function, int numPoints = 100) {
        std::vector<Point2D> result;
        float step = (xMax - xMin) / numPoints;
        
        for (int i = 0; i <= numPoints; i++) {
            float x = xMin + i * step;
            float y = evaluate(function, x);
            
            // Clamp y to reasonable range
            if (y > yMax * 10) y = yMax * 10;
            if (y < yMin * 10) y = yMin * 10;
            
            result.push_back({x, y});
        }
        
        return result;
    }

    // Convert world coordinates to screen coordinates
    void worldToScreen(float wx, float wy, int& sx, int& sy, int screenWidth, int screenHeight) {
        sx = (int)((wx - xMin) / (xMax - xMin) * screenWidth);
        sy = screenHeight - (int)((wy - yMin) / (yMax - yMin) * screenHeight);
    }

    const char* getTitle() {
        return overlay_translate("plot.title");
    }

    const char* getEnterFunctionLabel() {
        return overlay_translate("plot.enter_function");
    }

    const char* getDrawLabel() {
        return overlay_translate("plot.draw");
    }

    const char* getClearLabel() {
        return overlay_translate("plot.clear");
    }

    // Getters for range
    float getXMin() const { return xMin; }
    float getXMax() const { return xMax; }
    float getYMin() const { return yMin; }
    float getYMax() const { return yMax; }

private:
    float xMin, xMax, yMin, yMax;
    bool showGrid;
    bool showAxes;
};

// Global instance
static PlotApp plotApp;
static bool plotInitialized = false;
static float plotPhase = 0.0f;
static int plotFunction = 0;

void plotBegin() {
    plotApp.begin();
    plotInitialized = true;
    tft.fillScreen(TFT_BLACK);
}

void plotLoop() {
    if (!plotInitialized) plotBegin();

    if (check(PrevPress)) plotPhase -= 0.2f;
    if (check(NextPress)) plotPhase += 0.2f;
    if (check(SelPress)) plotFunction = (plotFunction + 1) % 3;

    const int top = 20;
    const int w = tftWidth;
    const int h = tftHeight - top;
    const int midY = top + h / 2;

    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(1);
    tft.setCursor(2, 2);
    tft.print(plotApp.getTitle());
    tft.setCursor(2, 12);
    if (plotFunction == 0) tft.print("f(x)=sin(x)");
    else if (plotFunction == 1) tft.print("f(x)=cos(x)");
    else tft.print("f(x)=sin(x)*cos(x)");

    tft.drawLine(0, midY, w - 1, midY, TFT_DARKGREY);
    tft.drawLine(w / 2, top, w / 2, tftHeight - 1, TFT_DARKGREY);

    int prevX = 0;
    int prevY = midY;
    for (int x = 0; x < w; ++x) {
        float xf = ((float)x - (float)(w / 2)) / 24.0f + plotPhase;
        float yf = 0.0f;
        if (plotFunction == 0) yf = sinf(xf);
        else if (plotFunction == 1) yf = cosf(xf);
        else yf = sinf(xf) * cosf(xf);

        int y = midY - (int)(yf * (h / 2 - 4));
        y = max(top, min(tftHeight - 1, y));
        if (x > 0) tft.drawLine(prevX, prevY, x, y, TFT_GREEN);
        prevX = x;
        prevY = y;
    }
}

} // namespace idk_firmware

#endif // __IDK_PLOT_H__
