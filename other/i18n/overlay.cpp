#include <cstring>

struct I18nEntry {
    const char *key;
    const char *en;
    const char *vi;
};

static const I18nEntry kI18nEntries[] = {
    {"drawing.title", "Drawing", "Vẽ"},
    {"drawing.clear", "Clear", "Xóa"},
    {"plot.title", "Plot", "Đồ thị"},
    {"plot.enter_function", "Enter function", "Nhập hàm"},
    {"plot.draw", "Draw", "Vẽ"},
    {"plot.clear", "Clear", "Xóa"},
    {"cube3d.title", "3D Cube", "Khối 3D"},
    {"cube3d.rotate_x", "Rotate X", "Xoay X"},
    {"cube3d.rotate_y", "Rotate Y", "Xoay Y"},
    {"cube3d.rotate_z", "Rotate Z", "Xoay Z"},
    {"cube3d.reset", "Reset", "Đặt lại"},
};

const char *tr(const char *key) {
    if (key == nullptr) return "";

    for (const auto &entry : kI18nEntries) {
        if (std::strcmp(entry.key, key) == 0) {
            if (entry.vi != nullptr && entry.vi[0] != '\0') return entry.vi;
            if (entry.en != nullptr && entry.en[0] != '\0') return entry.en;
            break;
        }
    }

    // Fallback to English key token when translation is missing.
    return key;
}

const char *overlay_translate(const char *key) { return tr(key); }
