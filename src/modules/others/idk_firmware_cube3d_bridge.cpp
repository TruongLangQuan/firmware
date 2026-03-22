#include "idk_apps.h"
#include <globals.h>

#include <cstring>
#include <math.h>

namespace {
void drawShapeLabel(int shape, int x, int y) {
    tft.setTextColor(0x7BEF, TFT_BLACK);
    tft.setCursor(x, y);
    if (shape == 0) tft.print("Cube");
    else if (shape == 1) tft.print("Tetra");
    else if (shape == 2) tft.print("Octa");
    else if (shape == 3) tft.print("Prism");
    else if (shape == 4) tft.print("Pyramid");
    else if (shape == 5) tft.print("Icosa");
    else if (shape == 6) tft.print("Dodeca");
    else if (shape == 7) tft.print("Torus");
    else if (shape == 8) tft.print("Sphere");
    else if (shape == 9) tft.print("Cylinder");
    else if (shape == 10) tft.print("Cone");
    else if (shape == 11) tft.print("4D Cube");
    else if (shape == 12) tft.print("4D Simplex");
    else tft.print("4D Cross");
}
} // namespace

void idk_firmware_run_cube3d() {
    returnToMenu = false;
    const int left = 0;
    const int top = 0;
    const int w = tftWidth;
    const int h = tftHeight;
    const int cx = left + w / 2;
    const int cy = top + h / 2;

    float angle = 0.0f;
    int shape = 0;

    auto drawLines = [&](int pts[][2], const int edges[][2], int edgeCount) {
        for (int i = 0; i < edgeCount; i++) {
            int a = edges[i][0], b = edges[i][1];
            tft.drawLine(pts[a][0], pts[a][1], pts[b][0], pts[b][1], TFT_WHITE);
        }
    };

    while (true) {
        InputHandler();
        if (check(EscPress) || check(LongPress)) break;
        if (check(NextPress)) shape = (shape + 1) % 14;

        tft.fillScreen(TFT_BLACK);
        tft.setTextSize(1);
        drawShapeLabel(shape, left + 2, top + 2);

        float s = 22.0f;

        if (shape >= 11) {
            float pts4[16][4];
            int vcount = 0;
            if (shape == 11) {
                for (int xi = 0; xi < 2; xi++) {
                    for (int yi = 0; yi < 2; yi++) {
                        for (int zi = 0; zi < 2; zi++) {
                            for (int wi = 0; wi < 2; wi++) {
                                pts4[vcount][0] = (xi ? s : -s);
                                pts4[vcount][1] = (yi ? s : -s);
                                pts4[vcount][2] = (zi ? s : -s);
                                pts4[vcount][3] = (wi ? s : -s);
                                vcount++;
                            }
                        }
                    }
                }
            } else if (shape == 12) {
                float a = s;
                float verts[5][4] = {
                    {a, 0, 0, -a / 2},
                    {-a, 0, 0, -a / 2},
                    {0, a, 0, -a / 2},
                    {0, -a, 0, -a / 2},
                    {0, 0, a, a},
                };
                for (int i = 0; i < 5; i++) {
                    for (int j = 0; j < 4; j++) pts4[i][j] = verts[i][j];
                }
                vcount = 5;
            } else {
                float verts[8][4] = {
                    {s, 0, 0, 0},
                    {-s, 0, 0, 0},
                    {0, s, 0, 0},
                    {0, -s, 0, 0},
                    {0, 0, s, 0},
                    {0, 0, -s, 0},
                    {0, 0, 0, s},
                    {0, 0, 0, -s},
                };
                for (int i = 0; i < 8; i++) {
                    for (int j = 0; j < 4; j++) pts4[i][j] = verts[i][j];
                }
                vcount = 8;
            }

            float ca = cos(angle), sa = sin(angle);
            float cb = cos(angle * 0.7f), sb = sin(angle * 0.7f);
            for (int i = 0; i < vcount; i++) {
                float x = pts4[i][0], y = pts4[i][1], z = pts4[i][2], w4 = pts4[i][3];
                float x1 = x * ca - w4 * sa;
                float w1 = x * sa + w4 * ca;
                float y1 = y * cb - w1 * sb;
                float w2 = y * sb + w1 * cb;
                pts4[i][0] = x1;
                pts4[i][1] = y1;
                pts4[i][2] = z;
                pts4[i][3] = w2;
            }

            int proj[16][2];
            for (int i = 0; i < vcount; i++) {
                float x = pts4[i][0], y = pts4[i][1], z = pts4[i][2], w4 = pts4[i][3];
                float d4 = 60.0f;
                float f4 = d4 / (d4 - w4);
                x *= f4;
                y *= f4;
                z *= f4;
                float d3 = 80.0f;
                float f3 = d3 / (d3 - z);
                proj[i][0] = cx + (int)(x * f3);
                proj[i][1] = cy + (int)(y * f3);
            }

            if (shape == 11) {
                for (int a = 0; a < vcount; a++) {
                    for (int b = a + 1; b < vcount; b++) {
                        int diff = a ^ b;
                        if ((diff & (diff - 1)) == 0) {
                            tft.drawLine(proj[a][0], proj[a][1], proj[b][0], proj[b][1], TFT_WHITE);
                        }
                    }
                }
            } else if (shape == 12) {
                for (int a = 0; a < vcount; a++) {
                    for (int b = a + 1; b < vcount; b++) {
                        tft.drawLine(proj[a][0], proj[a][1], proj[b][0], proj[b][1], TFT_WHITE);
                    }
                }
            } else {
                for (int a = 0; a < vcount; a++) {
                    for (int b = a + 1; b < vcount; b++) {
                        if (a / 2 != b / 2) {
                            tft.drawLine(proj[a][0], proj[a][1], proj[b][0], proj[b][1], TFT_WHITE);
                        }
                    }
                }
            }
        } else {
            float pts3[12][3];
            int vcount = 0;
            if (shape == 0) {
                float cube[8][3] = {
                    {-s, -s, -s}, {s, -s, -s}, {s, s, -s}, {-s, s, -s},
                    {-s, -s, s},  {s, -s, s},  {s, s, s},  {-s, s, s},
                };
                memcpy(pts3, cube, sizeof(cube));
                vcount = 8;
            } else if (shape == 1) {
                float tet[4][3] = {{s, s, s}, {-s, -s, s}, {-s, s, -s}, {s, -s, -s}};
                memcpy(pts3, tet, sizeof(tet));
                vcount = 4;
            } else if (shape == 2) {
                float oct[6][3] = {{0, 0, s}, {0, 0, -s}, {0, s, 0}, {0, -s, 0}, {s, 0, 0}, {-s, 0, 0}};
                memcpy(pts3, oct, sizeof(oct));
                vcount = 6;
            } else if (shape == 3) {
                float prism[6][3] = {
                    {-s, -s, -s}, {s, -s, -s}, {0, s, -s},
                    {-s, -s, s},  {s, -s, s},  {0, s, s},
                };
                memcpy(pts3, prism, sizeof(prism));
                vcount = 6;
            } else if (shape == 4) {
                float pyr[5][3] = {
                    {-s, -s, -s}, {s, -s, -s}, {s, s, -s}, {-s, s, -s}, {0, 0, s},
                };
                for (int i = 0; i < 5; i++) {
                    for (int j = 0; j < 3; j++) pts3[i][j] = pyr[i][j];
                }
                vcount = 5;
            } else if (shape == 5) {
                float icosa[8][3] = {
                    {-s, s, 0},  {s, s, 0},  {-s, -s, 0}, {s, -s, 0},
                    {0, -s, s}, {0, s, s},  {0, -s, -s}, {0, s, -s},
                };
                for (int i = 0; i < 8; i++) {
                    for (int j = 0; j < 3; j++) pts3[i][j] = icosa[i][j];
                }
                vcount = 8;
            } else if (shape == 6) {
                float t = (1.0f + sqrtf(5.0f)) / 2.0f;
                float dodeca[12][3] = {
                    {s, s, s},    {s, s, -s},   {s, -s, s},   {s, -s, -s},
                    {-s, s, s},   {-s, s, -s},  {-s, -s, s},  {-s, -s, -s},
                    {0, s / t, t * s}, {0, -s / t, t * s}, {t * s, 0, s / t}, {-t * s, 0, s / t},
                };
                for (int i = 0; i < 12; i++) {
                    for (int j = 0; j < 3; j++) pts3[i][j] = dodeca[i][j];
                }
                vcount = 12;
            } else if (shape == 7) {
                int segments = 4;
                for (int i = 0; i < segments; i++) {
                    float a1 = i * 2.0f * M_PI / segments;
                    for (int j = 0; j < 2; j++) {
                        float a2 = j * 2.0f * M_PI / 2.0f;
                        float r = s * 0.6f;
                        float R = s * 1.2f;
                        pts3[i * 2 + j][0] = (R + r * cosf(a2)) * cosf(a1);
                        pts3[i * 2 + j][1] = (R + r * cosf(a2)) * sinf(a1);
                        pts3[i * 2 + j][2] = r * sinf(a2);
                    }
                }
                vcount = segments * 2;
            } else if (shape == 8) {
                int segments = 4;
                for (int i = 0; i < segments; i++) {
                    float lat = (i - segments / 2.0f) * M_PI / segments;
                    for (int j = 0; j < 2; j++) {
                        float lon = j * M_PI;
                        pts3[i * 2 + j][0] = s * cosf(lat) * cosf(lon);
                        pts3[i * 2 + j][1] = s * cosf(lat) * sinf(lon);
                        pts3[i * 2 + j][2] = s * sinf(lat);
                    }
                }
                vcount = segments * 2;
            } else if (shape == 9) {
                int segments = 4;
                for (int i = 0; i < segments; i++) {
                    float a = i * 2.0f * M_PI / segments;
                    pts3[i * 2][0] = s * cosf(a);
                    pts3[i * 2][1] = s * sinf(a);
                    pts3[i * 2][2] = -s;
                    pts3[i * 2 + 1][0] = s * cosf(a);
                    pts3[i * 2 + 1][1] = s * sinf(a);
                    pts3[i * 2 + 1][2] = s;
                }
                vcount = segments * 2;
            } else if (shape == 10) {
                pts3[0][0] = 0;
                pts3[0][1] = 0;
                pts3[0][2] = s * 1.5f;
                int segments = 4;
                for (int i = 0; i < segments; i++) {
                    float a = i * 2.0f * M_PI / segments;
                    pts3[i + 1][0] = s * cosf(a);
                    pts3[i + 1][1] = s * sinf(a);
                    pts3[i + 1][2] = -s;
                }
                vcount = segments + 1;
            }

            float ca = cos(angle), sa = sin(angle);
            float cb = cos(angle * 0.7f), sb = sin(angle * 0.7f);
            for (int i = 0; i < vcount; i++) {
                float x = pts3[i][0];
                float y = pts3[i][1];
                float z = pts3[i][2];
                float x1 = x * ca + z * sa;
                float z1 = -x * sa + z * ca;
                float y1 = y * cb - z1 * sb;
                float z2 = y * sb + z1 * cb;
                pts3[i][0] = x1;
                pts3[i][1] = y1;
                pts3[i][2] = z2;
            }

            int proj[12][2];
            for (int i = 0; i < vcount; i++) {
                float z = pts3[i][2] + 80.0f;
                proj[i][0] = cx + (int)(pts3[i][0] * 80.0f / z);
                proj[i][1] = cy + (int)(pts3[i][1] * 80.0f / z);
            }

            if (shape == 0) {
                const int edges[][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6}, {6, 7}, {7, 4},
                                        {0, 4}, {1, 5}, {2, 6}, {3, 7}};
                drawLines(proj, edges, 12);
            } else if (shape == 1) {
                const int edges[][2] = {{0, 1}, {0, 2}, {0, 3}, {1, 2}, {1, 3}, {2, 3}};
                drawLines(proj, edges, 6);
            } else if (shape == 2) {
                const int edges[][2] = {{0, 2}, {0, 3}, {0, 4}, {0, 5}, {1, 2}, {1, 3}, {1, 4}, {1, 5},
                                        {2, 4}, {2, 5}, {3, 4}, {3, 5}};
                drawLines(proj, edges, 12);
            } else if (shape == 3) {
                const int edges[][2] = {{0, 1}, {1, 2}, {2, 0}, {3, 4}, {4, 5}, {5, 3}, {0, 3}, {1, 4}, {2, 5}};
                drawLines(proj, edges, 9);
            } else if (shape == 4) {
                const int edges[][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 0}, {0, 4}, {1, 4}, {2, 4}, {3, 4}};
                drawLines(proj, edges, 8);
            } else if (shape == 5) {
                for (int i = 0; i < vcount; i++) {
                    for (int j = i + 1; j < vcount; j++) {
                        float dist = sqrtf(powf(pts3[i][0] - pts3[j][0], 2) + powf(pts3[i][1] - pts3[j][1], 2) +
                                            powf(pts3[i][2] - pts3[j][2], 2));
                        if (dist < s * 1.8f) tft.drawLine(proj[i][0], proj[i][1], proj[j][0], proj[j][1], TFT_WHITE);
                    }
                }
            } else if (shape == 6) {
                for (int i = 0; i < vcount; i++) {
                    for (int j = i + 1; j < vcount; j++) {
                        float dist = sqrtf(powf(pts3[i][0] - pts3[j][0], 2) + powf(pts3[i][1] - pts3[j][1], 2) +
                                            powf(pts3[i][2] - pts3[j][2], 2));
                        if (dist < s * 2.2f) tft.drawLine(proj[i][0], proj[i][1], proj[j][0], proj[j][1], TFT_WHITE);
                    }
                }
            } else if (shape == 7) {
                for (int i = 0; i < vcount - 1; i++) {
                    tft.drawLine(proj[i][0], proj[i][1], proj[i + 1][0], proj[i + 1][1], TFT_WHITE);
                }
                tft.drawLine(proj[vcount - 1][0], proj[vcount - 1][1], proj[0][0], proj[0][1], TFT_WHITE);
            } else if (shape == 8) {
                for (int i = 0; i < vcount; i++) {
                    for (int j = i + 1; j < vcount; j++) {
                        float dist = sqrtf(powf(pts3[i][0] - pts3[j][0], 2) + powf(pts3[i][1] - pts3[j][1], 2) +
                                            powf(pts3[i][2] - pts3[j][2], 2));
                        if (dist < s * 1.5f) tft.drawLine(proj[i][0], proj[i][1], proj[j][0], proj[j][1], TFT_WHITE);
                    }
                }
            } else if (shape == 9) {
                for (int i = 0; i < vcount / 2 - 1; i++) {
                    tft.drawLine(proj[i * 2][0], proj[i * 2][1], proj[(i + 1) * 2][0], proj[(i + 1) * 2][1], TFT_WHITE);
                    tft.drawLine(
                        proj[i * 2 + 1][0],
                        proj[i * 2 + 1][1],
                        proj[(i + 1) * 2 + 1][0],
                        proj[(i + 1) * 2 + 1][1],
                        TFT_WHITE
                    );
                    tft.drawLine(proj[i * 2][0], proj[i * 2][1], proj[i * 2 + 1][0], proj[i * 2 + 1][1], TFT_WHITE);
                }
                tft.drawLine(proj[(vcount / 2 - 1) * 2][0], proj[(vcount / 2 - 1) * 2][1], proj[0][0], proj[0][1], TFT_WHITE);
                tft.drawLine(proj[(vcount / 2 - 1) * 2 + 1][0], proj[(vcount / 2 - 1) * 2 + 1][1], proj[1][0], proj[1][1], TFT_WHITE);
            } else if (shape == 10) {
                for (int i = 1; i < vcount; i++) {
                    tft.drawLine(proj[0][0], proj[0][1], proj[i][0], proj[i][1], TFT_WHITE);
                    if (i < vcount - 1) tft.drawLine(proj[i][0], proj[i][1], proj[i + 1][0], proj[i + 1][1], TFT_WHITE);
                }
                tft.drawLine(proj[vcount - 1][0], proj[vcount - 1][1], proj[1][0], proj[1][1], TFT_WHITE);
            }
        }

        angle += 0.04f;
        delay(16);
    }

    returnToMenu = true;
}
