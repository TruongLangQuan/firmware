#include "idk_apps.h"
#include <core/mykeyboard.h>
#include <globals.h>

#include <ctype.h>
#include <math.h>
#include <tinyexpr.h>

namespace {
String normalizeExpr(const String &in) {
    String out = "";
    char prev = 0;
    for (int i = 0; i < in.length(); i++) {
        char c = in[i];
        if (isspace(static_cast<unsigned char>(c))) continue;
        bool prevIsNum = (prev >= '0' && prev <= '9') || prev == '.';
        bool prevIsVar = (prev == 'x' || prev == 'X');
        bool prevIsClose = (prev == ')');
        bool curIsVar = (c == 'x' || c == 'X');
        bool curIsOpen = (c == '(');
        bool curIsNum = (c >= '0' && c <= '9') || c == '.';
        if ((prevIsNum || prevIsVar || prevIsClose) && (curIsVar || curIsOpen)) {
            out += '*';
        } else if ((prevIsVar || prevIsClose) && curIsNum) {
            out += '*';
        }
        out += c;
        prev = c;
    }
    return out;
}

void drawPlotArea(const String &expr) {
    const int left = 0;
    const int top = 0;
    const int w = tftWidth;
    const int h = tftHeight;

    tft.fillScreen(TFT_BLACK);

    int cx = left + w / 2;
    int cy = top + h / 2;

    tft.drawFastVLine(cx, top, h, TFT_WHITE);
    tft.drawFastHLine(left, cy, w, TFT_WHITE);

    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(FP);
    tft.setCursor(left + w - 8, cy - 10);
    tft.print("x");
    tft.setCursor(cx + 4, top + 2);
    tft.print("y");

    const int axisMin = -50;
    const int axisMax = 50;
    const int axisRange = axisMax - axisMin;

    for (int x = axisMin; x <= axisMax; x += 10) {
        int px = (int)((x - axisMin) * (w - 1) / (double)axisRange);
        if (px >= 0 && px < w) {
            uint16_t gridColor = (x % 50 == 0) ? 0x39E7 : 0x18C3;
            tft.drawFastVLine(left + px, top, h, gridColor);
        }
    }
    for (int y = axisMin; y <= axisMax; y += 10) {
        int py = cy - (int)(y * ((h - 1) / (double)axisRange));
        if (py >= top && py < top + h) {
            uint16_t gridColor = (y % 50 == 0) ? 0x39E7 : 0x18C3;
            tft.drawFastHLine(left, py, w, gridColor);
        }
    }

    double xval = 0;
    te_variable vars[] = {{"x", &xval}};
    int err = 0;
    String fixed = normalizeExpr(expr);
    te_expr *e = te_compile(fixed.c_str(), vars, 1, &err);
    if (!e) {
        tft.setTextColor(TFT_RED, TFT_BLACK);
        tft.setCursor(left + 6, top + 6);
        tft.print("Parse error");
        return;
    }

    double yRange = h - 1;
    int prevPy = -1;
    for (int px = 0; px < w; px++) {
        double x = ((double)px / (w - 1)) * axisRange + axisMin;
        xval = x;
        double y = te_eval(e);
        if (y < axisMin || y > axisMax) {
            prevPy = -1;
            continue;
        }
        int py = cy - (int)(y * (yRange / (double)axisRange));
        if (py >= top && py < top + h) {
            tft.drawPixel(left + px, py, TFT_YELLOW);
            if (prevPy >= top && prevPy < top + h) {
                tft.drawLine(left + px - 1, prevPy, left + px, py, TFT_YELLOW);
            }
            prevPy = py;
        } else {
            prevPy = -1;
        }
    }
    te_free(e);
}
} // namespace

void idk_firmware_run_plot() {
    returnToMenu = false;

    String expr = keyboard("sin(x)", 32, "f(x)=");
    if (expr == "\x1B") {
        returnToMenu = true;
        return;
    }

    bool redraw = true;

    while (1) {
        InputHandler();
        if (check(EscPress) || check(LongPress)) break;

        if (check(SelPress)) {
            String nextExpr = keyboard(expr, 32, "f(x)=");
            if (nextExpr == "\x1B") break;
            expr = nextExpr;
            redraw = true;
        }

        if (redraw) {
            drawPlotArea(expr);
            redraw = false;
        }

        if (returnToMenu) break;
        delay(10);
    }

    returnToMenu = true;
}
