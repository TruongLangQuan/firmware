#include "idk_apps.h"
#include <globals.h>
#include <interface.h>
#include <interface.h>
#include <Preferences.h>
#include <string.h>

namespace {

Preferences scorePrefs;
const char* SCORE_NS = "scores";

int getScore(const char *key) {
    scorePrefs.begin(SCORE_NS, true);
    int v = scorePrefs.getInt(key, 0);
    scorePrefs.end();
    return v;
}

void setScore(const char *key, int v) {
    scorePrefs.begin(SCORE_NS, false);
    scorePrefs.putInt(key, v);
    scorePrefs.end();
}

struct Tetromino {
    int shape[4][4];
    uint16_t color;
};

const Tetromino TETS[] = {
    {{{0, 0, 0, 0}, {1, 1, 1, 1}, {0, 0, 0, 0}, {0, 0, 0, 0}}, TFT_CYAN   },
    {{{1, 1, 0, 0}, {1, 1, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}}, TFT_YELLOW },
    {{{0, 1, 0, 0}, {1, 1, 1, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}}, TFT_MAGENTA},
    {{{1, 0, 0, 0}, {1, 1, 1, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}}, TFT_ORANGE },
    {{{0, 0, 1, 0}, {1, 1, 1, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}}, TFT_BLUE   },
    {{{0, 1, 1, 0}, {1, 1, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}}, TFT_GREEN  },
    {{{1, 1, 0, 0}, {0, 1, 1, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}}, TFT_RED    }
};

void rotateShape(int s[4][4]) {
    int tmp[4][4];
    for (int y = 0; y < 4; y++) for (int x = 0; x < 4; x++) tmp[y][x] = s[3 - x][y];
    for (int y = 0; y < 4; y++) for (int x = 0; x < 4; x++) s[y][x] = tmp[y][x];
}

} // namespace

void idk_firmware_run_tetris() {
    returnToMenu = false;

    int prevRot = tft.getRotation();
    tft.setRotation(0);
    delay(20);

    int dispW = tft.width();
    int dispH = tft.height();
    const int statusH = 16;

    const int W = 10;
    const int H = 20;
    uint16_t grid[H][W];
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) grid[y][x] = 0;

    int cur = random(0, 7);
    int next = random(0, 7);
    int sx = 3;
    int sy = 0;
    int shape[4][4];
    memcpy(shape, TETS[cur].shape, sizeof(shape));
    uint32_t lastDrop = millis();
    int score = 0;
    int lines = 0;
    int level = 1;
    uint32_t dropInterval = 500;
    int high = getScore("tetris_h");
    bool gameOver = false;
    bool exitGame = false;

    auto darken = [&](uint16_t c, uint8_t shift) -> uint16_t {
        uint8_t r = (c >> 11) & 0x1F;
        uint8_t g = (c >> 5) & 0x3F;
        uint8_t b = c & 0x1F;
        r = (r > shift) ? (r - shift) : 0;
        g = (g > shift) ? (g - (shift * 2)) : 0;
        b = (b > shift) ? (b - shift) : 0;
        return (uint16_t)((r << 11) | (g << 5) | b);
    };

    auto drawStatusT = [&]() {
        tft.fillRect(0, 0, dispW, statusH, TFT_BLACK);
        tft.setTextSize(1);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.setCursor(2, 2);
        tft.print("Bat:");
        int bat = getBattery();
        uint16_t c = (bat <= 20) ? TFT_RED : ((bat <= 50) ? TFT_YELLOW : TFT_GREEN);
        tft.setTextColor(c, TFT_BLACK);
        tft.printf("%d%%", bat);
    };

    auto canPlace = [&](int nx, int ny, int s[4][4]) -> bool {
        for (int y = 0; y < 4; y++) {
            for (int x = 0; x < 4; x++) {
                if (!s[y][x]) continue;
                int gx = nx + x;
                int gy = ny + y;
                if (gx < 0 || gx >= W || gy < 0 || gy >= H) return false;
                if (grid[gy][gx]) return false;
            }
        }
        return true;
    };

    auto lockPiece = [&]() {
        for (int y = 0; y < 4; y++) {
            for (int x = 0; x < 4; x++) {
                if (!shape[y][x]) continue;
                int gx = sx + x;
                int gy = sy + y;
                if (gy >= 0 && gy < H && gx >= 0 && gx < W) grid[gy][gx] = TETS[cur].color;
            }
        }

        int cleared = 0;
        for (int y = H - 1; y >= 0; y--) {
            bool full = true;
            for (int x = 0; x < W; x++) {
                if (!grid[y][x]) {
                    full = false;
                    break;
                }
            }
            if (full) {
                for (int yy = y; yy > 0; yy--) for (int x = 0; x < W; x++) grid[yy][x] = grid[yy - 1][x];
                for (int x = 0; x < W; x++) grid[0][x] = 0;
                y++;
                cleared++;
            }
        }

        if (cleared > 0) {
            lines += cleared;
            level = 1 + lines / 10;
            score += cleared * 100 * level;
            dropInterval = 500 - (level - 1) * 30;
            if (dropInterval < 120) dropInterval = 120;
        }
    };

    auto spawn = [&]() {
        cur = next;
        next = random(0, 7);
        memcpy(shape, TETS[cur].shape, sizeof(shape));
        sx = 3;
        sy = 0;
    };

    static bool prevDownLast = false;
    static bool nextDownLast = false;
    static bool selDownLast = false;

    while (true) {
        InputHandler();
        bool prevHeld = (digitalRead(UP_BTN) == LOW);
        bool nextHeld = (digitalRead(DW_BTN) == LOW);
        bool selHeld = (digitalRead(SEL_BTN) == LOW);
        bool prevEdge = prevHeld && !prevDownLast;
        bool nextEdge = nextHeld && !nextDownLast;
        bool selEdge = selHeld && !selDownLast;

        prevDownLast = prevHeld;
        nextDownLast = nextHeld;
        selDownLast = selHeld;

        if (prevHeld && selHeld) {
            exitGame = true;
            break;
        }

        bool didHardDrop = false;
        if (selHeld && nextEdge) {
            while (canPlace(sx, sy + 1, shape)) sy++;
            lockPiece();
            spawn();
            if (!canPlace(sx, sy, shape)) {
                gameOver = true;
                break;
            }
            didHardDrop = true;
        } else {
            if (prevEdge) {
                if (canPlace(sx - 1, sy, shape)) sx--;
            }
            if (nextEdge) {
                if (canPlace(sx + 1, sy, shape)) sx++;
            }
            if (selEdge && !nextHeld) {
                int tmp[4][4];
                memcpy(tmp, shape, sizeof(tmp));
                rotateShape(tmp);
                if (canPlace(sx, sy, tmp)) memcpy(shape, tmp, sizeof(tmp));
            }
        }

        uint32_t now = millis();
        uint32_t interval = nextHeld ? 80 : dropInterval;
        if (!didHardDrop && now - lastDrop > interval) {
            lastDrop = now;
            if (canPlace(sx, sy + 1, shape)) sy++;
            else {
                lockPiece();
                spawn();
                if (!canPlace(sx, sy, shape)) {
                    gameOver = true;
                    break;
                }
            }
        }

        tft.fillScreen(TFT_BLACK);
        drawStatusT();

        int cell = 10;
        int boardW = W * cell;
        int boardH = H * cell;
        int ox = 2;
        int oy = statusH + 4;
        if (oy + boardH > dispH - 4) {
            cell = 9;
            boardW = W * cell;
            boardH = H * cell;
        }

        tft.drawRect(ox - 2, oy - 2, boardW + 4, boardH + 4, 0x39E7);
        for (int y = 0; y <= H; y++) {
            int yy = oy + y * cell;
            tft.drawFastHLine(ox, yy, boardW, 0x18C3);
        }
        for (int x = 0; x <= W; x++) {
            int xx = ox + x * cell;
            tft.drawFastVLine(xx, oy, boardH, 0x18C3);
        }

        int panelX = ox + boardW + 6;
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.setCursor(panelX, statusH + 6);
        char scoreStr[16];
        snprintf(scoreStr, sizeof(scoreStr), "S:%d", score);
        if (panelX + (int)strlen(scoreStr) * 6 > dispW) {
            snprintf(scoreStr, sizeof(scoreStr), "S:%dK", score / 1000);
        }
        tft.print(scoreStr);
        tft.setCursor(panelX, statusH + 18);
        tft.printf("L:%d", level);
        tft.setCursor(panelX, statusH + 30);
        char highStr[16];
        snprintf(highStr, sizeof(highStr), "H:%d", high);
        if (panelX + (int)strlen(highStr) * 6 > dispW) {
            snprintf(highStr, sizeof(highStr), "H:%dK", high / 1000);
        }
        tft.print(highStr);

        int px0 = panelX;
        int py0 = statusH + 46;
        tft.setTextColor(0x7BEF, TFT_BLACK);
        tft.setCursor(panelX, statusH + 42);
        tft.print("NEXT");
        for (int y = 0; y < 4; y++) {
            for (int x = 0; x < 4; x++) {
                if (!TETS[next].shape[y][x]) continue;
                int px = px0 + x * (cell - 3);
                int py = py0 + y * (cell - 3);
                uint16_t c = TETS[next].color;
                tft.fillRect(px, py, cell - 4, cell - 4, c);
                tft.drawRect(px, py, cell - 4, cell - 4, darken(c, 4));
            }
        }

        for (int y = 0; y < H; y++) {
            for (int x = 0; x < W; x++) {
                if (!grid[y][x]) continue;
                int px = ox + x * cell + 1;
                int py = oy + y * cell + 1;
                uint16_t c = grid[y][x];
                tft.fillRect(px, py, cell - 2, cell - 2, c);
                tft.drawRect(px, py, cell - 2, cell - 2, darken(c, 4));
                tft.drawFastHLine(px + 1, py + 1, cell - 4, TFT_WHITE);
            }
        }

        for (int y = 0; y < 4; y++) {
            for (int x = 0; x < 4; x++) {
                if (!shape[y][x]) continue;
                int px = ox + (sx + x) * cell + 1;
                int py = oy + (sy + y) * cell + 1;
                uint16_t c = TETS[cur].color;
                tft.fillRect(px, py, cell - 2, cell - 2, c);
                tft.drawRect(px, py, cell - 2, cell - 2, darken(c, 4));
                tft.drawFastHLine(px + 1, py + 1, cell - 4, TFT_WHITE);
            }
        }
        delay(10);
    }

    setScore("tetris_last", score);
    if (gameOver && score > high) setScore("tetris_h", score);
    tft.setRotation(prevRot);
    returnToMenu = true;
    (void)exitGame;
}
