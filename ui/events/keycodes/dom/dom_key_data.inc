// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has no header guard because it is explicily intended
// to be included more than once with different definitions of the
// macros DOM_KEY_MAP and DOM_KEY_MAP_DECLARATION.

// This is a table of DOM Level 3 .key values, used to generate identifiers
// (enums) and string tables. These names are defined by:
//
//   [0] DOM Level 3 KeyboardEvent key Values,
//       http://www.w3.org/TR/DOM-Level-3-Events-key/
//
// That reference should be consulted on the meaning and usage of these
// values; this file does not attempt to describe that.
//
// The names given in the table here directly match the DOM Level 3 value,
// and for the most part the corresponding enum identifier is generated from
// the name by converting W3C's CamelCaseStyle to Chromium's C_MACRO_STYLE.

DOM_KEY_MAP_DECLARATION {

  // To avoid ambiguities, a small number of .key values that do have
  // standard names are encoded identical to corresponding Unicode characters;
  // for example, DomKey('\t') == DomKey::TAB.
  //
  //          Key                     Enum        Unicode code point
  DOM_KEY_UNI("Backspace",            BACKSPACE,  0x0008),
  DOM_KEY_UNI("Tab",                  TAB,        0x0009),
  DOM_KEY_UNI("Enter",                ENTER,      0x000D),
  DOM_KEY_UNI("Escape",               ESCAPE,     0x001B),
  // The enum constant for 'Delete' (Forward Delete) is named DEL because
  // DELETE conflicts with an unscoped declaration in Windows' <windows.h>.
  DOM_KEY_UNI("Delete",               DEL,        0x007F),

  DOM_KEY_MAP_BEGIN

  // =========================================================
  // Special Key Values
  // http://www.w3.org/TR/DOM-Level-3-Events-key/#keys-special
  // =========================================================

  // Unable to identify another key value
  DOM_KEY_MAP("Unidentified",         UNIDENTIFIED),

  // ==========================================================
  // Modifier Keys
  // http://www.w3.org/TR/DOM-Level-3-Events-key/#keys-modifier
  // ==========================================================

  //          Key                     Enum
  DOM_KEY_MAP("Accel",                ACCEL),  // Never returned in a key event
  DOM_KEY_MAP("Alt",                  ALT),
  DOM_KEY_MAP("AltGraph",             ALT_GRAPH),
  DOM_KEY_MAP("CapsLock",             CAPS_LOCK),
  DOM_KEY_MAP("Control",              CONTROL),
  DOM_KEY_MAP("Fn",                   FN),
  DOM_KEY_MAP("FnLock",               FN_LOCK),
  DOM_KEY_MAP("Hyper",                HYPER),
  DOM_KEY_MAP("Meta",                 META),
  DOM_KEY_MAP("NumLock",              NUM_LOCK),
  DOM_KEY_MAP("OS",                   OS),
  DOM_KEY_MAP("ScrollLock",           SCROLL_LOCK),
  DOM_KEY_MAP("Shift",                SHIFT),
  DOM_KEY_MAP("Super",                SUPER),
  DOM_KEY_MAP("Symbol",               SYMBOL),
  DOM_KEY_MAP("SymbolLock",           SYMBOL_LOCK),

  // Non-standard value corresponding to XKB keysym ISO_Level5_Shift,
  // an additional printable-character modifier like Shift and AltGraph,
  // used by such layouts as Neo (German) and Multix (French-Canadian).
  //          Key                     Enum
  DOM_KEY_MAP("ShiftLevel5",          SHIFT_LEVEL5),

  // Non-standard value representing a latching-modifier key in ui::Keyevent.
  // This is present on some keyboard layouts (Latvian, Tibetan, Cameroon),
  // and applies the modifier to the next (non-modifier) key.
  // These events are consumed internally and not exposed to the web,
  // either directly or as synthetic modifier up/down events.
  //          Key                     Enum
  DOM_KEY_MAP(".AltGraphLatch",       ALT_GRAPH_LATCH),

  // ============================================================
  // Whitespace Keys
  // http://www.w3.org/TR/DOM-Level-3-Events-key/#keys-whitespace
  // ============================================================

  //          Key                     Enum
  //          "Enter" is encoded in the Unicode space (at the end of this list).
  DOM_KEY_MAP("Separator",            SEPARATOR),
  //          "Tab" is encoded in the Unicode space.

  // ============================================================
  // Navigation Keys
  // http://www.w3.org/TR/DOM-Level-3-Events-key/#keys-navigation
  // ============================================================

  //          Key                     Enum
  DOM_KEY_MAP("ArrowDown",            ARROW_DOWN),
  DOM_KEY_MAP("ArrowLeft",            ARROW_LEFT),
  DOM_KEY_MAP("ArrowRight",           ARROW_RIGHT),
  DOM_KEY_MAP("ArrowUp",              ARROW_UP),
  DOM_KEY_MAP("End",                  END),
  DOM_KEY_MAP("Home",                 HOME),
  DOM_KEY_MAP("PageDown",             PAGE_DOWN),
  DOM_KEY_MAP("PageUp",               PAGE_UP),

  // =========================================================
  // Editing Keys
  // http://www.w3.org/TR/DOM-Level-3-Events-key/#keys-editing
  // =========================================================

  //          Key                     Enum
  //          "Backspace" is encoded in the Unicode space.
  DOM_KEY_MAP("Clear",                CLEAR),
  DOM_KEY_MAP("Copy",                 COPY),
  DOM_KEY_MAP("CrSel",                CR_SEL),     // Cursor Select
  DOM_KEY_MAP("Cut",                  CUT),
  //          "Delete" is encoded in the Unicode space.
  DOM_KEY_MAP("EraseEof",             ERASE_EOF),  // Erase to End of Field
  DOM_KEY_MAP("ExSel",                EX_SEL),     // Extend Selection
  DOM_KEY_MAP("Insert",               INSERT),
  DOM_KEY_MAP("Paste",                PASTE),
  DOM_KEY_MAP("Redo",                 REDO),
  DOM_KEY_MAP("Undo",                 UNDO),

  // ====================================================
  // UI Keys
  // http://www.w3.org/TR/DOM-Level-3-Events-key/#keys-ui
  // ====================================================

  //          Key                     Enum
  DOM_KEY_MAP("Accept",               ACCEPT),  // Accept current IME conversion
  DOM_KEY_MAP("Again",                AGAIN),   // Repeat an action
  DOM_KEY_MAP("Attn",                 ATTN),    // Attention
  DOM_KEY_MAP("Cancel",               CANCEL),
  DOM_KEY_MAP("ContextMenu",          CONTEXT_MENU),
  //          "Escape" is encoded in the Unicode space.
  DOM_KEY_MAP("Execute",              EXECUTE),
  DOM_KEY_MAP("Find",                 FIND),
  DOM_KEY_MAP("Help",                 HELP),
  DOM_KEY_MAP("Pause",                PAUSE),  // Program state - not for media
  DOM_KEY_MAP("Play",                 PLAY),   // Program state - not for media
  DOM_KEY_MAP("Props",                PROPS),  // Properties
  DOM_KEY_MAP("Select",               SELECT),
  DOM_KEY_MAP("ZoomIn",               ZOOM_IN),
  DOM_KEY_MAP("ZoomOut",              ZOOM_OUT),

  // ========================================================
  // Device Keys
  // http://www.w3.org/TR/DOM-Level-3-Events-key/#keys-device
  // ========================================================

  //          Key                     Enum
  DOM_KEY_MAP("BrightnessDown",       BRIGHTNESS_DOWN),
  DOM_KEY_MAP("BrightnessUp",         BRIGHTNESS_UP),
  DOM_KEY_MAP("Camera",               CAMERA),
  DOM_KEY_MAP("Eject",                EJECT),
  DOM_KEY_MAP("LogOff",               LOG_OFF),
  DOM_KEY_MAP("Power",                POWER),
  DOM_KEY_MAP("PowerOff",             POWER_OFF),
  DOM_KEY_MAP("PrintScreen",          PRINT_SCREEN),
  DOM_KEY_MAP("Hibernate",            HIBERNATE),
  DOM_KEY_MAP("Standby",              STANDBY),
  DOM_KEY_MAP("WakeUp",               WAKE_UP),

  // =============================================================
  // IME and Composition Keys
  // http://www.w3.org/TR/DOM-Level-3-Events-key/#keys-composition
  // =============================================================

  //          Key                     Enum
  DOM_KEY_MAP("AllCandidates",        ALL_CANDIDATES),
  DOM_KEY_MAP("Alphanumeric",         ALPHANUMERIC),
  DOM_KEY_MAP("CodeInput",            CODE_INPUT),
  DOM_KEY_MAP("Compose",              COMPOSE),
  DOM_KEY_MAP("Convert",              CONVERT),
  //          "Dead" is represented as a flag plus combining character.
  DOM_KEY_MAP("FinalMode",            FINAL_MODE),
  DOM_KEY_MAP("GroupFirst",           GROUP_FIRST),
  DOM_KEY_MAP("GroupLast",            GROUP_LAST),
  DOM_KEY_MAP("GroupNext",            GROUP_NEXT),
  DOM_KEY_MAP("GroupPrevious",        GROUP_PREVIOUS),
  DOM_KEY_MAP("ModeChange",           MODE_CHANGE),
  DOM_KEY_MAP("NextCandidate",        NEXT_CANDIDATE),
  DOM_KEY_MAP("NonConvert",           NON_CONVERT),
  DOM_KEY_MAP("PreviousCandidate",    PREVIOUS_CANDIDATE),
  DOM_KEY_MAP("Process",              PROCESS),
  DOM_KEY_MAP("SingleCandidate",      SINGLE_CANDIDATE),

  // Keys specific to Korean keyboards
  DOM_KEY_MAP("HangulMode",           HANGUL_MODE),
  DOM_KEY_MAP("HanjaMode",            HANJA_MODE),
  DOM_KEY_MAP("JunjaMode",            JUNJA_MODE),

  // Keys specific to Japanese keyboards
  DOM_KEY_MAP("Eisu",                 EISU),
  DOM_KEY_MAP("Hankaku",              HANKAKU),
  DOM_KEY_MAP("Hiragana",             HIRAGANA),
  DOM_KEY_MAP("HiraganaKatakana",     HIRAGANA_KATAKANA),
  DOM_KEY_MAP("KanaMode",             KANA_MODE),
  DOM_KEY_MAP("KanjiMode",            KANJI_MODE),
  DOM_KEY_MAP("Katakana",             KATAKANA),
  DOM_KEY_MAP("Romaji",               ROMAJI),
  DOM_KEY_MAP("Zenkaku",              ZENKAKU),
  DOM_KEY_MAP("ZenkakuHankaku",       ZENKAKU_HANKAKU),

  // ==========================================================
  // General-Purpose Function Keys
  // http://www.w3.org/TR/DOM-Level-3-Events-key/#keys-function
  // ==========================================================

  // DOM defines open-ended sets, but if we want a finite numeric encoding we
  // need to draw the line somewhere. USB and Windows (VK) and Linux (evdev)
  // all support 24, so we define 24.

  //          Key                   Enum
  DOM_KEY_MAP("F1",                   F1),
  DOM_KEY_MAP("F2",                   F2),
  DOM_KEY_MAP("F3",                   F3),
  DOM_KEY_MAP("F4",                   F4),
  DOM_KEY_MAP("F5",                   F5),
  DOM_KEY_MAP("F6",                   F6),
  DOM_KEY_MAP("F7",                   F7),
  DOM_KEY_MAP("F8",                   F8),
  DOM_KEY_MAP("F9",                   F9),
  DOM_KEY_MAP("F10",                  F10),
  DOM_KEY_MAP("F11",                  F11),
  DOM_KEY_MAP("F12",                  F12),
  DOM_KEY_MAP("F13",                  F13),
  DOM_KEY_MAP("F14",                  F14),
  DOM_KEY_MAP("F15",                  F15),
  DOM_KEY_MAP("F16",                  F16),
  DOM_KEY_MAP("F17",                  F17),
  DOM_KEY_MAP("F18",                  F18),
  DOM_KEY_MAP("F19",                  F19),
  DOM_KEY_MAP("F20",                  F20),
  DOM_KEY_MAP("F21",                  F21),
  DOM_KEY_MAP("F22",                  F22),
  DOM_KEY_MAP("F23",                  F23),
  DOM_KEY_MAP("F24",                  F24),
  DOM_KEY_MAP("Soft1",                SOFT1),
  DOM_KEY_MAP("Soft2",                SOFT2),
  DOM_KEY_MAP("Soft3",                SOFT3),
  DOM_KEY_MAP("Soft4",                SOFT4),
  DOM_KEY_MAP("Soft5",                SOFT5),
  DOM_KEY_MAP("Soft6",                SOFT6),
  DOM_KEY_MAP("Soft7",                SOFT7),
  DOM_KEY_MAP("Soft8",                SOFT8),

  // ============================================================
  // Multimedia Keys
  // http://www.w3.org/TR/DOM-Level-3-Events-key/#keys-multimedia
  // ============================================================

  //          Key                     Enum
  DOM_KEY_MAP("Close",                CLOSE),
  DOM_KEY_MAP("MailForward",          MAIL_FORWARD),
  DOM_KEY_MAP("MailReply",            MAIL_REPLY),
  DOM_KEY_MAP("MailSend",             MAIL_SEND),
  DOM_KEY_MAP("MediaPlayPause",       MEDIA_PLAY_PAUSE),
  DOM_KEY_MAP("MediaSelect",          MEDIA_SELECT),
  DOM_KEY_MAP("MediaStop",            MEDIA_STOP),
  DOM_KEY_MAP("MediaTrackNext",       MEDIA_TRACK_NEXT),
  DOM_KEY_MAP("MediaTrackPrevious",   MEDIA_TRACK_PREVIOUS),
  DOM_KEY_MAP("New",                  NEW),
  DOM_KEY_MAP("Open",                 OPEN),
  DOM_KEY_MAP("Print",                PRINT),
  DOM_KEY_MAP("Save",                 SAVE),
  DOM_KEY_MAP("SpellCheck",           SPELL_CHECK),
  DOM_KEY_MAP("VolumeDown",           VOLUME_DOWN),
  DOM_KEY_MAP("VolumeUp",             VOLUME_UP),
  DOM_KEY_MAP("VolumeMute",           VOLUME_MUTE),

  // ======================================================
  // Application Keys
  // http://www.w3.org/TR/DOM-Level-3-Events-key/#keys-apps
  // ======================================================

  //          Key                     Enum
  DOM_KEY_MAP("LaunchCalculator",     LAUNCH_CALCULATOR),  // Application 2
  DOM_KEY_MAP("LaunchCalendar",       LAUNCH_CALENDAR),
  DOM_KEY_MAP("LaunchMail",           LAUNCH_MAIL),
  DOM_KEY_MAP("LaunchMediaPlayer",    LAUNCH_MEDIA_PLAYER),
  DOM_KEY_MAP("LaunchMusicPlayer",    LAUNCH_MUSIC_PLAYER),
  DOM_KEY_MAP("LaunchMyComputer",     LAUNCH_MY_COMPUTER),  // Application 1
  DOM_KEY_MAP("LaunchScreenSaver",    LAUNCH_SCREEN_SAVER),
  DOM_KEY_MAP("LaunchSpreadsheet",    LAUNCH_SPREADSHEET),
  DOM_KEY_MAP("LaunchWebBrowser",     LAUNCH_WEB_BROWSER),
  DOM_KEY_MAP("LaunchWebCam",         LAUNCH_WEB_CAM),
  DOM_KEY_MAP("LaunchWordProcessor",  LAUNCH_WORD_PROCESSOR),

  // =========================================================
  // Browser Keys
  // http://www.w3.org/TR/DOM-Level-3-Events-key/#keys-browser
  // =========================================================

  //          Key                   Enum
  DOM_KEY_MAP("BrowserBack",          BROWSER_BACK),
  DOM_KEY_MAP("BrowserFavorites",     BROWSER_FAVORITES),
  DOM_KEY_MAP("BrowserForward",       BROWSER_FORWARD),
  DOM_KEY_MAP("BrowserHome",          BROWSER_HOME),
  DOM_KEY_MAP("BrowserRefresh",       BROWSER_REFRESH),
  DOM_KEY_MAP("BrowserSearch",        BROWSER_SEARCH),
  DOM_KEY_MAP("BrowserStop",          BROWSER_STOP),

  // ==================================================================
  // Media Controller Keys
  // http://www.w3.org/TR/DOM-Level-3-Events-key/#keys-media-controller
  // ==================================================================

  //          Key                   Enum
  DOM_KEY_MAP("AudioBalanceLeft",     AUDIO_BALANCE_LEFT),
  DOM_KEY_MAP("AudioBalanceRight",    AUDIO_BALANCE_RIGHT),
  DOM_KEY_MAP("AudioBassBoostDown",   AUDIO_BASS_BOOST_DOWN),
  DOM_KEY_MAP("AudioBassBoostUp",     AUDIO_BASS_BOOST_UP),
  DOM_KEY_MAP("AudioFaderFront",      AUDIO_FADER_FRONT),
  DOM_KEY_MAP("AudioFaderRear",       AUDIO_FADER_REAR),
  DOM_KEY_MAP("AudioSurroundModeNext",AUDIO_SURROUND_MODE_NEXT),
  DOM_KEY_MAP("AVRInput",             AVR_INPUT),
  DOM_KEY_MAP("AVRPower",             AVR_POWER),
  DOM_KEY_MAP("ChannelDown",          CHANNEL_DOWN),
  DOM_KEY_MAP("ChannelUp",            CHANNEL_UP),
  DOM_KEY_MAP("ColorF0Red",           COLOR_F0_RED),
  DOM_KEY_MAP("ColorF1Green",         COLOR_F1_GREEN),
  DOM_KEY_MAP("ColorF2Yellow",        COLOR_F2_YELLOW),
  DOM_KEY_MAP("ColorF3Blue",          COLOR_F3_BLUE),
  DOM_KEY_MAP("ColorF4Grey",          COLOR_F4_GREY),
  DOM_KEY_MAP("ColorF5Brown",         COLOR_F5_BROWN),
  DOM_KEY_MAP("ClosedCaptionToggle",  CLOSED_CAPTION_TOGGLE),
  DOM_KEY_MAP("Dimmer",               DIMMER),
  DOM_KEY_MAP("DisplaySwap",          DISPLAY_SWAP),
  DOM_KEY_MAP("Exit",                 EXIT),
  DOM_KEY_MAP("FavoriteClear0",       FAVORITE_CLEAR0),
  DOM_KEY_MAP("FavoriteClear1",       FAVORITE_CLEAR1),
  DOM_KEY_MAP("FavoriteClear2",       FAVORITE_CLEAR2),
  DOM_KEY_MAP("FavoriteClear3",       FAVORITE_CLEAR3),
  DOM_KEY_MAP("FavoriteRecall0",      FAVORITE_RECALL0),
  DOM_KEY_MAP("FavoriteRecall1",      FAVORITE_RECALL1),
  DOM_KEY_MAP("FavoriteRecall2",      FAVORITE_RECALL2),
  DOM_KEY_MAP("FavoriteRecall3",      FAVORITE_RECALL3),
  DOM_KEY_MAP("FavoriteStore0",       FAVORITE_STORE0),
  DOM_KEY_MAP("FavoriteStore1",       FAVORITE_STORE1),
  DOM_KEY_MAP("FavoriteStore2",       FAVORITE_STORE2),
  DOM_KEY_MAP("FavoriteStore3",       FAVORITE_STORE3),
  DOM_KEY_MAP("Guide",                GUIDE),
  DOM_KEY_MAP("GuideNextDay",         GUIDE_NEXT_DAY),
  DOM_KEY_MAP("GuidePreviousDay",     GUIDE_PREVIOUS_DAY),
  DOM_KEY_MAP("Info",                 INFO),
  DOM_KEY_MAP("InstantReplay",        INSTANT_REPLAY),
  DOM_KEY_MAP("Link",                 LINK),
  DOM_KEY_MAP("ListProgram",          LIST_PROGRAM),
  DOM_KEY_MAP("LiveContent",          LIVE_CONTENT),
  DOM_KEY_MAP("Lock",                 LOCK),
  DOM_KEY_MAP("MediaApps",            MEDIA_APPS),
  DOM_KEY_MAP("MediaFastForward",     MEDIA_FAST_FORWARD),
  DOM_KEY_MAP("MediaLast",            MEDIA_LAST),
  DOM_KEY_MAP("MediaPause",           MEDIA_PAUSE),
  DOM_KEY_MAP("MediaPlay",            MEDIA_PLAY),
  DOM_KEY_MAP("MediaRecord",          MEDIA_RECORD),
  DOM_KEY_MAP("MediaRewind",          MEDIA_REWIND),
  DOM_KEY_MAP("MediaSkip",            MEDIA_SKIP),
  DOM_KEY_MAP("NextFavoriteChannel",  NEXT_FAVORITE_CHANNEL),
  DOM_KEY_MAP("NextUserProfile",      NEXT_USER_PROFILE),
  DOM_KEY_MAP("OnDemand",             ON_DEMAND),
  DOM_KEY_MAP("PinPDown",             PINP_DOWN),
  DOM_KEY_MAP("PinPMove",             PINP_MOVE),
  DOM_KEY_MAP("PinPToggle",           PINP_TOGGLE),
  DOM_KEY_MAP("PinPUp",               PINP_UP),
  DOM_KEY_MAP("PlaySpeedDown",        PLAY_SPEED_DOWN),
  DOM_KEY_MAP("PlaySpeedReset",       PLAY_SPEED_RESET),
  DOM_KEY_MAP("PlaySpeedUp",          PLAY_SPEED_UP),
  DOM_KEY_MAP("RandomToggle",         RANDOM_TOGGLE),
  DOM_KEY_MAP("RcLowBattery",         RC_LOW_BATTERY),
  DOM_KEY_MAP("RecordSpeedNext",      RECORD_SPEED_NEXT),
  DOM_KEY_MAP("RfBypass",             RF_BYPASS),
  DOM_KEY_MAP("ScanChannelsToggle",   SCAN_CHANNELS_TOGGLE),
  DOM_KEY_MAP("ScreenModeNext",       SCREEN_MODE_NEXT),
  DOM_KEY_MAP("Settings",             SETTINGS),
  DOM_KEY_MAP("SplitScreenToggle",    SPLIT_SCREEN_TOGGLE),
  DOM_KEY_MAP("STBInput",             STB_INPUT),
  DOM_KEY_MAP("STBPower",             STB_POWER),
  DOM_KEY_MAP("Subtitle",             SUBTITLE),
  DOM_KEY_MAP("Teletext",             TELETEXT),
  DOM_KEY_MAP("TV",                   T_V),
  DOM_KEY_MAP("TVInput",              TV_INPUT),
  DOM_KEY_MAP("TVPower",              TV_POWER),
  DOM_KEY_MAP("VideoModeNext",        VIDEO_MODE_NEXT),
  DOM_KEY_MAP("Wink",                 WINK),
  DOM_KEY_MAP("ZoomToggle",           ZOOM_TOGGLE),

  DOM_KEY_MAP_END
};
