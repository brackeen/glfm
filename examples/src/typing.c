/// This example demonstrates character input, key codes, and the virtual keyboard.
/// * iOS/Android: Tap to show the virtual keyboard.
/// * Caveat: This example uses an ASCII-only font.
/// Devices with a physical keyboard:
/// * Ctrl-M to switch to KeyCode mode.
/// * Ctrl-L to clear the screen.
/// Tips:
/// * iOS Simulator: Toggle "I/O -> Keyboard -> Connect Hardware Keyboard" to test with it both
///   enabled (physical keyboard) and disabled (virtual keyboard).
/// * Devices with a USB-C port: Connect a physical keyboard directly (no need to use Bluetooth).
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "glfm.h"
#define FILE_COMPAT_ANDROID_ACTIVITY glfmAndroidGetActivity()
#include "file_compat.h"

enum {
    CONSOLE_COLS = 22,
    CONSOLE_MAX_LINES = 40,
    CONSOLE_MAX_SCALE = 3,

    FONT_CHAR_FIRST = ' ',
    FONT_CHAR_COUNT = 96,
    FONT_CHAR_WIDTH = 6,
    FONT_CHAR_HEIGHT = 13,

    TEXTURE_CHARS_X = 8,
    TEXTURE_CHARS_Y = (FONT_CHAR_COUNT + TEXTURE_CHARS_X - 1) / TEXTURE_CHARS_X,
    TEXTURE_SPACING = 1, // Prevent bleeding
};

// Cozette font converted to bitmap via Image Magick
static const uint8_t FONT_DATA[FONT_CHAR_COUNT][FONT_CHAR_HEIGHT] = {
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x00, 0x08, 0x00, 0x00, 0x00 },
    { 0x00, 0x14, 0x14, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x14, 0x14, 0x3E, 0x14, 0x14, 0x3E, 0x14, 0x14, 0x00, 0x00, 0x00 },
    { 0x00, 0x08, 0x1C, 0x2A, 0x0A, 0x1C, 0x28, 0x28, 0x2A, 0x1C, 0x08, 0x00, 0x00 },
    { 0x00, 0x04, 0x0A, 0x24, 0x10, 0x08, 0x04, 0x12, 0x28, 0x10, 0x00, 0x00, 0x00 },
    { 0x00, 0x08, 0x14, 0x14, 0x08, 0x2C, 0x12, 0x12, 0x12, 0x2C, 0x00, 0x00, 0x00 },
    { 0x00, 0x08, 0x08, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    { 0x00, 0x10, 0x08, 0x08, 0x04, 0x04, 0x04, 0x04, 0x04, 0x08, 0x08, 0x10, 0x00 },
    { 0x00, 0x04, 0x08, 0x08, 0x10, 0x10, 0x10, 0x10, 0x10, 0x08, 0x08, 0x04, 0x00 },
    { 0x00, 0x00, 0x00, 0x00, 0x14, 0x08, 0x3E, 0x08, 0x14, 0x00, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x00, 0x00, 0x08, 0x08, 0x3E, 0x08, 0x08, 0x00, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C, 0x08, 0x04, 0x00 },
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C, 0x00, 0x00, 0x00 },
    { 0x00, 0x20, 0x20, 0x10, 0x10, 0x08, 0x08, 0x04, 0x04, 0x02, 0x02, 0x00, 0x00 },
    { 0x00, 0x00, 0x1C, 0x22, 0x22, 0x2A, 0x2A, 0x22, 0x22, 0x1C, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x08, 0x0C, 0x0A, 0x08, 0x08, 0x08, 0x08, 0x3E, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x1C, 0x22, 0x20, 0x10, 0x08, 0x04, 0x02, 0x3E, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x1C, 0x22, 0x20, 0x18, 0x20, 0x20, 0x22, 0x1C, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x20, 0x30, 0x28, 0x24, 0x22, 0x7E, 0x20, 0x20, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x3E, 0x02, 0x02, 0x1E, 0x20, 0x20, 0x22, 0x1C, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x18, 0x04, 0x02, 0x1E, 0x22, 0x22, 0x22, 0x1C, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x3E, 0x20, 0x10, 0x10, 0x08, 0x08, 0x04, 0x04, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x1C, 0x22, 0x22, 0x1C, 0x22, 0x22, 0x22, 0x1C, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x1C, 0x22, 0x22, 0x22, 0x3C, 0x20, 0x10, 0x0C, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C, 0x00, 0x00, 0x0C, 0x0C, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C, 0x00, 0x00, 0x0C, 0x0C, 0x08, 0x04, 0x00 },
    { 0x00, 0x00, 0x00, 0x20, 0x10, 0x08, 0x04, 0x08, 0x10, 0x20, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x3E, 0x00, 0x3E, 0x00, 0x00, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x00, 0x02, 0x04, 0x08, 0x10, 0x08, 0x04, 0x02, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x1C, 0x22, 0x20, 0x10, 0x08, 0x08, 0x00, 0x08, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x1C, 0x22, 0x22, 0x3A, 0x2A, 0x3A, 0x02, 0x3C, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x1C, 0x22, 0x22, 0x22, 0x3E, 0x22, 0x22, 0x22, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x1E, 0x22, 0x22, 0x1E, 0x22, 0x22, 0x22, 0x1E, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x1C, 0x22, 0x02, 0x02, 0x02, 0x02, 0x22, 0x1C, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x0E, 0x12, 0x22, 0x22, 0x22, 0x22, 0x12, 0x0E, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x3E, 0x02, 0x02, 0x1E, 0x02, 0x02, 0x02, 0x3E, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x3E, 0x02, 0x02, 0x1E, 0x02, 0x02, 0x02, 0x02, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x1C, 0x22, 0x02, 0x02, 0x32, 0x22, 0x22, 0x1C, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x22, 0x22, 0x22, 0x3E, 0x22, 0x22, 0x22, 0x22, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x1C, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x1C, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x38, 0x20, 0x20, 0x20, 0x20, 0x22, 0x22, 0x1C, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x22, 0x12, 0x0A, 0x0E, 0x12, 0x12, 0x22, 0x22, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x3E, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x22, 0x36, 0x2A, 0x2A, 0x22, 0x22, 0x22, 0x22, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x22, 0x26, 0x26, 0x2A, 0x2A, 0x32, 0x32, 0x22, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x1C, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x1C, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x1E, 0x22, 0x22, 0x22, 0x1E, 0x02, 0x02, 0x02, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x1C, 0x22, 0x22, 0x22, 0x22, 0x22, 0x12, 0x2C, 0x20, 0x00, 0x00 },
    { 0x00, 0x00, 0x1E, 0x22, 0x22, 0x1E, 0x12, 0x22, 0x22, 0x22, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x1C, 0x22, 0x02, 0x1C, 0x20, 0x20, 0x22, 0x1C, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x3E, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x1C, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x22, 0x22, 0x22, 0x14, 0x14, 0x14, 0x08, 0x08, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x22, 0x22, 0x22, 0x2A, 0x2A, 0x1C, 0x14, 0x14, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x22, 0x22, 0x14, 0x08, 0x08, 0x14, 0x22, 0x22, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x22, 0x22, 0x22, 0x14, 0x08, 0x08, 0x08, 0x08, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x3E, 0x10, 0x08, 0x08, 0x04, 0x04, 0x02, 0x3E, 0x00, 0x00, 0x00 },
    { 0x00, 0x1C, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x1C, 0x00 },
    { 0x00, 0x02, 0x02, 0x04, 0x04, 0x08, 0x08, 0x10, 0x10, 0x20, 0x20, 0x00, 0x00 },
    { 0x00, 0x1C, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1C, 0x00 },
    { 0x08, 0x14, 0x22, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3E, 0x00, 0x00 },
    { 0x00, 0x04, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x00, 0x00, 0x3C, 0x22, 0x22, 0x22, 0x32, 0x2C, 0x00, 0x00, 0x00 },
    { 0x00, 0x02, 0x02, 0x02, 0x1E, 0x22, 0x22, 0x22, 0x22, 0x1E, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x00, 0x00, 0x1C, 0x22, 0x02, 0x02, 0x22, 0x1C, 0x00, 0x00, 0x00 },
    { 0x00, 0x20, 0x20, 0x20, 0x3C, 0x22, 0x22, 0x22, 0x22, 0x3C, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x00, 0x00, 0x1C, 0x22, 0x3E, 0x02, 0x22, 0x1C, 0x00, 0x00, 0x00 },
    { 0x00, 0x38, 0x04, 0x04, 0x1E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x00, 0x00, 0x3C, 0x22, 0x22, 0x22, 0x22, 0x3C, 0x20, 0x20, 0x1C },
    { 0x00, 0x02, 0x02, 0x02, 0x1E, 0x22, 0x22, 0x22, 0x22, 0x22, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x08, 0x00, 0x0C, 0x08, 0x08, 0x08, 0x08, 0x30, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x10, 0x00, 0x18, 0x10, 0x10, 0x10, 0x10, 0x10, 0x14, 0x08, 0x00 },
    { 0x00, 0x02, 0x02, 0x02, 0x22, 0x12, 0x0A, 0x0E, 0x12, 0x22, 0x00, 0x00, 0x00 },
    { 0x00, 0x0C, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x18, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x00, 0x00, 0x16, 0x2A, 0x2A, 0x2A, 0x2A, 0x2A, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x00, 0x00, 0x1E, 0x22, 0x22, 0x22, 0x22, 0x22, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x00, 0x00, 0x1C, 0x22, 0x22, 0x22, 0x22, 0x1C, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x00, 0x00, 0x1E, 0x22, 0x22, 0x22, 0x22, 0x1E, 0x02, 0x02, 0x02 },
    { 0x00, 0x00, 0x00, 0x00, 0x3C, 0x22, 0x22, 0x22, 0x22, 0x3C, 0x20, 0x20, 0x60 },
    { 0x00, 0x00, 0x00, 0x00, 0x1E, 0x22, 0x02, 0x02, 0x02, 0x02, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x00, 0x00, 0x3C, 0x02, 0x1C, 0x20, 0x20, 0x1E, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x04, 0x04, 0x1E, 0x04, 0x04, 0x04, 0x04, 0x38, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x00, 0x00, 0x22, 0x22, 0x22, 0x22, 0x22, 0x3C, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x00, 0x00, 0x22, 0x22, 0x14, 0x14, 0x08, 0x08, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x00, 0x00, 0x22, 0x22, 0x2A, 0x2A, 0x14, 0x14, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x00, 0x00, 0x22, 0x14, 0x08, 0x08, 0x14, 0x22, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x00, 0x00, 0x22, 0x22, 0x22, 0x22, 0x22, 0x3C, 0x20, 0x20, 0x1C },
    { 0x00, 0x00, 0x00, 0x00, 0x3E, 0x10, 0x08, 0x04, 0x02, 0x3E, 0x00, 0x00, 0x00 },
    { 0x00, 0x30, 0x08, 0x08, 0x08, 0x08, 0x06, 0x08, 0x08, 0x08, 0x08, 0x30, 0x00 },
    { 0x00, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x00 },
    { 0x00, 0x06, 0x08, 0x08, 0x08, 0x08, 0x30, 0x08, 0x08, 0x08, 0x08, 0x06, 0x00 },
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x24, 0x2A, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
};

typedef struct {
    GLuint program;
    GLuint vertexArray;
    GLuint positionBuffer;
    GLuint texCoordBuffer;
    GLuint indexBuffer;
    GLuint texture;

    GLfloat texCoords[CONSOLE_MAX_LINES * CONSOLE_COLS * 4 * 2];

    unsigned char console[CONSOLE_MAX_LINES][CONSOLE_COLS];
    size_t consoleLineFirst;
    size_t consoleLineCount;
    size_t consoleCol;

    size_t bottomSpacingRequested;
    size_t bottomSpacingActual;

    double cursorBlinkStartTime;
    bool focused;
    bool keyCodeMode;
} TypingApp;

static void consoleNewline(TypingApp *app) {
    app->cursorBlinkStartTime = glfmGetTime();
    if (app->consoleLineCount < CONSOLE_MAX_LINES) {
        app->consoleLineCount++;
    }
    app->consoleLineFirst = (app->consoleLineFirst + CONSOLE_MAX_LINES - 1) % CONSOLE_MAX_LINES;
    app->consoleCol = 0;
    memset(app->console[app->consoleLineFirst], 0, CONSOLE_COLS);
}

static void consoleBackspace(TypingApp *app) {
    app->cursorBlinkStartTime = glfmGetTime();
    if (app->consoleLineCount > 0) {
        if (app->consoleCol > 0) {
            app->console[app->consoleLineFirst][--app->consoleCol] = 0;
        } else if (app->consoleLineCount > 1) {
            app->consoleLineFirst = (app->consoleLineFirst + 1) % CONSOLE_MAX_LINES;
            app->consoleLineCount--;
            app->consoleCol = CONSOLE_COLS - 1;
            app->console[app->consoleLineFirst][app->consoleCol] = 0;
            while (app->consoleCol > 0 && app->console[app->consoleLineFirst][app->consoleCol - 1] == 0) {
                app->consoleCol--; // Find EOL
            }
        }
    }
}

static void consolePrint(TypingApp *app, const char *utf8) {
    app->cursorBlinkStartTime = glfmGetTime();
    if (app->consoleLineCount == 0) {
        app->consoleLineCount = 1;
        app->consoleCol = 0;
        memset(app->console[app->consoleLineFirst], 0, CONSOLE_COLS);
    }
    while (*utf8) {
        unsigned char ch = *utf8++;
        if (ch == '\n') {
            consoleNewline(app);
        } else {
            if (ch < FONT_CHAR_FIRST || ch >= FONT_CHAR_FIRST + FONT_CHAR_COUNT) {
                ch = '?';
            }
            app->console[app->consoleLineFirst][app->consoleCol++] = ch;
            if (app->consoleCol >= CONSOLE_COLS) {
                consoleNewline(app);
            }
        }
    }
}

static void consoleClear(TypingApp *app) {
    app->consoleLineCount = 0;
    consolePrint(app, "");
}

static double consoleGetScale(const GLFMDisplay *display) {
    // Center horizontally with one column of spacing on either side. Shrink if needed.
    int width, height;
    glfmGetDisplaySize(display, &width, &height);
    double consoleWidth = FONT_CHAR_WIDTH * (CONSOLE_COLS + 2);
    double maxConsoleWidth = CONSOLE_MAX_SCALE * glfmGetDisplayScale(display) * consoleWidth;
    double scaleX = (width > maxConsoleWidth) ? maxConsoleWidth / width : 1.0;
    return scaleX * width / consoleWidth;
}

static void onKeyboardVisibilityChanged(GLFMDisplay *display, bool visible,
                                        double x, double y, double width, double height) {
    // Assume virtual keyboard is at the bottom of the screen
    double scale = consoleGetScale(display);
    double lineHeight = FONT_CHAR_HEIGHT * scale;
    TypingApp *app = glfmGetUserData(display);
    app->bottomSpacingRequested = visible ? (size_t)ceil(height / lineHeight) : 0;

    // Check bottom insets
    double bottom;
    glfmGetDisplayChromeInsets(display, NULL, NULL, &bottom, NULL);
    size_t minimumBottomSpace = 1 + (size_t)floor(bottom / lineHeight);
    if (app->bottomSpacingRequested < minimumBottomSpace) {
        app->bottomSpacingRequested = minimumBottomSpace;
    }
}

static bool onTouch(GLFMDisplay *display, int touch, GLFMTouchPhase phase, double x, double y) {
    if (phase == GLFMTouchPhaseBegan) {
        glfmSetKeyboardVisible(display, !glfmIsKeyboardVisible(display));
        return true;
    } else {
        return false;
    }
}

static void onChar(GLFMDisplay *display, const char *utf8, int modifiers) {
    TypingApp *app = glfmGetUserData(display);
    consolePrint(app, utf8);
}

static bool onKey(GLFMDisplay *display, GLFMKeyCode keyCode, GLFMKeyAction action, int modifiers) {
    TypingApp *app = glfmGetUserData(display);
    if (action == GLFMKeyActionPressed) {
        if (keyCode == GLFMKeyCodeL && modifiers == GLFMKeyModifierControl) {
            consoleClear(app);
            return true;
        } else if (keyCode == GLFMKeyCodeM && modifiers == GLFMKeyModifierControl) {
            app->keyCodeMode = !app->keyCodeMode;
            if (app->consoleCol > 0) {
                consoleNewline(app);
            }
            if (app->keyCodeMode) {
                consolePrint(app, "KeyCode mode: on\n");
                glfmSetCharFunc(display, NULL);
            } else {
                consolePrint(app, "KeyCode mode: off\n");
                glfmSetCharFunc(display, onChar);
            }
            return true;
        }
    }
    if (app->keyCodeMode) {
        char line[256];
        snprintf(line, sizeof(line), "Key 0x%x %s\n", keyCode,
                 action == GLFMKeyActionPressed ? "pressed" :
                 action == GLFMKeyActionRepeated ? "repeated" : "released");
        consolePrint(app, line);
        return true;
    } else {
        if (action == GLFMKeyActionPressed || action == GLFMKeyActionRepeated) {
            if (keyCode == GLFMKeyCodeEnter || keyCode == GLFMKeyCodeNumpadEnter) {
                consoleNewline(app);
                return true;
            } else if (keyCode == GLFMKeyCodeBackspace) {
                consoleBackspace(app);
                return true;
            }
        }
        return false;
    }
}

static GLuint compileShader(GLenum type, const char *shaderName) {
    char fullPath[PATH_MAX];
    fc_resdir(fullPath, sizeof(fullPath));
    strncat(fullPath, shaderName, sizeof(fullPath) - strlen(fullPath) - 1);

    // Get shader string
    char *shaderString = NULL;
    FILE *shaderFile = fopen(fullPath, "rb");
    if (shaderFile) {
        fseek(shaderFile, 0, SEEK_END);
        long length = ftell(shaderFile);
        fseek(shaderFile, 0, SEEK_SET);

        shaderString = malloc(length + 1);
        if (shaderString) {
            fread(shaderString, length, 1, shaderFile);
            shaderString[length] = 0;
        }
        fclose(shaderFile);
    }
    if (!shaderString) {
        printf("Couldn't read file: %s\n", fullPath);
        return 0;
    }

    // Compile
    const char *constShaderString = shaderString;
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &constShaderString, NULL);
    glCompileShader(shader);
    free(shaderString);

    // Check compile status
    GLint status;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (status == 0) {
        printf("Couldn't compile shader: %s\n", shaderName);
        GLint logLength;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
        if (logLength > 0) {
            GLchar *log = malloc(logLength);
            glGetShaderInfoLog(shader, logLength, &logLength, log);
            if (log[0] != 0) {
                printf("Shader log: %s\n", log);
            }
            free(log);
        }
        glDeleteShader(shader);
        shader = 0;
    }
    return shader;
}

static void onFocus(GLFMDisplay *display, bool focused) {
    TypingApp *app = glfmGetUserData(display);
    app->focused = focused;
    app->cursorBlinkStartTime = glfmGetTime();
}

static void onSurfaceCreatedOrResized(GLFMDisplay *display, int width, int height) {
    TypingApp *app = glfmGetUserData(display);
    double scale = consoleGetScale(display);

    // Set minimum bottom space
    {
        double bottom;
        glfmGetDisplayChromeInsets(display, NULL, NULL, &bottom, NULL);
        double lineHeight = FONT_CHAR_HEIGHT * scale;
        size_t minimumBottomSpace = 1 + (size_t)floor(bottom / lineHeight);
        if (app->bottomSpacingRequested < minimumBottomSpace) {
            app->bottomSpacingRequested = minimumBottomSpace;
        }
    }

    // Create shader
    if (app->program == 0) {
        GLuint vertShader = compileShader(GL_VERTEX_SHADER, "texture.vert");
        GLuint fragShader = compileShader(GL_FRAGMENT_SHADER, "texture.frag");
        if (vertShader == 0 || fragShader == 0) {
            return;
        }
        app->program = glCreateProgram();

        glAttachShader(app->program, vertShader);
        glAttachShader(app->program, fragShader);

        glBindAttribLocation(app->program, 0, "position");
        glBindAttribLocation(app->program, 1, "texCoord");

        glLinkProgram(app->program);

        glDeleteShader(vertShader);
        glDeleteShader(fragShader);
    }

    // Create font texture
    if (app->texture == 0) {
        GLsizei textureWidth = TEXTURE_CHARS_X * (FONT_CHAR_WIDTH + TEXTURE_SPACING);
        GLsizei textureHeight = TEXTURE_CHARS_Y * (FONT_CHAR_HEIGHT + TEXTURE_SPACING);
        size_t bpp = 4;
        size_t stride = textureWidth * bpp;
        uint8_t *textureData = malloc(stride * textureHeight);
        for (size_t ch = 0; ch < FONT_CHAR_COUNT; ch++) {
            size_t offset = ((ch % TEXTURE_CHARS_X) * (FONT_CHAR_WIDTH + TEXTURE_SPACING) * bpp +
                             (ch / TEXTURE_CHARS_X) * (FONT_CHAR_HEIGHT + TEXTURE_SPACING) * stride);
            for (GLsizei y = 0; y < FONT_CHAR_HEIGHT; y++) {
                unsigned int row = FONT_DATA[ch][FONT_CHAR_HEIGHT - y - 1];
                for (GLsizei x = 0; x < FONT_CHAR_WIDTH; x++) {
                    GLubyte b = ((row >> x) & 1);
                    textureData[offset++] = b * 0xdd;
                    textureData[offset++] = b * 0xdf;
                    textureData[offset++] = b * 0xe4;
                    textureData[offset++] = b * 0xff;
                }
                memset(textureData + offset, 0, TEXTURE_SPACING * bpp);
                offset += stride - bpp * FONT_CHAR_WIDTH;
            }
            memset(textureData + offset, 0, (FONT_CHAR_WIDTH + TEXTURE_SPACING) * bpp);
        }

        glGenTextures(1, &app->texture);
        glBindTexture(GL_TEXTURE_2D, app->texture);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, textureWidth, textureHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, textureData);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        free(textureData);
    }

#if defined(GL_VERSION_3_0) && GL_VERSION_3_0
    if (app->vertexArray == 0) {
        glGenVertexArrays(1, &app->vertexArray);
    }
    glBindVertexArray(app->vertexArray);
#endif

    // Create position buffer (re-layout when display size changes)
    {
        float charDX = (float)(2.0 * FONT_CHAR_WIDTH * scale / width);
        float charDY = (float)(2.0 * FONT_CHAR_HEIGHT * scale / height);
        float offsetX = (float)(-CONSOLE_COLS * FONT_CHAR_WIDTH * scale / width);
        float offsetY = -1.0f;
        size_t positionSize = sizeof(GLfloat) * 2;
        size_t positionCount = CONSOLE_MAX_LINES * CONSOLE_COLS * 4;
        GLfloat *positions = malloc(positionSize * positionCount);
        size_t i = 0;
        for (size_t line = 0; line < CONSOLE_MAX_LINES; line++) {
            float y0 = offsetY + charDY * line;
            float y1 = y0 + charDY;
            for (size_t col = 0; col < CONSOLE_COLS; col++) {
                float x0 = offsetX + charDX * col;
                float x1 = x0 + charDX;
                positions[i++] = x0; positions[i++] = y0;
                positions[i++] = x1; positions[i++] = y0;
                positions[i++] = x0; positions[i++] = y1;
                positions[i++] = x1; positions[i++] = y1;
            }
        }
        if (app->positionBuffer == 0) {
            glGenBuffers(1, &app->positionBuffer);
        }
        glBindBuffer(GL_ARRAY_BUFFER, app->positionBuffer);
        glBufferData(GL_ARRAY_BUFFER, positionSize * positionCount, positions, GL_STATIC_DRAW);
        free(positions);
    }

    // Create index buffer
    if (app->indexBuffer == 0) {
        size_t indexSize = sizeof(GLshort);
        size_t indexCount = CONSOLE_MAX_LINES * CONSOLE_COLS * 6;
        GLshort *indices = malloc(indexSize * indexCount);
        size_t i = 0;
        for (size_t line = 0; line < CONSOLE_MAX_LINES; line++) {
            for (size_t col = 0; col < CONSOLE_COLS; col++) {
                indices[i++] = 0 + col * 4 + line * (CONSOLE_COLS * 4);
                indices[i++] = 1 + col * 4 + line * (CONSOLE_COLS * 4);
                indices[i++] = 2 + col * 4 + line * (CONSOLE_COLS * 4);
                indices[i++] = 3 + col * 4 + line * (CONSOLE_COLS * 4);
                indices[i++] = 2 + col * 4 + line * (CONSOLE_COLS * 4);
                indices[i++] = 1 + col * 4 + line * (CONSOLE_COLS * 4);
            }
        }
        glGenBuffers(1, &app->indexBuffer);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, app->indexBuffer);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indexSize * indexCount, indices, GL_STATIC_DRAW);
        free(indices);
    }
}

static void onSurfaceDestroyed(GLFMDisplay *display) {
    TypingApp *app = glfmGetUserData(display);
    app->program = 0;
    app->vertexArray = 0;
    app->positionBuffer = 0;
    app->texCoordBuffer = 0;
    app->indexBuffer = 0;
    app->texture = 0;
}

static void onFrame(GLFMDisplay *display) {
    TypingApp *app = glfmGetUserData(display);
    double frameTime = glfmGetTime();

    // Animate hidden lines
    if (app->bottomSpacingActual != app->bottomSpacingRequested) {
        app->bottomSpacingActual += (app->bottomSpacingRequested > app->bottomSpacingActual) ? 1 : -1;
        app->cursorBlinkStartTime = frameTime;
    }

    // Create texCoord buffer
    size_t i = 0;
    for (size_t screenLine = 0; screenLine < CONSOLE_MAX_LINES; screenLine++) {
        for (size_t col = 0; col < CONSOLE_COLS; col++) {
            unsigned char ch = ' ';
            if (screenLine >= app->bottomSpacingActual) {
                size_t line = screenLine - app->bottomSpacingActual;
                if (line < app->consoleLineCount) {
                    if (line == 0 && col == app->consoleCol) {
                        const double cursorBlinkDuration = 0.5;
                        double blink = fmod(frameTime - app->cursorBlinkStartTime, cursorBlinkDuration * 2);
                        ch = (app->focused && blink <= cursorBlinkDuration) ? '_' : ' ';
                    } else {
                        ch = app->console[(app->consoleLineFirst + line) % CONSOLE_MAX_LINES][col];
                        if (ch < FONT_CHAR_FIRST || ch >= FONT_CHAR_FIRST + FONT_CHAR_COUNT) {
                            ch = ' ';
                        }
                    }
                }
            }
            size_t charIndex = ch - FONT_CHAR_FIRST;
            size_t charX = charIndex % TEXTURE_CHARS_X;
            size_t charY = charIndex / TEXTURE_CHARS_X;
            float spaceU = 1.0f / (TEXTURE_CHARS_X * (FONT_CHAR_WIDTH + TEXTURE_SPACING));
            float spaceV = 1.0f / (TEXTURE_CHARS_Y * (FONT_CHAR_HEIGHT + TEXTURE_SPACING));
            float u0 = (float)(charX + 0) / TEXTURE_CHARS_X;
            float v0 = (float)(charY + 0) / TEXTURE_CHARS_Y;
            float u1 = (float)(charX + 1) / TEXTURE_CHARS_X - spaceU;
            float v1 = (float)(charY + 1) / TEXTURE_CHARS_Y - spaceV;
            app->texCoords[i++] = u0; app->texCoords[i++] = v0;
            app->texCoords[i++] = u1; app->texCoords[i++] = v0;
            app->texCoords[i++] = u0; app->texCoords[i++] = v1;
            app->texCoords[i++] = u1; app->texCoords[i++] = v1;
        }
    }
    if (app->texCoordBuffer == 0) {
        glGenBuffers(1, &app->texCoordBuffer);
    }
    glBindBuffer(GL_ARRAY_BUFFER, app->texCoordBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(app->texCoords), app->texCoords, GL_DYNAMIC_DRAW);

    // Draw background
    int width, height;
    glfmGetDisplaySize(display, &width, &height);
    glViewport(0, 0, width, height);
    glClearColor(0.11f, 0.12f, 0.15f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Draw text
    glUseProgram(app->program);
    glEnable(GL_BLEND);
    glBlendFuncSeparate(GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    glBindBuffer(GL_ARRAY_BUFFER, app->positionBuffer);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 2, (void *)0);

    glBindBuffer(GL_ARRAY_BUFFER, app->texCoordBuffer);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 2, (void *)0);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, app->indexBuffer);
    glDrawElements(GL_TRIANGLES, CONSOLE_COLS * CONSOLE_MAX_LINES * 6, GL_UNSIGNED_SHORT, (void *)0);

    // Show
    glfmSwapBuffers(display);
}

void glfmMain(GLFMDisplay *display) {
    TypingApp *app = calloc(1, sizeof(TypingApp));

    glfmSetDisplayConfig(display,
                         GLFMRenderingAPIOpenGLES2,
                         GLFMColorFormatRGBA8888,
                         GLFMDepthFormatNone,
                         GLFMStencilFormatNone,
                         GLFMMultisampleNone);
    glfmSetUserData(display, app);
    glfmSetAppFocusFunc(display, onFocus);
    glfmSetSurfaceCreatedFunc(display, onSurfaceCreatedOrResized);
    glfmSetSurfaceResizedFunc(display, onSurfaceCreatedOrResized);
    glfmSetSurfaceDestroyedFunc(display, onSurfaceDestroyed);
    glfmSetRenderFunc(display, onFrame);
    glfmSetTouchFunc(display, onTouch);
    glfmSetKeyFunc(display, onKey);
    glfmSetCharFunc(display, onChar);
    glfmSetKeyboardVisibilityChangedFunc(display, onKeyboardVisibilityChanged);

    if (glfmHasVirtualKeyboard(display)) {
        consolePrint(app, "Tap to show keyboard\n");
    } else {
        consolePrint(app, "");
    }
}
