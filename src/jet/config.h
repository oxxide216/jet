#ifndef CONFIG_H
#define CONFIG_H

/* BEHAVIOUR */

#define SPACES_PER_TAB 2

#define AUTOCOMPLETION                   true
#define MINIMAL_COMPLETION_PREFIX_LENGTH 2

/* APPEARANCE */

#define NEW_FILE_NAME    U"<New file>"
#define LINE_WRAP_MARKER U"→"

// General
#define MAX_FONT_SCALE     240
#define MIN_FONT_SCALE     8
#define DEFAULT_FONT_SCALE 24
#define SHADOW_COLOR       0.0, 0.0, 0.0, 0.5

// Main buffer
#define BUFFER_PADDING        10.0

// Palette
#define PALETTE_WIDTH_FACTOR  0.8
#define PALETTE_HEIGHT_FACTOR 0.6
#define PALETTE_BORDER_WIDTH  3.0
#define PALETTE_BORDER_RADIUS 15.0
#define PALETTE_ALPHA         1.0
// If less than PALETTE_BORDER_RADIUS, gives artifacts
#define PALETTE_SHADOW_OFFSET 15.0

// Inline errors
#define INLINE_ERROR_OFFSET_MULTIPLIER 2.0

// Completions window
#define MAX_COMPLETIONS_WINDOW_WIDTH    (250.0 + BUFFER_PADDING)
#define COMPLETIONS_WINDOW_CAPACITY     7
#define COMPLETIONS_WINDOW_BORDER_WIDTH 2.0

// Colors
#define BG_COLOR     0x18/255.0, 0x18/255.0, 0x18/255.0
#define FG_COLOR     0x86/255.0, 0x8a/255.0, 0xac/255.0
#define ACC_COLOR    0x82/255.0, 0x53/255.0, 0xa1/255.0
#define ALT_BG_COLOR 0x6a/255.0, 0x7c/255.0, 0x93/255.0
#define ALT_FG_COLOR 0x18/255.0, 0x18/255.0, 0x18/255.0
#define ERROR_COLOR  0xb5/255.0, 0x40/255.0, 0x36/255.0
#define WARN_COLOR   0xde/255.0, 0xb5/255.0, 0x66/255.0
#define INFO_COLOR   0x5a/255.0, 0xb9/255.0, 0x77/255.0

#endif // CONFIG_H
