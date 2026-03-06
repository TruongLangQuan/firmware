/**
 * Drawing App for M5StickC Plus 2
 * Simple drawing application with touch/stick input support
 */

#ifndef __IDK_DRAWING_H__
#define __IDK_DRAWING_H__

#include <Arduino.h>
#include <vector>

// Use i18n for translations
extern const char* overlay_translate(const char* key);

namespace idk_firmware {

struct Point {
    int x;
    int y;
};

struct DrawingTool {
    const char* name;
    uint16_t color;
    int size;
};

class DrawingApp {
public:
    DrawingApp() : currentTool(0), brushSize(2), currentColor(TFT_WHITE), isDrawing(false) {
        // Initialize tools
        tools.push_back({"Pencil", TFT_WHITE, 2});
        tools.push_back({"Line", TFT_CYAN, 1});
        tools.push_back({"Rectangle", TFT_GREEN, 1});
        tools.push_back({"Circle", TFT_MAGENTA, 1});
    }

    void begin() {
        clearCanvas();
        log_i("Drawing app initialized");
    }

    void clearCanvas() {
        // Fill with black
        // In real implementation: tft.fillScreen(TFT_BLACK);
        points.clear();
    }

    void setTool(int index) {
        if (index >= 0 && index < tools.size()) {
            currentTool = index;
            currentColor = tools[index].color;
            brushSize = tools[index].size;
        }
    }

    void setColor(uint16_t color) {
        currentColor = color;
    }

    void setBrushSize(int size) {
        brushSize = size;
    }

    void startDrawing(int x, int y) {
        isDrawing = true;
        lastPoint = {x, y};
    }

    void continueDrawing(int x, int y) {
        if (!isDrawing) return;
        
        switch (currentTool) {
            case 0: // Pencil - draw points
                points.push_back({x, y});
                break;
            case 1: // Line - store start and end
                // In full implementation, draw line from lastPoint to current
                break;
            case 2: // Rectangle
            case 3: // Circle
                // Store shape parameters
                break;
        }
        lastPoint = {x, y};
    }

    void stopDrawing(int x, int y) {
        isDrawing = false;
    }

    const char* getTitle() {
        return overlay_translate("drawing.title");
    }

    const char* getToolName() {
        return tools[currentTool].name;
    }

    const char* getClearLabel() {
        return overlay_translate("drawing.clear");
    }

private:
    std::vector<Point> points;
    std::vector<DrawingTool> tools;
    int currentTool;
    int brushSize;
    uint16_t currentColor;
    bool isDrawing;
    Point lastPoint;
};

// Global instance
static DrawingApp drawingApp;

void drawingBegin() {
    drawingApp.begin();
}

void drawingLoop() {
    // Handle input and drawing
    // This would integrate with M5StickC input handling
}

} // namespace idk_firmware

#endif // __IDK_DRAWING_H__
