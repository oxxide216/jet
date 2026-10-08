#ifndef CONFIG_H
#define CONFIG_H

#define SPACES_PER_TAB 2

#define NEW_FILE_NAME    U"<New file>"
#define LINE_WRAP_MARKER U"→"

#define MAX_FONT_SCALE     240.0
#define MIN_FONT_SCALE     8.0
#define DEFAULT_FONT_SCALE 24.0

#define BUFFER_PADDING        10.0
#define PALETTE_WIDTH_FACTOR  0.8
#define PALETTE_HEIGHT_FACTOR 0.6
#define PALETTE_BORDER_WIDTH  2.5
#define PALETTE_BORDER_RADIUS 15.0
#define PALETTE_ALPHA         1.0
// If less than PALETTE_BORDER_RADIUS, gives artifacts
#define PALETTE_SHADOW_OFFSET 15.0

#define SHADOW_COLOR 0.0, 0.0, 0.0, 0.5

#define BG_COLOR     0x18/255.0, 0x18/255.0, 0x18/255.0
#define FG_COLOR     0x86/255.0, 0x8a/255.0, 0xac/255.0
#define ACC_COLOR    0x82/255.0, 0x53/255.0, 0xa1/255.0
#define ALT_BG_COLOR 0x6a/255.0, 0x7c/255.0, 0x93/255.0
#define ALT_FG_COLOR 0x18/255.0, 0x18/255.0, 0x18/255.0
#define ERROR_COLOR  0xb5/255.0, 0x40/255.0, 0x36/255.0
#define WARN_COLOR   0xde/255.0, 0xb5/255.0, 0x66/255.0
#define INFO_COLOR   0x5a/255.0, 0xb9/255.0, 0x77/255.0

#endif // CONFIG_H
