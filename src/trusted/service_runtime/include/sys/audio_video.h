/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


/*
 * NaCl low-level runtime library interfaces.
 *
 * Note: audio/video function declarations have moved to
 *     native_client/tools/libav
 *
 * Applications should #include <nacl/nacl_av.h> instead of this file.
 *
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_SERVICE_RUNTIME_AUDIO_VIDEO_H_
#define NATIVE_CLIENT_SRC_TRUSTED_SERVICE_RUNTIME_AUDIO_VIDEO_H_

#ifdef __native_client__
# include <stdint.h>
# include <machine/_default_types.h>
#else
# include "native_client/src/include/portability.h"
#endif

/**
 * @file
 * Defines a basic audio/video interface for Native Client applications.
 *
 * @addtogroup audio_video
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

#define kNaClAudioBufferLength (16 * 1024)
#define kNaClVideoMinWindowSize (32)
#define kNaClVideoMaxWindowSize (4096)

/**
 * @nacl
 * Subsystem enumerations for nacl_multimedia_init()
 * These values can be used as a bitset.
 * To initialize multiple subsystems, use the | operator to
 * combine them, for example:
 * NACL_SUBSYSTEM_VIDEO | NACL_SUBSYSTEM_AUDIO
 */
enum {
  NACL_SUBSYSTEM_VIDEO = 0x01,  /**< application will output video */
  NACL_SUBSYSTEM_AUDIO = 0x02,  /**< application will emit audio */
  NACL_SUBSYSTEM_EMBED = 0x04   /**< application will embed in browser */
};

/**
 * @nacl
 * Audio format enumerations for nacl_audio_init()
 * Describes the source data format that will be used to drive
 * audio output.
 */
enum NaClAudioFormat {
  NACL_AUDIO_FORMAT_STEREO_44K = 0,   /**< 44.1kHz 16 bit stereo */
  NACL_AUDIO_FORMAT_STEREO_48K       /**< 48kHz 16 bit stereo */
};

/**
 * @nacl
 * Event type enumerations used to identify event structure.
 */
enum NaClEvent {
  NACL_EVENT_NOT_USED = 0,
  NACL_EVENT_ACTIVE,             /**< triggered when focus gained / lost */
  NACL_EVENT_EXPOSE,             /**< external source requests redraw */
  NACL_EVENT_KEY_DOWN,           /**< key down (pressed) */
  NACL_EVENT_KEY_UP,             /**< key up (released) */
  NACL_EVENT_MOUSE_MOTION,       /**< mouse movement */
  NACL_EVENT_MOUSE_BUTTON_DOWN,  /**< mouse button down (pressed) */
  NACL_EVENT_MOUSE_BUTTON_UP,    /**< mouse button up (released) */
  NACL_EVENT_QUIT,               /**< application quit */
  NACL_EVENT_UNSUPPORTED
};

/**
 * @nacl
 * Key enumerations
 * Do not alter these mappings; they must match SDL.
 */
enum NaClKey {
  NACL_KEY_UNKNOWN      = 0,
  NACL_KEY_FIRST        = 0,
  NACL_KEY_BACKSPACE    = 8,
  NACL_KEY_TAB          = 9,
  NACL_KEY_CLEAR        = 12,
  NACL_KEY_RETURN       = 13,
  NACL_KEY_PAUSE        = 19,
  NACL_KEY_ESCAPE       = 27,
  NACL_KEY_SPACE        = 32,
  NACL_KEY_EXCLAIM      = 33,
  NACL_KEY_QUOTEDBL     = 34,
  NACL_KEY_HASH         = 35,
  NACL_KEY_DOLLAR       = 36,
  NACL_KEY_AMPERSAND    = 38,
  NACL_KEY_QUOTE        = 39,
  NACL_KEY_LEFTPAREN    = 40,
  NACL_KEY_RIGHTPAREN   = 41,
  NACL_KEY_ASTERISK     = 42,
  NACL_KEY_PLUS         = 43,
  NACL_KEY_COMMA        = 44,
  NACL_KEY_MINUS        = 45,
  NACL_KEY_PERIOD       = 46,
  NACL_KEY_SLASH        = 47,
  NACL_KEY_0            = 48,
  NACL_KEY_1            = 49,
  NACL_KEY_2            = 50,
  NACL_KEY_3            = 51,
  NACL_KEY_4            = 52,
  NACL_KEY_5            = 53,
  NACL_KEY_6            = 54,
  NACL_KEY_7            = 55,
  NACL_KEY_8            = 56,
  NACL_KEY_9            = 57,
  NACL_KEY_COLON        = 58,
  NACL_KEY_SEMICOLON    = 59,
  NACL_KEY_LESS         = 60,
  NACL_KEY_EQUALS       = 61,
  NACL_KEY_GREATER      = 62,
  NACL_KEY_QUESTION     = 63,
  NACL_KEY_AT           = 64,
  NACL_KEY_LEFTBRACKET  = 91,
  NACL_KEY_BACKSLASH    = 92,
  NACL_KEY_RIGHTBRACKET = 93,
  NACL_KEY_CARET        = 94,
  NACL_KEY_UNDERSCORE   = 95,
  NACL_KEY_BACKQUOTE    = 96,
  NACL_KEY_a            = 97,
  NACL_KEY_b            = 98,
  NACL_KEY_c            = 99,
  NACL_KEY_d            = 100,
  NACL_KEY_e            = 101,
  NACL_KEY_f            = 102,
  NACL_KEY_g            = 103,
  NACL_KEY_h            = 104,
  NACL_KEY_i            = 105,
  NACL_KEY_j            = 106,
  NACL_KEY_k            = 107,
  NACL_KEY_l            = 108,
  NACL_KEY_m            = 109,
  NACL_KEY_n            = 110,
  NACL_KEY_o            = 111,
  NACL_KEY_p            = 112,
  NACL_KEY_q            = 113,
  NACL_KEY_r            = 114,
  NACL_KEY_s            = 115,
  NACL_KEY_t            = 116,
  NACL_KEY_u            = 117,
  NACL_KEY_v            = 118,
  NACL_KEY_w            = 119,
  NACL_KEY_x            = 120,
  NACL_KEY_y            = 121,
  NACL_KEY_z            = 122,
  NACL_KEY_DELETE       = 127,
  NACL_KEY_WORLD_0      = 160,
  NACL_KEY_WORLD_1      = 161,
  NACL_KEY_WORLD_2      = 162,
  NACL_KEY_WORLD_3      = 163,
  NACL_KEY_WORLD_4      = 164,
  NACL_KEY_WORLD_5      = 165,
  NACL_KEY_WORLD_6      = 166,
  NACL_KEY_WORLD_7      = 167,
  NACL_KEY_WORLD_8      = 168,
  NACL_KEY_WORLD_9      = 169,
  NACL_KEY_WORLD_10     = 170,
  NACL_KEY_WORLD_11     = 171,
  NACL_KEY_WORLD_12     = 172,
  NACL_KEY_WORLD_13     = 173,
  NACL_KEY_WORLD_14     = 174,
  NACL_KEY_WORLD_15     = 175,
  NACL_KEY_WORLD_16     = 176,
  NACL_KEY_WORLD_17     = 177,
  NACL_KEY_WORLD_18     = 178,
  NACL_KEY_WORLD_19     = 179,
  NACL_KEY_WORLD_20     = 180,
  NACL_KEY_WORLD_21     = 181,
  NACL_KEY_WORLD_22     = 182,
  NACL_KEY_WORLD_23     = 183,
  NACL_KEY_WORLD_24     = 184,
  NACL_KEY_WORLD_25     = 185,
  NACL_KEY_WORLD_26     = 186,
  NACL_KEY_WORLD_27     = 187,
  NACL_KEY_WORLD_28     = 188,
  NACL_KEY_WORLD_29     = 189,
  NACL_KEY_WORLD_30     = 190,
  NACL_KEY_WORLD_31     = 191,
  NACL_KEY_WORLD_32     = 192,
  NACL_KEY_WORLD_33     = 193,
  NACL_KEY_WORLD_34     = 194,
  NACL_KEY_WORLD_35     = 195,
  NACL_KEY_WORLD_36     = 196,
  NACL_KEY_WORLD_37     = 197,
  NACL_KEY_WORLD_38     = 198,
  NACL_KEY_WORLD_39     = 199,
  NACL_KEY_WORLD_40     = 200,
  NACL_KEY_WORLD_41     = 201,
  NACL_KEY_WORLD_42     = 202,
  NACL_KEY_WORLD_43     = 203,
  NACL_KEY_WORLD_44     = 204,
  NACL_KEY_WORLD_45     = 205,
  NACL_KEY_WORLD_46     = 206,
  NACL_KEY_WORLD_47     = 207,
  NACL_KEY_WORLD_48     = 208,
  NACL_KEY_WORLD_49     = 209,
  NACL_KEY_WORLD_50     = 210,
  NACL_KEY_WORLD_51     = 211,
  NACL_KEY_WORLD_52     = 212,
  NACL_KEY_WORLD_53     = 213,
  NACL_KEY_WORLD_54     = 214,
  NACL_KEY_WORLD_55     = 215,
  NACL_KEY_WORLD_56     = 216,
  NACL_KEY_WORLD_57     = 217,
  NACL_KEY_WORLD_58     = 218,
  NACL_KEY_WORLD_59     = 219,
  NACL_KEY_WORLD_60     = 220,
  NACL_KEY_WORLD_61     = 221,
  NACL_KEY_WORLD_62     = 222,
  NACL_KEY_WORLD_63     = 223,
  NACL_KEY_WORLD_64     = 224,
  NACL_KEY_WORLD_65     = 225,
  NACL_KEY_WORLD_66     = 226,
  NACL_KEY_WORLD_67     = 227,
  NACL_KEY_WORLD_68     = 228,
  NACL_KEY_WORLD_69     = 229,
  NACL_KEY_WORLD_70     = 230,
  NACL_KEY_WORLD_71     = 231,
  NACL_KEY_WORLD_72     = 232,
  NACL_KEY_WORLD_73     = 233,
  NACL_KEY_WORLD_74     = 234,
  NACL_KEY_WORLD_75     = 235,
  NACL_KEY_WORLD_76     = 236,
  NACL_KEY_WORLD_77     = 237,
  NACL_KEY_WORLD_78     = 238,
  NACL_KEY_WORLD_79     = 239,
  NACL_KEY_WORLD_80     = 240,
  NACL_KEY_WORLD_81     = 241,
  NACL_KEY_WORLD_82     = 242,
  NACL_KEY_WORLD_83     = 243,
  NACL_KEY_WORLD_84     = 244,
  NACL_KEY_WORLD_85     = 245,
  NACL_KEY_WORLD_86     = 246,
  NACL_KEY_WORLD_87     = 247,
  NACL_KEY_WORLD_88     = 248,
  NACL_KEY_WORLD_89     = 249,
  NACL_KEY_WORLD_90     = 250,
  NACL_KEY_WORLD_91     = 251,
  NACL_KEY_WORLD_92     = 252,
  NACL_KEY_WORLD_93     = 253,
  NACL_KEY_WORLD_94     = 254,
  NACL_KEY_WORLD_95     = 255,

  /* numeric keypad */
  NACL_KEY_KP0          = 256,
  NACL_KEY_KP1          = 257,
  NACL_KEY_KP2          = 258,
  NACL_KEY_KP3          = 259,
  NACL_KEY_KP4          = 260,
  NACL_KEY_KP5          = 261,
  NACL_KEY_KP6          = 262,
  NACL_KEY_KP7          = 263,
  NACL_KEY_KP8          = 264,
  NACL_KEY_KP9          = 265,
  NACL_KEY_KP_PERIOD    = 266,
  NACL_KEY_KP_DIVIDE    = 267,
  NACL_KEY_KP_MULTIPLY  = 268,
  NACL_KEY_KP_MINUS     = 269,
  NACL_KEY_KP_PLUS      = 270,
  NACL_KEY_KP_ENTER     = 271,
  NACL_KEY_KP_EQUALS    = 272,

  /* arrow & insert/delete pad */
  NACL_KEY_UP           = 273,
  NACL_KEY_DOWN         = 274,
  NACL_KEY_RIGHT        = 275,
  NACL_KEY_LEFT         = 276,
  NACL_KEY_INSERT       = 277,
  NACL_KEY_HOME         = 278,
  NACL_KEY_END          = 279,
  NACL_KEY_PAGEUP       = 280,
  NACL_KEY_PAGEDOWN     = 281,

  /* function keys */
  NACL_KEY_F1           = 282,
  NACL_KEY_F2           = 283,
  NACL_KEY_F3           = 284,
  NACL_KEY_F4           = 285,
  NACL_KEY_F5           = 286,
  NACL_KEY_F6           = 287,
  NACL_KEY_F7           = 288,
  NACL_KEY_F8           = 289,
  NACL_KEY_F9           = 290,
  NACL_KEY_F10          = 291,
  NACL_KEY_F11          = 292,
  NACL_KEY_F12          = 293,
  NACL_KEY_F13          = 294,
  NACL_KEY_F14          = 295,
  NACL_KEY_F15          = 296,

  /* modifier keys */
  NACL_KEY_NUMLOCK      = 300,
  NACL_KEY_CAPSLOCK     = 301,
  NACL_KEY_SCROLLOCK    = 302,
  NACL_KEY_RSHIFT       = 303,
  NACL_KEY_LSHIFT       = 304,
  NACL_KEY_RCTRL        = 305,
  NACL_KEY_LCTRL        = 306,
  NACL_KEY_RALT         = 307,
  NACL_KEY_LALT         = 308,
  NACL_KEY_RMETA        = 309,
  NACL_KEY_LMETA        = 310,
  NACL_KEY_LSUPER       = 311,
  NACL_KEY_RSUPER       = 312,
  NACL_KEY_MODE         = 313,
  NACL_KEY_COMPOSE      = 314,

  /* misc keys */
  NACL_KEY_HELP         = 315,
  NACL_KEY_PRINT        = 316,
  NACL_KEY_SYSREQ       = 317,
  NACL_KEY_BREAK        = 318,
  NACL_KEY_MENU         = 319,
  NACL_KEY_POWER        = 320,
  NACL_KEY_EURO         = 321,
  NACL_KEY_UNDO         = 322,

  /* Add any other keys here */
  NACL_KEY_LAST
};

/**
 * @nacl
 * Key modifier enumerations
 * These values can be used as a bitset.
 * Do not alter these mappings; they must match SDL.
 */
enum NaClKeyMod {
  /* mods represented as bitset */
  NACL_KEYMOD_NONE  = 0x0000,
  NACL_KEYMOD_LSHIFT= 0x0001,
  NACL_KEYMOD_RSHIFT= 0x0002,
  NACL_KEYMOD_LCTRL = 0x0040,
  NACL_KEYMOD_RCTRL = 0x0080,
  NACL_KEYMOD_LALT  = 0x0100,
  NACL_KEYMOD_RALT  = 0x0200,
  NACL_KEYMOD_LMETA = 0x0400,
  NACL_KEYMOD_RMETA = 0x0800,
  NACL_KEYMOD_NUM   = 0x1000,
  NACL_KEYMOD_CAPS  = 0x2000,
  NACL_KEYMOD_MODE  = 0x4000,
  NACL_KEYMOD_RESERVED = 0x8000
};

/**
 * @nacl
 * Mouse Buttons.  Do not alter these mappings; they must match SDL.
 */
enum NaClMouseButton {
  NACL_MOUSE_BUTTON_LEFT = 1,
  NACL_MOUSE_BUTTON_MIDDLE = 2,
  NACL_MOUSE_BUTTON_RIGHT = 3,
  NACL_MOUSE_SCROLL_UP = 4,
  NACL_MOUSE_SCROLL_DOWN = 5
};

/**
 * @nacl
 * Mouse states
 * These values can be used as bitsets.  Do not alter these mappings; they
 * must match SDL.
 */
enum NaClMouseState {
  NACL_MOUSE_STATE_LEFT_BUTTON_PRESSED = 1,
  NACL_MOUSE_STATE_MIDDLE_BUTTON_PRESSED = 2,
  NACL_MOUSE_STATE_RIGHT_BUTTON_PRESSED = 4
};

/**
 * @nacl
 * Gain enumerations for NaClMultimediaActiveEvent
 * These values can be used as a bitset.  Do not alter these mappings; they
 * must match SDL.
 */
enum NaClActive {
  NACL_ACTIVE_MOUSE = 1,        /**< mouse leaving/entering */
  NACL_ACTIVE_INPUT_FOCUS = 2,  /**< input focus lost/restored */
  NACL_ACTIVE_APPLICATION = 4   /**< application minimized/restored */
};

/**
 * @nacl
 * Key symbol structure
 */
struct NaClMultimediaKeySymbol {
  uint8_t scancode;     /**< H/W scancode */
  int16_t sym;          /**< NaClKey symbol */
  int16_t mod;          /**< NaClKeyMod key modifier */
  uint16_t unicode;
};

/**
 * @nacl
 * Active event.  Generated when mouse cursor exits/enters window,
 * input focus is lost/gained, and application is minimized/restored.
 */
struct NaClMultimediaActiveEvent {
  uint8_t type;        /**< NACL_EVENT_ACTIVE */
  uint8_t gain;        /**< NaClActive gain */
  uint8_t state;       /**< state 0 - lost, minimized, leaving
                                  1 - acquired, restored, entering */
};

/**
 * @nacl
 * Expose event.  Generated when the windowing system requests
 * an application redraw/repaint update.  Upon receiving this
 * event, an application should call nacl_video_update()
 */
struct NaClMultimediaExposeEvent {
  uint8_t type;        /**< NACL_EVENT_EXPOSE */
};

/**
 * @nacl
 * Keyboard event.  For both key down and key up events.
 */
struct NaClMultimediaKeyboardEvent {
  uint8_t type;        /**< NACL_EVENT_KEY_UP or NACL_EVENT_KEY_DOWN */
  uint8_t which;       /**< which keyboard (0 for primary) */
  uint8_t state;       /**< indicates pressed (1) or released (0) */
  struct NaClMultimediaKeySymbol keysym;  /**< key info */
};

/**
 * @nacl
 * Mouse motion event.  Generated when mouse moved by user and
 * application is in focus.
 */
struct NaClMultimediaMouseMotionEvent {
  uint8_t type;        /**< NACL_EVENT_MOUSE_MOTION */
  uint8_t which;       /**< which mouse (0 for primary) */
  uint8_t state;       /**< NaClMouseState button state bitset */
  uint16_t x;          /**< x position */
  uint16_t y;          /**< y position */
  int16_t xrel;        /**< relative x position */
  int16_t yrel;        /**< relative y position */
};

/**
 * @nacl
 * Mouse button event.  Generated when user presses or releases
 * a mouse button, including the scroll wheel.
 */
struct NaClMultimediaMouseButtonEvent {
  uint8_t type;        /**< NACL_EVENT_MOUSE_BUTTON_UP or
                            NACL_EVENT_MOUSE_BUTTON_DOWN */
  uint8_t which;       /**< which mouse (0 for primary) */
  uint8_t button;      /**< NaClMouseButton which button */
  uint8_t state;       /**< indicates pressed (1) or released (0) */
  uint16_t x;          /**< x position */
  uint16_t y;          /**< y position */
};

/**
 * @nacl
 * Quit event.  The application should quit when receiving this event.
 * This includes shutting down the video subsystem with nacl_video_shutdown()
 * and other active subsystems (audio) followed by shutting down the
 * multimedia library with nacl_multimedia_shutdown().
 */
struct NaClMultimediaQuitEvent {
  uint8_t type;        /**< NACL_EVENT_QUIT */
};

/**
 * @nacl
 * Pad event.  Not used, but defines the largest size the event union
 * should ever be.  Events added in the future should never be larger than
 * this structure.  64 byte events should be enough for all sane events.
 *
 *    *** Do not ever change the size of this structure. ***
 *        Doing so will break other existing native client applications
 */
struct NaClMultimediaPadEvent {
  uint8_t pad[64];
};

/**
 * @nacl
 * The event wrapper.  Use type field to select which structure to process.
 * At all times, sizeof NaClMultimediaEvent must match sizeof
 * NaClMultimediaPadEvent.
 */
union NaClMultimediaEvent {
  uint8_t type;        /**< one of NaClEvent */
  struct NaClMultimediaActiveEvent active;
  struct NaClMultimediaExposeEvent expose;
  struct NaClMultimediaKeyboardEvent key;
  struct NaClMultimediaMouseMotionEvent motion;
  struct NaClMultimediaMouseButtonEvent button;
  struct NaClMultimediaQuitEvent quit;
  struct NaClMultimediaPadEvent pad;
};


#ifdef __cplusplus
}
#endif

/**
 * @}
 * End of Audio/Video group
 */

#endif  /* NATIVE_CLIENT_SRC_TRUSTED_SERVICE_RUNTIME_AUDIO_VIDEO_H_ */
