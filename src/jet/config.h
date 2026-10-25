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
#define MIN_FONT_SCALE     24
#define DEFAULT_FONT_SCALE 28
#define FONT_RENDER_SCALE  48
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
#define MAX_COMPLETIONS_WINDOW_WIDTH    (500.0 + BUFFER_PADDING)
#define COMPLETIONS_WINDOW_CAPACITY     7
#define COMPLETIONS_WINDOW_BORDER_WIDTH 2.0

// Colors
#define BG_COLOR      0x18/255.0, 0x18/255.0, 0x18/255.0
#define FG_COLOR      0x86/255.0, 0x8a/255.0, 0xac/255.0
#define ACC_COLOR     0x82/255.0, 0x53/255.0, 0xa1/255.0
#define ALT_BG_COLOR  0x6a/255.0, 0x7c/255.0, 0x93/255.0
#define ALT_FG_COLOR  0x18/255.0, 0x18/255.0, 0x18/255.0
#define BLACK_COLOR   0x23/255.0, 0x25/255.0, 0x2b/255.0
#define RED_COLOR     0xb5/255.0, 0x40/255.0, 0x36/255.0
#define GREEN_COLOR   0x5a/255.0, 0xb9/255.0, 0x77/255.0
#define YELLOW_COLOR  0xde/255.0, 0xb5/255.0, 0x66/255.0
#define GREY_COLOR    0x6a/255.0, 0x7c/255.0, 0x93/255.0
#define MAGENTA_COLOR 0x82/255.0, 0x53/255.0, 0xa1/255.0
#define CYAN_COLOR    0x3f/255.0, 0x94/255.0, 0xa8/255.0
#define WHITE_COLOR   0xeb/255.0, 0xed/255.0, 0xf2/255.0
// Specific theme
#define ERROR_COLOR   RED_COLOR
#define WARN_COLOR    YELLOW_COLOR
#define INFO_COLOR    GREEN_COLOR
#define KW_COLOR      MAGENTA_COLOR
#define ALT_KW_COLOR  CYAN_COLOR
#define TYPE_COLOR    YELLOW_COLOR
#define STRING_COLOR  GREEN_COLOR
#define COMMENT_COLOR GREY_COLOR

#endif // CONFIG_H
