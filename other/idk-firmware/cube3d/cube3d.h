/**
 * 3D Cube Renderer for M5StickC Plus 2
 * Renders a rotating 3D cube with adjustable parameters
 */

#ifndef __IDK_CUBE3D_H__
#define __IDK_CUBE3D_H__

#include <Arduino.h>
#include <math.h>

// Use i18n for translations
extern const char* overlay_translate(const char* key);

namespace idk_firmware {

struct Point3D {
    float x;
    float y;
    float z;
};

struct Point2D {
    int x;
    int y;
};

class Cube3D {
public:
    Cube3D() 
        : rotationX(0.5f), rotationY(0.5f), rotationZ(0.0f)
        , autoRotate(true), wireframe(true)
        , speed(1), scale(50.0f)
        , centerX(120), centerY(135) {}

    void begin() {
        log_i("3D Cube initialized");
    }

    void setRotationX(float angle) { rotationX = angle; }
    void setRotationY(float angle) { rotationY = angle; }
    void setRotationZ(float angle) { rotationZ = angle; }
    void setAutoRotate(bool autoR) { autoRotate = autoR; }
    void setWireframe(bool wf) { wireframe = wf; }
    void setSpeed(float spd) { speed = spd; }
    void setScale(float sc) { scale = sc; }
    void setCenter(int cx, int cy) { centerX = cx; centerY = cy; }

    float getRotationX() const { return rotationX; }
    float getRotationY() const { return rotationY; }
    float getRotationZ() const { return rotationZ; }
    bool isAutoRotate() const { return autoRotate; }
    bool isWireframe() const { return wireframe; }
    float getSpeed() const { return speed; }

    // Apply rotation transformations
    Point3D rotateX(Point3D p, float angle) {
        float cosA = cos(angle);
        float sinA = sin(angle);
        float y = p.y * cosA - p.z * sinA;
        float z = p.y * sinA + p.z * cosA;
        return {p.x, y, z};
    }

    Point3D rotateY(Point3D p, float angle) {
        float cosA = cos(angle);
        float sinA = sin(angle);
        float x = p.x * cosA + p.z * sinA;
        float z = -p.x * sinA + p.z * cosA;
        return {x, p.y, z};
    }

    Point3D rotateZ(Point3D p, float angle) {
        float cosA = cos(angle);
        float sinA = sin(angle);
        float x = p.x * cosA - p.y * sinA;
        float y = p.x * sinA + p.y * cosA;
        return {x, y, p.z};
    }

    Point3D transform(Point3D p) {
        p = rotateX(p, rotationX);
        p = rotateY(p, rotationY);
        p = rotateZ(p, rotationZ);
        return p;
    }

    // Project 3D point to 2D screen coordinates
    Point2D project(Point3D p) {
        // Simple orthographic projection
        int sx = (int)(p.x * scale) + centerX;
        int sy = (int)(p.y * scale) + centerY;
        return {sx, sy};
    }

    // Get the 8 vertices of a cube centered at origin with size 1
    std::vector<Point3D> getCubeVertices() {
        float s = 0.5f;  // Half-size
        return {
            {-s, -s, -s}, {s, -s, -s}, {s, s, -s}, {-s, s, -s},
            {-s, -s, s},  {s, -s, s},  {s, s, s},  {-s, s, s}
        };
    }

    // Get edges of cube as pairs of vertex indices
    std::vector<std::pair<int, int>> getCubeEdges() {
        return {
            {0, 1}, {1, 2}, {2, 3}, {3, 0},  // Back face
            {4, 5}, {5, 6}, {6, 7}, {7, 4},  // Front face
            {0, 4}, {1, 5}, {2, 6}, {3, 7}   // Connecting edges
        };
    }

    void update() {
        if (autoRotate) {
            rotationY += 0.02f * speed;
            rotationX += 0.01f * speed;
        }
    }

    void reset() {
        rotationX = 0.5f;
        rotationY = 0.5f;
        rotationZ = 0.0f;
    }

    const char* getTitle() {
        return overlay_translate("cube3d.title");
    }

    const char* getRotateXLabel() {
        return overlay_translate("cube3d.rotate_x");
    }

    const char* getRotateYLabel() {
        return overlay_translate("cube3d.rotate_y");
    }

    const char* getRotateZLabel() {
        return overlay_translate("cube3d.rotate_z");
    }

    const char* getResetLabel() {
        return overlay_translate("cube3d.reset");
    }

private:
    float rotationX, rotationY, rotationZ;
    bool autoRotate;
    bool wireframe;
    float speed;
    float scale;
    int centerX, centerY;
};

// Global instance
static Cube3D cube3d;
static bool cube3dInitialized = false;
static bool cube3dAutoToggleLatch = false;

void cube3dBegin() {
    cube3d.begin();
    cube3dInitialized = true;
    tft.fillScreen(TFT_BLACK);
}

void cube3dLoop() {
    if (!cube3dInitialized) cube3dBegin();

    if (check(PrevPress)) cube3d.setSpeed(max(0.2f, cube3d.getSpeed() - 0.1f));
    if (check(NextPress)) cube3d.setSpeed(min(4.0f, cube3d.getSpeed() + 0.1f));
    if (SelPress && !cube3dAutoToggleLatch) {
        cube3d.setAutoRotate(!cube3d.isAutoRotate());
        cube3dAutoToggleLatch = true;
    }
    if (!SelPress) cube3dAutoToggleLatch = false;

    cube3d.update();

    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(1);
    tft.setCursor(2, 2);
    tft.print(cube3d.getTitle());
    tft.setCursor(2, 12);
    tft.printf("SPD %.1f | AUTO %s", cube3d.getSpeed(), cube3d.isAutoRotate() ? "ON" : "OFF");

    std::vector<Point3D> vertices = cube3d.getCubeVertices();
    std::vector<Point2D> projected;
    projected.reserve(vertices.size());
    for (auto &v : vertices) {
        Point3D p = cube3d.transform(v);
        projected.push_back(cube3d.project(p));
    }

    for (const auto &edge : cube3d.getCubeEdges()) {
        const Point2D &a = projected[edge.first];
        const Point2D &b = projected[edge.second];
        tft.drawLine(a.x, a.y, b.x, b.y, TFT_CYAN);
    }

    for (const auto &p : projected) { tft.fillCircle(p.x, p.y, 2, TFT_YELLOW); }
}

} // namespace idk_firmware

#endif // __IDK_CUBE3D_H__
