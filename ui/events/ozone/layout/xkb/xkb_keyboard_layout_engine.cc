// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/layout/xkb/xkb_keyboard_layout_engine.h"

#include <xkbcommon/xkbcommon-names.h>

#include "base/logging.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/dom3/dom_code.h"
#include "ui/events/keycodes/dom3/dom_key.h"
#include "ui/events/keycodes/dom4/keycode_converter.h"
#include "ui/events/ozone/layout/layout_util.h"
#include "ui/events/ozone/layout/xkb/xkb_keyboard_code_conversion.h"

namespace ui {

namespace {

// Temporarily hard-code an initial layout to test libxkbcommon.
// TODO: remove this after SetCurrentLayoutByName() is implemented.
const char* temporary_initial_keymap =
    "xkb_keymap{"
    "xkb_keycodes \"evdev_aliases(qwerty)\"{minimum=8;maximum=255;"
    "<ESC>=9;<AE01>=10;<AE02>=11;<AE03>=12;<AE04>=13;<AE05>=14;"
    "<AE06>=15;<AE07>=16;<AE08>=17;<AE09>=18;<AE10>=19;<AE11>=20;"
    "<AE12>=21;<BKSP>=22;<TAB>=23;<AD01>=24;<AD02>=25;<AD03>=26;"
    "<AD04>=27;<AD05>=28;<AD06>=29;<AD07>=30;<AD08>=31;<AD09>=32;"
    "<AD10>=33;<AD11>=34;<AD12>=35;<RTRN>=36;<LCTL>=37;<AC01>=38;"
    "<AC02>=39;<AC03>=40;<AC04>=41;<AC05>=42;<AC06>=43;<AC07>=44;"
    "<AC08>=45;<AC09>=46;<AC10>=47;<AC11>=48;<TLDE>=49;<LFSH>=50;"
    "<BKSL>=51;<AB01>=52;<AB02>=53;<AB03>=54;<AB04>=55;<AB05>=56;"
    "<AB06>=57;<AB07>=58;<AB08>=59;<AB09>=60;<AB10>=61;<RTSH>=62;"
    "<KPMU>=63;<LALT>=64;<SPCE>=65;<CAPS>=66;<FK01>=67;<FK02>=68;"
    "<FK03>=69;<FK04>=70;<FK05>=71;<FK06>=72;<FK07>=73;<FK08>=74;"
    "<FK09>=75;<FK10>=76;<FK11>=95;<FK12>=96;<FK13>=191;<FK14>=192;"
    "<FK15>=193;<FK16>=194;<FK17>=195;<FK18>=196;<FK19>=197;<FK20>=198;"
    "<FK21>=199;<FK22>=200;<FK23>=201;<FK24>=202;<LWIN>=133;<RWIN>=134;"
    "<NMLK>=77;<SCLK>=78;<KP7>=79;<KP8>=80;<KP9>=81;<KPSU>=82;<KP4>=83;"
    "<KP5>=84;<KP6>=85;<KPAD>=86;<KP1>=87;<KP2>=88;<KP3>=89;<KP0>=90;"
    "<KPDL>=91;<LVL3>=92;<LSGT>=94;<FK11>=95;<FK12>=96;<KATA>=98;<HIRA>=99;"
    "<HENK>=100;<HKTG>=101;<MUHE>=102;<KPEN>=104;<RCTL>=105;"
    "<KPDV>=106;<PRSC>=107;<RALT>=108;<LNFD>=109;<HOME>=110;<UP>=111;"
    "<PGUP>=112;<LEFT>=113;<RGHT>=114;<END>=115;<DOWN>=116;<PGDN>=117;"
    "<INS>=118;<DELE>=119;<MUTE>=121;<VOL->=122;<VOL+>=123;<POWR>=124;"
    "<MDSW>=203;<ALT>=204;<META>=205;<SUPR>=206;<HYPR>=207;};"
    "xkb_types \"complete\"{virtual_modifiers NumLock,Alt,LevelThree,LAlt,"
    "RAlt,RControl,LControl,ScrollLock,LevelFive,AltGr,Meta,Super,Hyper;"
    "type \"ONE_LEVEL\"{modifiers=none;level_name[Level1]=\"Any\";};"
    "type \"TWO_LEVEL\"{modifiers=Shift;map[Shift]=Level2;"
    "level_name[Level1]=\"Base\";level_name[Level2]=\"Shift\";};"
    "type \"ALPHABETIC\"{modifiers=Shift+Lock;"
    "map[Shift]=Level2;map[Lock]=Level2;"
    "level_name[Level1]=\"Base\";level_name[Level2]=\"Caps\";};"
    "type \"FOUR_LEVEL\"{modifiers=Shift+LevelThree;map[Shift]=Level2;"
    "map[LevelThree]=Level3;map[Shift+LevelThree]=Level4;"
    "level_name[Level1]=\"Base\";level_name[Level2]=\"Shift\";"
    "level_name[Level3]=\"Alt Base\";level_name[Level4]=\"Shift Alt\";};"
    "type \"KEYPAD\"{modifiers=Shift+NumLock;"
    "map[Shift]=Level2;map[NumLock]=Level2;"
    "level_name[Level1]=\"Base\";level_name[Level2]=\"Number\";};};"
    "xkb_compatibility \"complete\"{virtual_modifiers NumLock,Alt,LevelThree,"
    "LAlt,RAlt,RControl,LControl,ScrollLock,LevelFive,AltGr,Meta,Super,Hyper;"
    "interpret.useModMapMods=AnyLevel;interpret.repeat=False;"
    "interpret ISO_Level2_Latch+Exactly(Shift){useModMapMods=level1;"
    "action=LatchMods(modifiers=Shift,clearLocks,latchToLock);};"
    "interpret Shift_Lock+AnyOf(Shift+Lock){action=LockMods(modifiers=Shift);};"
    "interpret Num_Lock+AnyOf(all){virtualModifier=NumLock;"
    "action=LockMods(modifiers=NumLock);};"
    "interpret ISO_Level3_Shift+AnyOf(all){virtualModifier=LevelThree;"
    "useModMapMods=level1;action=SetMods(modifiers=LevelThree,clearLocks);};"
    "interpret ISO_Level3_Latch+AnyOf(all){"
    "virtualModifier=LevelThree;useModMapMods=level1;"
    "action=LatchMods(modifiers=LevelThree,clearLocks,latchToLock);};"
    "interpret ISO_Level3_Lock+AnyOf(all){virtualModifier=LevelThree;"
    "useModMapMods=level1;action=LockMods(modifiers=LevelThree);};"
    "interpret Alt_L+AnyOf(all){virtualModifier=Alt;"
    "action=SetMods(modifiers=modMapMods,clearLocks);};"
    "interpret Alt_R+AnyOf(all){virtualModifier=Alt;"
    "action=SetMods(modifiers=modMapMods,clearLocks);};"
    "interpret Meta_L+AnyOf(all){virtualModifier=Meta;"
    "action=SetMods(modifiers=modMapMods,clearLocks);};"
    "interpret Meta_R+AnyOf(all){virtualModifier=Meta;"
    "action=SetMods(modifiers=modMapMods,clearLocks);};"
    "interpret Super_L+AnyOf(all){virtualModifier=Super;"
    "action=SetMods(modifiers=modMapMods,clearLocks);};"
    "interpret Super_R+AnyOf(all){virtualModifier=Super;"
    "action=SetMods(modifiers=modMapMods,clearLocks);};"
    "interpret Hyper_L+AnyOf(all){virtualModifier=Hyper;"
    "action=SetMods(modifiers=modMapMods,clearLocks);};"
    "interpret Hyper_R+AnyOf(all){virtualModifier=Hyper;"
    "action=SetMods(modifiers=modMapMods,clearLocks);};"
    "interpret Mode_switch+AnyOfOrNone(all){virtualModifier=AltGr;"
    "useModMapMods=level1;action=SetGroup(group=+1);};"
    "interpret ISO_Level3_Shift+AnyOfOrNone(all){"
    "action=SetMods(modifiers=LevelThree,clearLocks);};"
    "interpret ISO_Level3_Latch+AnyOfOrNone(all){"
    "action=LatchMods(modifiers=LevelThree,clearLocks,latchToLock);};"
    "interpret ISO_Level3_Lock+AnyOfOrNone(all){"
    "action=LockMods(modifiers=LevelThree);};"
    "interpret Alt_L+AnyOfOrNone(all){"
    "action=SetMods(modifiers=Alt,clearLocks);};"
    "interpret Alt_R+AnyOfOrNone(all){"
    "action=SetMods(modifiers=Alt,clearLocks);};"
    "interpret Meta_L+AnyOfOrNone(all){"
    "action=SetMods(modifiers=Meta,clearLocks);};"
    "interpret Meta_R+AnyOfOrNone(all){"
    "action=SetMods(modifiers=Meta,clearLocks);};"
    "interpret Super_L+AnyOfOrNone(all){"
    "action=SetMods(modifiers=Super,clearLocks);};"
    "interpret Super_R+AnyOfOrNone(all){"
    "action=SetMods(modifiers=Super,clearLocks);};"
    "interpret Hyper_L+AnyOfOrNone(all){"
    "action=SetMods(modifiers=Hyper,clearLocks);};"
    "interpret Hyper_R+AnyOfOrNone(all){"
    "action=SetMods(modifiers=Hyper,clearLocks);};"
    "interpret Shift_L+AnyOfOrNone(all){"
    "action=SetMods(modifiers=Shift,clearLocks);};"
    "interpret XF86Switch_VT_1+AnyOfOrNone(all){repeat=True;"
    "action=SwitchScreen(screen=1,!same);};"
    "interpret XF86Switch_VT_2+AnyOfOrNone(all){repeat=True;"
    "action=SwitchScreen(screen=2,!same);};"
    "interpret XF86Switch_VT_3+AnyOfOrNone(all){repeat=True;"
    "action=SwitchScreen(screen=3,!same);};"
    "interpret XF86Switch_VT_4+AnyOfOrNone(all){repeat=True;"
    "action=SwitchScreen(screen=4,!same);};"
    "interpret Caps_Lock+AnyOfOrNone(all){action=LockMods(modifiers=Lock);};"
    "interpret Any+Exactly(Lock){action=LockMods(modifiers=Lock);};"
    "interpret Any+AnyOf(all){action=SetMods(modifiers=modMapMods,clearLocks);"
    "};};"
    "xkb_symbols \"pc_us_inet(evdev)\"{name[group1]=\"English (US)\";"
    "key<ESC>{[Escape]};key<AE01>{[1,exclam]};key<AE02>{[2,at]};"
    "key<AE03>{[3,numbersign]};key<AE04>{[4,dollar]};key<AE05>{[5,percent]};"
    "key<AE06>{[6,asciicircum]};key<AE07>{[7,ampersand]};"
    "key<AE08>{[8,asterisk]};key<AE09>{[9,parenleft]};"
    "key<AE10>{[0,parenright]};key<AE11>{[minus,underscore]};"
    "key<AE12>{[equal,plus]};key<BKSP>{[BackSpace,BackSpace]};"
    "key<TAB>{[Tab,ISO_Left_Tab]};key<AD01>{[q,Q]};key<AD02>{[w,W]};"
    "key<AD03>{[e,E]};key<AD04>{[r,R]};key<AD05>{[t,T]};key<AD06>{[y,Y]};"
    "key<AD07>{[u,U]};key<AD08>{[i,I]};key<AD09>{[o,O]};key<AD10>{[p,P]};"
    "key<AD11>{[bracketleft,braceleft]};key<AD12>{[bracketright,braceright]};"
    "key<RTRN>{[Return]};key<LCTL>{[Control_L]};key<AC01>{[a,A]};"
    "key<AC02>{[s,S]};key<AC03>{[d,D]};key<AC04>{[f,F]};key<AC05>{[g,G]};"
    "key<AC06>{[h,H]};key<AC07>{[j,J]};key<AC08>{[k,K]};key<AC09>{[l,L]};"
    "key<AC10>{[semicolon,colon]};key<AC11>{[apostrophe,quotedbl]};"
    "key<TLDE>{[grave,asciitilde]};key<LFSH>{[Shift_L]};"
    "key<BKSL>{[backslash,bar]};key<AB01>{[z,Z]};key<AB02>{[x,X]};"
    "key<AB03>{[c,C]};key<AB04>{[v,V]};key<AB05>{[b,B]};key<AB06>{[n,N]};"
    "key<AB07>{[m,M]};key<AB08>{[comma,less]};key<AB09>{[period,greater]};"
    "key<AB10>{[slash,question]};key<RTSH>{[Shift_R]};"
    "key<KPMU>{[KP_Multiply,KP_Multiply]};key<LALT>{[Alt_L,Meta_L]};"
    "key<SPCE>{[space]};key<CAPS>{[Caps_Lock]};key<FK01>{[F1,F1]};"
    "key<FK02>{[F2,F2]};key<FK03>{[F3,F3]};key<FK04>{[F4,F4]};"
    "key<FK05>{[F5,F5]};key<FK06>{[F6,F6]};key<FK07>{[F7,F7]};"
    "key<FK08>{[F8,F8]};key<FK09>{[F9,F9]};key<FK10>{[F10,F10]};"
    "key<FK11>{[F11,F11]};key<FK12>{[F12,F12]};key<FK13>{[F13,F13]};"
    "key<FK14>{[F14,F14]};key<FK15>{[F15,F15]};key<FK16>{[F16,F16]};"
    "key<FK17>{[F17,F17]};key<FK18>{[F18,F18]};key<FK19>{[F19,F19]};"
    "key<FK20>{[F20,F20]};key<FK21>{[F21,F21]};key<FK22>{[F22,F22]};"
    "key<FK23>{[F23,F23]}; key<FK24>{[F24,F24]};"
    "key<NMLK>{[Num_Lock]};key<SCLK>{[Scroll_Lock]};"
    "key<KP7>{[KP_Home,KP_7]};key<KP8>{[KP_Up,KP_8]};key<KP9>{[KP_Prior,KP_9]};"
    "key<KPSU>{[KP_Subtract,KP_Subtract]};key<KP4>{[KP_Left,KP_4]};"
    "key<KP5>{[KP_Begin,KP_5]};key<KP6>{[KP_Right,KP_6]};"
    "key<KPAD>{[KP_Add,KP_Add]};key<KP1>{[KP_End,KP_1]};"
    "key<KP2>{[KP_Down,KP_2]};key<KP3>{[KP_Next,KP_3]};"
    "key<KP0>{[KP_Insert,KP_0]};key<KPDL>{[KP_Delete,KP_Decimal]};"
    "key<LVL3>{[ISO_Level3_Shift]};key<LSGT>{[less,greater, bar,brokenbar]};"
    "key<FK11>{[F11,F11]};key<FK12>{[F12,F12]};key<KATA>{[Katakana]};"
    "key<HIRA>{[Hiragana]};key<HENK>{[Henkan_Mode]};"
    "key<HKTG>{[Hiragana_Katakana]};key<MUHE>{[Muhenkan]};"
    "key<KPEN>{[KP_Enter]};key<RCTL>{[Control_R]};key<MDSW>{[Mode_switch]};"
    "key<KPDV>{[KP_Divide,KP_Divide]};key<PRSC>{[Print,Sys_Req]};"
    "key<RALT>{type=\"TWO_LEVEL\",symbols[Group1]=[Alt_R,Meta_R]};"
    "key<LNFD>{[Linefeed]};key<HOME>{[Home]};key<UP>{[Up]};key<PGUP>{[Prior]};"
    "key<LEFT>{[Left]};key<RGHT>{[Right]};key<END>{[End]};key<DOWN>{[Down]};"
    "key<PGDN>{[Next]};key<INS>{[Insert]};key<DELE>{[Delete]};"
    "key<MUTE>{[XF86AudioMute]};key<VOL->{[XF86AudioLowerVolume]};"
    "key<VOL+>{[XF86AudioRaiseVolume]};key<POWR>{[XF86PowerOff]};"
    "key<ALT>{[NoSymbol,Alt_L]};key<META>{[NoSymbol,Meta_L]};"
    "key<SUPR>{[NoSymbol,Super_L]};key<HYPR>{[NoSymbol,Hyper_L]};"
    "key<LWIN>{[Super_L,Super_L]};key<RWIN>{[Super_R,Super_R]};"
    "modifier_map Control{<LCTL>};modifier_map Shift{<LFSH>};"
    "modifier_map Shift{<RTSH>};modifier_map Mod1{<LALT>};"
    "modifier_map Lock{<CAPS>};modifier_map Mod2{<NMLK>};"
    "modifier_map Mod5{<LVL3>};modifier_map Control{<RCTL>};"
    "modifier_map Mod1{<RALT>};modifier_map Mod5{<MDSW>};"
    "modifier_map Mod1{<META>};modifier_map Mod4{<SUPR>};"
    "modifier_map Mod4{<LWIN>};modifier_map Mod4{<RWIN>};"
    "modifier_map Mod4{<HYPR>};};};";

DomKey CharacterToDomKey(base::char16 character) {
  switch (character) {
    case 0x08:
      return DomKey::BACKSPACE;
    case 0x09:
      return DomKey::TAB;
    case 0x0A:
    case 0x0D:
      return DomKey::ENTER;
    case 0x1B:
      return DomKey::ESCAPE;
    default:
      return DomKey::CHARACTER;
  }
}

KeyboardCode AlphanumericKeyboardCode(base::char16 character) {
  // Plain ASCII letters and digits map directly to VKEY values.
  if ((character >= '0') && (character <= '9'))
    return static_cast<KeyboardCode>(VKEY_0 + character - '0');
  if ((character >= 'a') && (character <= 'z'))
    return static_cast<KeyboardCode>(VKEY_A + character - 'a');
  if ((character >= 'A') && (character <= 'Z'))
    return static_cast<KeyboardCode>(VKEY_A + character - 'A');
  return VKEY_UNKNOWN;
}

// These tables map layout-dependent printable characters/codes
// to legacy Windows-based VKEY values.
//
// VKEYs are determined by the character produced from a DomCode without
// any modifiers, plus zero or more of the DomCode itself, the character
// produced with the Shift modifier, and the character produced with the
// AltGr modifier.

// A table of one or more PrintableSubEntry cases applies when the VKEY is
// not determined by the unmodified character value alone. Each such table
// corresponds to one unmodified character value. For an entry to match,
// the dom_code must match, and, if test_X is set, then the character for
// the key plus modifier X must also match.
struct PrintableSubEntry {
  DomCode dom_code;
  bool test_shift : 1;
  bool test_altgr : 1;
  base::char16 shift_character;
  base::char16 altgr_character;
  KeyboardCode key_code;
};

const base::char16 kNone = 0xFFFE;
const base::char16 kAny = 0xFFFF;

// U+0021 exclamation mark
const PrintableSubEntry kU0021[] = {
    {DomCode::DIGIT1, 0, 0, kAny, kAny, VKEY_1},
    {DomCode::DIGIT8, 0, 0, kAny, kAny, VKEY_8},
    {DomCode::SLASH, 0, 0, kAny, kAny, VKEY_OEM_8}};

// U+0022 quote
const PrintableSubEntry kU0022[] = {
    {DomCode::DIGIT2, 0, 0, kAny, kAny, VKEY_2},
    {DomCode::DIGIT3, 0, 0, kAny, kAny, VKEY_3}};

// U+0023 number sign
const PrintableSubEntry kU0023[] = {
    {DomCode::BACKQUOTE, 0, 0, kAny, kAny, VKEY_OEM_7},
    {DomCode::BACKSLASH, 1, 0, 0x0027, kAny, VKEY_OEM_2},   // apostrophe
    {DomCode::BACKSLASH, 1, 1, 0x007E, kNone, VKEY_OEM_7}};  // ~, NoSymbol

// U+0024 dollar sign
const PrintableSubEntry kU0024[] = {
    {DomCode::BRACKET_RIGHT, 0, 0, kAny, kAny, VKEY_OEM_1},
    {DomCode::BACKSLASH, 0, 0, kAny, kAny, VKEY_OEM_8}};

// U+0027 apostrophe
const PrintableSubEntry kU0027[] = {
    {DomCode::DIGIT4, 0, 0, kAny, kAny, VKEY_4},
    {DomCode::KEY_Q, 0, 0, kAny, kAny, VKEY_OEM_7},
    {DomCode::BRACKET_RIGHT, 0, 0, kAny, kAny, VKEY_OEM_1},
    {DomCode::SLASH, 0, 0, kAny, kAny, VKEY_OEM_7},
    {DomCode::QUOTE, 1, 0, 0x0022, kAny, VKEY_OEM_7},      // quote
    {DomCode::BACKQUOTE, 1, 0, 0x0022, kAny, VKEY_OEM_3},  // quote
    {DomCode::BACKQUOTE, 1, 0, 0x00B7, kAny, VKEY_OEM_5},  // middle dot
    {DomCode::BACKSLASH, 1, 0, kNone, kAny, VKEY_OEM_5},  // NoSymbol
    {DomCode::MINUS, 1, 1, 0x003F, kNone, VKEY_OEM_4},      // ?, NoSymbol
    {DomCode::MINUS, 1, 1, 0x003F, 0x00DD, VKEY_OEM_4},      // ?, Y acute
    {DomCode::EQUAL, 1, 1, 0x002A, kNone, VKEY_OEM_PLUS},   // *, NoSymbol
    {DomCode::QUOTE, 1, 1, 0x0040, kNone, VKEY_OEM_3},      // @, NoSymbol
    {DomCode::BACKSLASH, 1, 1, 0x002A, kNone, VKEY_OEM_2},  // *, NoSymbol
    {DomCode::BACKSLASH, 1, 1, 0x002A, 0x00BD, VKEY_OEM_5},  // *, one half
    {DomCode::BACKSLASH, 1, 1, 0x002A, 0x0141, VKEY_OEM_2},  // *, L stroke
    {DomCode::KEY_Z, 1, 1, 0x0022, kNone, VKEY_Z},          // quote, NoSymbol
    {DomCode::KEY_Z, 1, 1, 0x0022, 0x0158, VKEY_OEM_7}};     // quote, R caron

// U+0028 left parenthesis
const PrintableSubEntry kU0028[] = {
    {DomCode::DIGIT5, 0, 0, kAny, kAny, VKEY_5},
    {DomCode::DIGIT9, 0, 0, kAny, kAny, VKEY_9},
    {DomCode::BACKSLASH, 0, 0, kAny, kAny, VKEY_OEM_5}};

// U+0029 right parenthesis
const PrintableSubEntry kU0029[] = {
    {DomCode::DIGIT0, 0, 0, kAny, kAny, VKEY_0},
    {DomCode::MINUS, 0, 0, kAny, kAny, VKEY_OEM_4},
    {DomCode::BRACKET_RIGHT, 0, 0, kAny, kAny, VKEY_OEM_6}};

// U+002A *
const PrintableSubEntry kU002A[] = {
    {DomCode::BRACKET_RIGHT, 0, 0, kAny, kAny, VKEY_OEM_1},
    {DomCode::BACKSLASH, 0, 0, kAny, kAny, VKEY_OEM_5}};

// U+002B plus sign
const PrintableSubEntry kU002B[] = {
    {DomCode::DIGIT1, 0, 0, kAny, kAny, VKEY_1},
    {DomCode::EQUAL, 0, 0, kAny, kAny, VKEY_OEM_PLUS},
    {DomCode::BRACKET_LEFT, 0, 0, kAny, kAny, VKEY_OEM_PLUS},
    {DomCode::BRACKET_RIGHT, 0, 0, kAny, kAny, VKEY_OEM_PLUS},
    {DomCode::SEMICOLON, 0, 0, kAny, kAny, VKEY_OEM_PLUS},
    {DomCode::BACKSLASH, 0, 0, kAny, kAny, VKEY_OEM_2},
    {DomCode::MINUS, 1, 1, 0x003F, kNone, VKEY_OEM_PLUS},   // ?, NoSymbol
    {DomCode::MINUS, 1, 1, 0x003F, 0x005C, VKEY_OEM_MINUS},  // ?, backslash
    {DomCode::MINUS, 1, 1, 0x003F, 0x0151, VKEY_OEM_PLUS}};  // ?, o''

// U+002C comma
const PrintableSubEntry kU002C[] = {
    {DomCode::DIGIT3, 0, 0, kAny, kAny, VKEY_3},
    {DomCode::DIGIT5, 0, 0, kAny, kAny, VKEY_5},
    {DomCode::DIGIT6, 0, 0, kAny, kAny, VKEY_6},
    {DomCode::DIGIT9, 0, 0, kAny, kAny, VKEY_9},
    {DomCode::KEY_W, 0, 0, kAny, kAny, VKEY_OEM_COMMA},
    {DomCode::KEY_V, 0, 0, kAny, kAny, VKEY_OEM_COMMA},
    {DomCode::KEY_M, 0, 0, kAny, kAny, VKEY_OEM_COMMA},
    {DomCode::COMMA, 0, 0, kAny, kAny, VKEY_OEM_COMMA}};

// U+002D hyphen-minus
const PrintableSubEntry kU002D[] = {
    {DomCode::DIGIT2, 0, 0, kAny, kAny, VKEY_2},
    {DomCode::DIGIT6, 0, 0, kAny, kAny, VKEY_6},
    {DomCode::MINUS, 0, 0, kAny, kAny, VKEY_OEM_MINUS},
    {DomCode::KEY_A, 0, 0, kAny, kAny, VKEY_OEM_MINUS},
    {DomCode::QUOTE, 0, 0, kAny, kAny, VKEY_OEM_MINUS},
    {DomCode::SLASH, 1, 0, 0x003D, kAny, VKEY_OEM_MINUS},   // =
    {DomCode::EQUAL, 1, 1, 0x005F, kNone, VKEY_OEM_MINUS},   // _, NoSymbol
    {DomCode::EQUAL, 1, 1, 0x005F, 0x0157, VKEY_OEM_4},       // _, r cedilla
    {DomCode::SLASH, 1, 1, 0x005F, kNone, VKEY_OEM_MINUS},   // _, NoSymbol
    {DomCode::SLASH, 1, 1, 0x005F, 0x002A, VKEY_OEM_MINUS},   // _, *
    {DomCode::SLASH, 1, 1, 0x005F, 0x002F, VKEY_OEM_2},       // _, /
    {DomCode::SLASH, 1, 1, 0x005F, 0x006E, VKEY_OEM_MINUS}};  // _, n

// U+002E full stop
const PrintableSubEntry kU002E[] = {
    {DomCode::DIGIT7, 0, 0, kAny, kAny, VKEY_7},
    {DomCode::DIGIT8, 0, 0, kAny, kAny, VKEY_8},
    {DomCode::KEY_E, 0, 0, kAny, kAny, VKEY_OEM_PERIOD},
    {DomCode::KEY_R, 0, 0, kAny, kAny, VKEY_OEM_PERIOD},
    {DomCode::KEY_O, 0, 0, kAny, kAny, VKEY_OEM_PERIOD},
    {DomCode::QUOTE, 0, 0, kAny, kAny, VKEY_OEM_7},
    {DomCode::PERIOD, 0, 0, kAny, kAny, VKEY_OEM_PERIOD},
    {DomCode::SLASH, 0, 0, kAny, kAny, VKEY_OEM_2}};

// U+002F /
const PrintableSubEntry kU002F[] = {
    {DomCode::MINUS, 0, 0, kAny, kAny, VKEY_OEM_4},
    {DomCode::BRACKET_LEFT, 0, 0, kAny, kAny, VKEY_OEM_2},
    {DomCode::BACKQUOTE, 0, 0, kAny, kAny, VKEY_OEM_7},
    {DomCode::BACKSLASH, 0, 0, kAny, kAny, VKEY_OEM_5},
    {DomCode::SLASH, 0, 0, kAny, kAny, VKEY_OEM_2},
    {DomCode::DIGIT3, 1, 0, 0x0033, kAny, VKEY_3},       // 3
    {DomCode::DIGIT3, 1, 0, 0x003F, kAny, VKEY_OEM_2},   // ?
    {DomCode::DIGIT0, 1, 0, 0x0030, kAny, VKEY_0},       // 0
    {DomCode::DIGIT0, 1, 0, 0x003F, kAny, VKEY_OEM_2}};  // ?

// U+003A colon
const PrintableSubEntry kU003A[] = {
    {DomCode::DIGIT1, 0, 0, kAny, kAny, VKEY_1},
    {DomCode::DIGIT5, 0, 0, kAny, kAny, VKEY_5},
    {DomCode::DIGIT6, 0, 0, kAny, kAny, VKEY_6},
    {DomCode::PERIOD, 0, 0, kAny, kAny, VKEY_OEM_2}};

// U+003B semicolon
const PrintableSubEntry kU003B[] = {
    {DomCode::DIGIT4, 0, 0, kAny, kAny, VKEY_4},
    {DomCode::DIGIT8, 0, 0, kAny, kAny, VKEY_8},
    {DomCode::KEY_Q, 0, 0, kAny, kAny, VKEY_OEM_1},
    {DomCode::BRACKET_LEFT, 0, 0, kAny, kAny, VKEY_OEM_1},
    {DomCode::BRACKET_RIGHT, 0, 0, kAny, kAny, VKEY_OEM_6},
    {DomCode::SEMICOLON, 0, 0, kAny, kAny, VKEY_OEM_1},
    {DomCode::BACKQUOTE, 0, 0, kAny, kAny, VKEY_OEM_3},
    {DomCode::KEY_Z, 0, 0, kAny, kAny, VKEY_OEM_1},
    {DomCode::COMMA, 0, 0, kAny, kAny, VKEY_OEM_PERIOD},
    {DomCode::SLASH, 0, 0, kAny, kAny, VKEY_OEM_2}};

// U+003D =
const PrintableSubEntry kU003D[] = {
    {DomCode::DIGIT8, 0, 0, kAny, kAny, VKEY_8},
    {DomCode::EQUAL, 0, 0, kAny, kAny, VKEY_OEM_PLUS},
    {DomCode::BRACKET_RIGHT, 0, 0, kAny, kAny, VKEY_OEM_PLUS},
    {DomCode::SLASH, 1, 0, 0x0025, kAny, VKEY_OEM_8},       // %
    {DomCode::SLASH, 1, 0, 0x002B, kAny, VKEY_OEM_PLUS},    // +
    {DomCode::MINUS, 1, 1, 0x0025, kNone, VKEY_OEM_PLUS},    // %, NoSymbol
    {DomCode::MINUS, 1, 1, 0x0025, 0x002D, VKEY_OEM_MINUS}};  // %, -

// U+003F ?
const PrintableSubEntry kU003F[] = {
    {DomCode::DIGIT2, 0, 0, kAny, kAny, VKEY_2},
    {DomCode::DIGIT7, 0, 0, kAny, kAny, VKEY_7},
    {DomCode::DIGIT8, 0, 0, kAny, kAny, VKEY_8},
    {DomCode::MINUS, 0, 0, kAny, kAny, VKEY_OEM_PLUS}};

// U+0040 @
const PrintableSubEntry kU0040[] = {
    {DomCode::BRACKET_RIGHT, 0, 0, kAny, kAny, VKEY_OEM_6},
    {DomCode::BACKQUOTE, 0, 0, kAny, kAny, VKEY_OEM_7}};

// U+005B left square bracket
const PrintableSubEntry kU005B[] = {
    {DomCode::DIGIT1, 0, 0, kAny, kAny, VKEY_OEM_4},
    {DomCode::MINUS, 0, 0, kAny, kAny, VKEY_OEM_4},
    {DomCode::BRACKET_LEFT, 0, 0, kAny, kAny, VKEY_OEM_4},
    {DomCode::BRACKET_RIGHT, 0, 0, kAny, kAny, VKEY_OEM_6},
    {DomCode::QUOTE, 0, 0, kAny, kAny, VKEY_OEM_7}};

// U+005C backslash
const PrintableSubEntry kU005C[] = {
    {DomCode::EQUAL, 0, 0, kAny, kAny, VKEY_OEM_4},
    {DomCode::BACKSLASH, 1, 0, 0x002F, kAny, VKEY_OEM_7},   // /
    {DomCode::BACKSLASH, 1, 0, 0x007C, kAny, VKEY_OEM_5},   // |
    {DomCode::BACKQUOTE, 1, 1, 0x007C, 0x0031, VKEY_OEM_5},   // |, 1
    {DomCode::BACKQUOTE, 1, 1, 0x007C, 0x0145, VKEY_OEM_3}};  // |, N cedilla

// U+005D right square bracket
const PrintableSubEntry kU005D[] = {
    {DomCode::DIGIT2, 0, 0, kAny, kAny, VKEY_OEM_6},
    {DomCode::EQUAL, 0, 0, kAny, kAny, VKEY_OEM_6},
    {DomCode::BRACKET_RIGHT, 0, 0, kAny, kAny, VKEY_OEM_6},
    {DomCode::BACKQUOTE, 0, 0, kAny, kAny, VKEY_OEM_3},
    {DomCode::BACKSLASH, 0, 0, kAny, kAny, VKEY_OEM_5}};

// U+005F _
const PrintableSubEntry kU005F[] = {
    {DomCode::DIGIT8, 0, 0, kAny, kAny, VKEY_8},
    {DomCode::MINUS, 0, 0, kAny, kAny, VKEY_OEM_MINUS}};

// U+0060 grave accent
const PrintableSubEntry kU0060[] = {
    {DomCode::BACKQUOTE, 1, 0, kNone, kAny, VKEY_OEM_3},   // NoSymbol
    {DomCode::BACKQUOTE, 1, 0, 0x00AC, kAny, VKEY_OEM_8},   // not
    {DomCode::BACKQUOTE, 1, 1, 0x007E, kNone, VKEY_OEM_3},   // ~, NoSymbol
    {DomCode::BACKQUOTE, 1, 1, 0x007E, 0x0031, VKEY_OEM_3},   // ~, 1
    {DomCode::BACKQUOTE, 1, 1, 0x007E, 0x003B, VKEY_OEM_3},   // ~, ;
    {DomCode::BACKQUOTE, 1, 1, 0x007E, 0x0060, VKEY_OEM_3},   // ~, `
    {DomCode::BACKQUOTE, 1, 1, 0x007E, 0x00BF, VKEY_OEM_3},   // ~, inverted ?
    {DomCode::BACKQUOTE, 1, 1, 0x007E, 0x0151, VKEY_OEM_3}};  // ~, o''

// U+00A7 section
const PrintableSubEntry kU00A7[] = {
    {DomCode::DIGIT4, 0, 0, kAny, kAny, VKEY_4},
    {DomCode::DIGIT6, 0, 0, kAny, kAny, VKEY_6},
    {DomCode::QUOTE, 0, 0, kAny, kAny, VKEY_OEM_7},
    {DomCode::BACKQUOTE, 1, 0, 0x00B0, kAny, VKEY_OEM_2},   // degree
    {DomCode::BACKQUOTE, 1, 0, 0x00BD, kAny, VKEY_OEM_5}};  // one half

// U+00AB left-pointing double angle quote
const PrintableSubEntry kU00AB[] = {
    {DomCode::DIGIT8, 0, 0, kAny, kAny, VKEY_8},
    {DomCode::EQUAL, 0, 0, kAny, kAny, VKEY_OEM_6}};

// U+00B0 degree
const PrintableSubEntry kU00B0[] = {
    {DomCode::EQUAL, 0, 0, kAny, kAny, VKEY_OEM_2},
    {DomCode::BACKQUOTE, 0, 0, kAny, kAny, VKEY_OEM_7}};

// U+00BA masculine ordinal indicator
const PrintableSubEntry kU00BA[] = {
    {DomCode::QUOTE, 0, 0, kAny, kAny, VKEY_OEM_7},
    {DomCode::BACKQUOTE, 0, 0, kAny, kAny, VKEY_OEM_5}};

// U+00E0 a grave
const PrintableSubEntry kU00E0[] = {
    {DomCode::DIGIT0, 0, 0, kAny, kAny, VKEY_0},
    {DomCode::BACKSLASH, 0, 0, kAny, kAny, VKEY_OEM_5},
    {DomCode::QUOTE, 1, 0, 0x00B0, kAny, VKEY_OEM_7},   // degree
    {DomCode::QUOTE, 1, 0, 0x00E4, kAny, VKEY_OEM_5}};  // a diaeresis

// U+00E1 a acute
const PrintableSubEntry kU00E1[] = {
    {DomCode::DIGIT8, 0, 0, kAny, kAny, VKEY_8},
    {DomCode::QUOTE, 0, 0, kAny, kAny, VKEY_OEM_7}};

// U+00E2 a circumflex
const PrintableSubEntry kU00E2[] = {
    {DomCode::DIGIT2, 0, 0, kAny, kAny, VKEY_2},
    {DomCode::BACKSLASH, 0, 0, kAny, kAny, VKEY_OEM_5}};

// U+00E4 a diaeresis
const PrintableSubEntry kU00E4[] = {
    {DomCode::BRACKET_RIGHT, 0, 0, kAny, kAny, VKEY_OEM_6},
    {DomCode::QUOTE, 1, 0, 0x00E0, kAny, VKEY_OEM_5},   // a grave
    {DomCode::QUOTE, 1, 1, 0x00C4, kNone, VKEY_OEM_7},   // A dia., NoSymbol
    {DomCode::QUOTE, 1, 1, 0x00C4, 0x015A, VKEY_OEM_7},   // A dia., S acute
    {DomCode::QUOTE, 1, 1, 0x00C4, 0x0159, VKEY_OEM_7}};  // A dia., r caron

// U+00E6 ae
const PrintableSubEntry kU00E6[] = {
    {DomCode::SEMICOLON, 0, 0, kAny, kAny, VKEY_OEM_3},
    {DomCode::QUOTE, 0, 0, kAny, kAny, VKEY_OEM_7}};

// U+00E7 c cedilla
const PrintableSubEntry kU00E7[] = {
    {DomCode::DIGIT9, 0, 0, kAny, kAny, VKEY_9},
    {DomCode::BRACKET_LEFT, 0, 0, kAny, kAny, VKEY_OEM_4},
    {DomCode::BRACKET_RIGHT, 0, 0, kAny, kAny, VKEY_OEM_6},
    {DomCode::QUOTE, 0, 0, kAny, kAny, VKEY_OEM_7},
    {DomCode::BACKSLASH, 0, 0, kAny, kAny, VKEY_OEM_2},
    {DomCode::COMMA, 0, 0, kAny, kAny, VKEY_OEM_COMMA},
    {DomCode::SEMICOLON, 1, 1, 0x00C7, kNone, VKEY_OEM_1},   // C ced., NoSy
    {DomCode::SEMICOLON, 1, 1, 0x00C7, 0x00DE, VKEY_OEM_3}};  // C ced., Thorn

// U+00E8 e grave
const PrintableSubEntry kU00E8[] = {
    {DomCode::DIGIT7, 0, 0, kAny, kAny, VKEY_7},
    {DomCode::BRACKET_LEFT, 0, 0, kAny, kAny, VKEY_OEM_1},
    {DomCode::QUOTE, 0, 0, kAny, kAny, VKEY_OEM_3}};

// U+00E9 e acute
const PrintableSubEntry kU00E9[] = {
    {DomCode::DIGIT2, 0, 0, kAny, kAny, VKEY_2},
    {DomCode::DIGIT0, 0, 0, kAny, kAny, VKEY_0},
    {DomCode::SLASH, 0, 0, kAny, kAny, VKEY_OEM_2},
    {DomCode::SEMICOLON, 1, 0, 0x00C9, kAny, VKEY_OEM_1},   // E acute
    {DomCode::SEMICOLON, 1, 0, 0x00F6, kAny, VKEY_OEM_7}};  // o diaeresis

// U+00ED i acute
const PrintableSubEntry kU00ED[] = {
    {DomCode::DIGIT9, 0, 0, kAny, kAny, VKEY_9},
    {DomCode::BACKQUOTE, 0, 0, kAny, kAny, VKEY_0}};

// U+00F0 eth
const PrintableSubEntry kU00F0[] = {
    {DomCode::BRACKET_LEFT, 0, 0, kAny, kAny, VKEY_OEM_6},
    {DomCode::BRACKET_RIGHT, 0, 0, kAny, kAny, VKEY_OEM_1}};

// U+00F3 o acute
const PrintableSubEntry kU00F3[] = {
    {DomCode::EQUAL, 0, 0, kAny, kAny, VKEY_OEM_PLUS},
    {DomCode::BACKSLASH, 0, 0, kAny, kAny, VKEY_OEM_5}};

// U+00F4 o circumflex
const PrintableSubEntry kU00F4[] = {
    {DomCode::DIGIT4, 0, 0, kAny, kAny, VKEY_4},
    {DomCode::SEMICOLON, 0, 0, kAny, kAny, VKEY_OEM_1}};

// U+00F6 o diaeresis
const PrintableSubEntry kU00F6[] = {
    {DomCode::DIGIT0, 0, 0, kAny, kAny, VKEY_OEM_3},
    {DomCode::MINUS, 0, 0, kAny, kAny, VKEY_OEM_PLUS},
    {DomCode::BRACKET_LEFT, 0, 0, kAny, kAny, VKEY_OEM_4},
    {DomCode::SEMICOLON, 1, 0, 0x00E9, kAny, VKEY_OEM_7},   // e acute
    {DomCode::SEMICOLON, 1, 1, 0x00D6, kNone, VKEY_OEM_3},   // O dia., NoSy
    {DomCode::SEMICOLON, 1, 1, 0x00D6, 0x0162, VKEY_OEM_3}};  // O dia., T ced.

// U+00F8 o stroke
const PrintableSubEntry kU00F8[] = {
    {DomCode::SEMICOLON, 0, 0, kAny, kAny, VKEY_OEM_3},
    {DomCode::QUOTE, 0, 0, kAny, kAny, VKEY_OEM_7}};

// U+00F9 u grave
const PrintableSubEntry kU00F9[] = {
    {DomCode::QUOTE, 0, 0, kAny, kAny, VKEY_OEM_3},
    {DomCode::BACKSLASH, 0, 0, kAny, kAny, VKEY_OEM_2}};

// U+00FA u acute
const PrintableSubEntry kU00FA[] = {
    {DomCode::BRACKET_LEFT, 0, 0, kAny, kAny, VKEY_OEM_4},
    {DomCode::BRACKET_RIGHT, 0, 0, kAny, kAny, VKEY_OEM_6}};

// U+00FC u diaeresis
const PrintableSubEntry kU00FC[] = {
    {DomCode::KEY_W, 0, 0, kAny, kAny, VKEY_W},
    {DomCode::BRACKET_LEFT, 1, 0, 0x00E8, kAny, VKEY_OEM_1},   // e grave
    {DomCode::MINUS, 1, 1, 0x00DC, kNone, VKEY_OEM_2},          // U dia., NoSy
    {DomCode::BRACKET_LEFT, 1, 1, 0x00DC, kNone, VKEY_OEM_1},   // U dia., NoSy
    {DomCode::BRACKET_LEFT, 1, 1, 0x00DC, 0x0141, VKEY_OEM_3}};  // U dia., L-

// U+0103 a breve
const PrintableSubEntry kU0103[] = {
    {DomCode::DIGIT1, 0, 0, kAny, kAny, VKEY_1},
    {DomCode::BRACKET_LEFT, 0, 0, kAny, kAny, VKEY_OEM_4}};

// U+0105 a ogonek
const PrintableSubEntry kU0105[] = {
    {DomCode::DIGIT1, 0, 0, kAny, kAny, VKEY_1},
    {DomCode::KEY_Q, 0, 0, kAny, kAny, VKEY_Q},
    {DomCode::QUOTE, 0, 0, kAny, kAny, VKEY_OEM_7}};

// U+010D c caron
const PrintableSubEntry kU010D[] = {
    {DomCode::DIGIT2, 0, 0, kAny, kAny, VKEY_2},
    {DomCode::DIGIT4, 0, 0, kAny, kAny, VKEY_4},
    {DomCode::KEY_P, 0, 0, kAny, kAny, VKEY_X},
    {DomCode::SEMICOLON, 0, 0, kAny, kAny, VKEY_OEM_1},
    {DomCode::COMMA, 0, 0, kAny, kAny, VKEY_OEM_COMMA}};

// U+0111 d stroke
const PrintableSubEntry kU0111[] = {
    {DomCode::DIGIT0, 0, 0, kAny, kAny, VKEY_0},
    {DomCode::BRACKET_RIGHT, 0, 0, kAny, kAny, VKEY_OEM_6}};

// U+0117 e dot above
const PrintableSubEntry kU0117[] = {
    {DomCode::DIGIT4, 0, 0, kAny, kAny, VKEY_4},
    {DomCode::QUOTE, 0, 0, kAny, kAny, VKEY_OEM_7}};

// U+0119 e ogonek
const PrintableSubEntry kU0119[] = {
    {DomCode::DIGIT3, 0, 0, kAny, kAny, VKEY_3},
    {DomCode::SLASH, 1, 1, 0x0118, kNone, VKEY_OEM_MINUS},  // E ogonek, NoSy
    {DomCode::SLASH, 1, 1, 0x0118, 0x006E, VKEY_OEM_2}};     // E ogonek, n

// U+012F i ogonek
const PrintableSubEntry kU012F[] = {
    {DomCode::DIGIT5, 0, 0, kAny, kAny, VKEY_5},
    {DomCode::BRACKET_LEFT, 1, 1, 0x012E, kNone, VKEY_OEM_4}};  // Iogonek, NoSy

// U+0142 l stroke
const PrintableSubEntry kU0142[] = {
    {DomCode::SEMICOLON, 0, 0, kAny, kAny, VKEY_OEM_1},
    {DomCode::BACKSLASH, 0, 0, kAny, kAny, VKEY_OEM_2}};

// U+015F s cedilla
const PrintableSubEntry kU015F[] = {
    {DomCode::SEMICOLON, 0, 0, kAny, kAny, VKEY_OEM_1},
    {DomCode::PERIOD, 0, 0, kAny, kAny, VKEY_OEM_PERIOD}};

// U+0161 s caron
const PrintableSubEntry kU0161[] = {
    {DomCode::DIGIT3, 0, 0, kAny, kAny, VKEY_3},
    {DomCode::DIGIT6, 0, 0, kAny, kAny, VKEY_6},
    {DomCode::BRACKET_LEFT, 0, 0, kAny, kAny, VKEY_OEM_4},
    {DomCode::KEY_A, 0, 0, kAny, kAny, VKEY_OEM_1},
    {DomCode::KEY_F, 0, 0, kAny, kAny, VKEY_F},
    {DomCode::PERIOD, 0, 0, kAny, kAny, VKEY_OEM_PERIOD}};

// U+016B u macron
const PrintableSubEntry kU016B[] = {
    {DomCode::DIGIT8, 0, 0, kAny, kAny, VKEY_8},
    {DomCode::KEY_Q, 0, 0, kAny, kAny, VKEY_Q},
    {DomCode::KEY_X, 0, 0, kAny, kAny, VKEY_X}};

// U+0173 u ogonek
const PrintableSubEntry kU0173[] = {
    {DomCode::DIGIT7, 0, 0, kAny, kAny, VKEY_7},
    {DomCode::SEMICOLON, 1, 1, 0x0172, kNone, VKEY_OEM_3},   // U ogo., NoSy
    {DomCode::SEMICOLON, 1, 1, 0x0172, 0x0162, VKEY_OEM_1}};  // U ogo., T ced.

// U+017C z dot above
const PrintableSubEntry kU017C[] = {
    {DomCode::BRACKET_LEFT, 0, 0, kAny, kAny, VKEY_OEM_4},
    {DomCode::BACKSLASH, 0, 0, kAny, kAny, VKEY_OEM_5}};

// U+017E z caron
const PrintableSubEntry kU017E[] = {
    {DomCode::DIGIT6, 0, 0, kAny, kAny, VKEY_6},
    {DomCode::EQUAL, 0, 0, kAny, kAny, VKEY_OEM_PLUS},
    {DomCode::KEY_W, 0, 0, kAny, kAny, VKEY_W},
    {DomCode::BRACKET_LEFT, 0, 0, kAny, kAny, VKEY_Y},
    {DomCode::BACKSLASH, 0, 0, kAny, kAny, VKEY_OEM_5}};

// Table mapping unshifted characters to PrintableSubEntry tables.
struct PrintableMultiEntry {
  base::char16 plain_character;
  const PrintableSubEntry* subtable;
  size_t subtable_size;
};

// Entries are ordered by character value.
const PrintableMultiEntry kMultiMap[] = {
    {0x0021, kU0021, arraysize(kU0021)},  // exclamation mark
    {0x0022, kU0022, arraysize(kU0022)},  // quotation mark
    {0x0023, kU0023, arraysize(kU0023)},  // number sign
    {0x0024, kU0024, arraysize(kU0024)},  // dollar sign
    {0x0027, kU0027, arraysize(kU0027)},  // apostrophe
    {0x0028, kU0028, arraysize(kU0028)},  // left parenthesis
    {0x0029, kU0029, arraysize(kU0029)},  // right parenthesis
    {0x002A, kU002A, arraysize(kU002A)},  // asterisk
    {0x002B, kU002B, arraysize(kU002B)},  // plus sign
    {0x002C, kU002C, arraysize(kU002C)},  // comma
    {0x002D, kU002D, arraysize(kU002D)},  // hyphen-minus
    {0x002E, kU002E, arraysize(kU002E)},  // full stop
    {0x002F, kU002F, arraysize(kU002F)},  // solidus
    {0x003A, kU003A, arraysize(kU003A)},  // colon
    {0x003B, kU003B, arraysize(kU003B)},  // semicolon
    {0x003D, kU003D, arraysize(kU003D)},  // equals sign
    {0x003F, kU003F, arraysize(kU003F)},  // question mark
    {0x0040, kU0040, arraysize(kU0040)},  // commercial at
    {0x005B, kU005B, arraysize(kU005B)},  // left square bracket
    {0x005C, kU005C, arraysize(kU005C)},  // reverse solidus
    {0x005D, kU005D, arraysize(kU005D)},  // right square bracket
    {0x005F, kU005F, arraysize(kU005F)},  // low line
    {0x0060, kU0060, arraysize(kU0060)},  // grave accent
    {0x00A7, kU00A7, arraysize(kU00A7)},  // section sign
    {0x00AB, kU00AB, arraysize(kU00AB)},  // left double angle quotation mark
    {0x00B0, kU00B0, arraysize(kU00B0)},  // degree sign
    {0x00BA, kU00BA, arraysize(kU00BA)},  // masculine ordinal indicator
    {0x00E0, kU00E0, arraysize(kU00E0)},  // a grave
    {0x00E1, kU00E1, arraysize(kU00E1)},  // a acute
    {0x00E2, kU00E2, arraysize(kU00E2)},  // a circumflex
    {0x00E4, kU00E4, arraysize(kU00E4)},  // a diaeresis
    {0x00E6, kU00E6, arraysize(kU00E6)},  // ae
    {0x00E7, kU00E7, arraysize(kU00E7)},  // c cedilla
    {0x00E8, kU00E8, arraysize(kU00E8)},  // e grave
    {0x00E9, kU00E9, arraysize(kU00E9)},  // e acute
    {0x00ED, kU00ED, arraysize(kU00ED)},  // i acute
    {0x00F0, kU00F0, arraysize(kU00F0)},  // eth
    {0x00F3, kU00F3, arraysize(kU00F3)},  // o acute
    {0x00F4, kU00F4, arraysize(kU00F4)},  // o circumflex
    {0x00F6, kU00F6, arraysize(kU00F6)},  // o diaeresis
    {0x00F8, kU00F8, arraysize(kU00F8)},  // o stroke
    {0x00F9, kU00F9, arraysize(kU00F9)},  // u grave
    {0x00FA, kU00FA, arraysize(kU00FA)},  // u acute
    {0x00FC, kU00FC, arraysize(kU00FC)},  // u diaeresis
    {0x0103, kU0103, arraysize(kU0103)},  // a breve
    {0x0105, kU0105, arraysize(kU0105)},  // a ogonek
    {0x010D, kU010D, arraysize(kU010D)},  // c caron
    {0x0111, kU0111, arraysize(kU0111)},  // d stroke
    {0x0117, kU0117, arraysize(kU0117)},  // e dot above
    {0x0119, kU0119, arraysize(kU0119)},  // e ogonek
    {0x012F, kU012F, arraysize(kU012F)},  // i ogonek
    {0x0142, kU0142, arraysize(kU0142)},  // l stroke
    {0x015F, kU015F, arraysize(kU015F)},  // s cedilla
    {0x0161, kU0161, arraysize(kU0161)},  // s caron
    {0x016B, kU016B, arraysize(kU016B)},  // u macron
    {0x0173, kU0173, arraysize(kU0173)},  // u ogonek
    {0x017C, kU017C, arraysize(kU017C)},  // z dot above
    {0x017E, kU017E, arraysize(kU017E)},  // z caron
};

// Table mapping unshifted characters to VKEY values.
struct PrintableSimpleEntry {
  base::char16 plain_character;
  KeyboardCode key_code;
};

// Entries are ordered by character value.
const PrintableSimpleEntry kSimpleMap[] = {
    {0x0025, VKEY_5},          // percent sign
    {0x0026, VKEY_1},          // ampersand
    {0x003C, VKEY_OEM_5},      // less-than sign
    {0x007B, VKEY_OEM_7},      // left curly bracket
    {0x007C, VKEY_OEM_5},      // vertical line
    {0x007D, VKEY_OEM_2},      // right curly bracket
    {0x007E, VKEY_OEM_5},      // tilde
    {0x00A1, VKEY_OEM_6},      // inverted exclamation mark
    {0x00AD, VKEY_OEM_3},      // soft hyphen
    {0x00B2, VKEY_OEM_7},      // superscript two
    {0x00B5, VKEY_OEM_5},      // micro sign
    {0x00BB, VKEY_9},          // right-pointing double angle quotation mark
    {0x00BD, VKEY_OEM_5},      // vulgar fraction one half
    {0x00BF, VKEY_OEM_6},      // inverted question mark
    {0x00DF, VKEY_OEM_4},      // sharp s
    {0x00E5, VKEY_OEM_6},      // a ring above
    {0x00EA, VKEY_3},          // e circumflex
    {0x00EB, VKEY_OEM_1},      // e diaeresis
    {0x00EC, VKEY_OEM_6},      // i grave
    {0x00EE, VKEY_OEM_6},      // i circumflex
    {0x00F1, VKEY_OEM_3},      // n tilde
    {0x00F2, VKEY_OEM_3},      // o grave
    {0x00F5, VKEY_OEM_4},      // o tilde
    {0x00F7, VKEY_OEM_6},      // division sign
    {0x00FD, VKEY_7},          // y acute
    {0x00FE, VKEY_OEM_MINUS},  // thorn
    {0x0101, VKEY_OEM_8},      // a macron
    {0x0107, VKEY_OEM_7},      // c acute
    {0x010B, VKEY_OEM_3},      // c dot above
    {0x0113, VKEY_W},          // e macron
    {0x011B, VKEY_2},          // e caron
    {0x011F, VKEY_OEM_6},      // g breve
    {0x0121, VKEY_OEM_4},      // g dot above
    {0x0127, VKEY_OEM_6},      // h stroke
    {0x012B, VKEY_OEM_6},      // i macron
    {0x0131, VKEY_OEM_1},      // dotless i
    {0x0137, VKEY_OEM_5},      // k cedilla
    {0x013C, VKEY_OEM_2},      // l cedilla
    {0x013E, VKEY_2},          // l caron
    {0x0146, VKEY_OEM_4},      // n cedilla
    {0x0148, VKEY_OEM_5},      // n caron
    {0x0151, VKEY_OEM_4},      // o double acute
    {0x0159, VKEY_5},          // r caron
    {0x0163, VKEY_OEM_7},      // t cedilla
    {0x0165, VKEY_5},          // t caron
    {0x016F, VKEY_OEM_1},      // u ring above
    {0x0171, VKEY_OEM_5},      // u double acute
    {0x01A1, VKEY_OEM_6},      // o horn
    {0x01B0, VKEY_OEM_4},      // u horn
    {0x01B6, VKEY_OEM_6},      // z stroke
    {0x0259, VKEY_OEM_3},      // schwa
};

}  // anonymous namespace

XkbKeyCodeConverter::XkbKeyCodeConverter() {
}

XkbKeyCodeConverter::~XkbKeyCodeConverter() {
}

XkbKeyboardLayoutEngine::XkbKeyboardLayoutEngine(
    const XkbKeyCodeConverter& converter)
    : key_code_converter_(converter) {
  // TODO: add XKB_CONTEXT_NO_ENVIRONMENT_NAMES
  xkb_context_.reset(xkb_context_new(XKB_CONTEXT_NO_DEFAULT_INCLUDES));

  // Temporary initial keymap to avoid file I/O on initialization.
  // TODO: remove this after SetCurrentLayoutByName() is implemented.
  xkb_keymap* keymap = xkb_keymap_new_from_string(
      xkb_context_.get(), temporary_initial_keymap, XKB_KEYMAP_FORMAT_TEXT_V1,
      XKB_KEYMAP_COMPILE_NO_FLAGS);
  SetKeymap(keymap);
}

XkbKeyboardLayoutEngine::~XkbKeyboardLayoutEngine() {
}

bool XkbKeyboardLayoutEngine::CanSetCurrentLayout() const {
#if defined(OS_CHROMEOS)
  return true;
#else
  return false;
#endif
}

bool XkbKeyboardLayoutEngine::SetCurrentLayoutByName(
    const std::string& layout_name) {
#if defined(OS_CHROMEOS)
  // TODO(fengyuan)
  // End successful set with SetKeymap(keymap) to update other fields.
  return false;
#else
  return false;
#endif
}

bool XkbKeyboardLayoutEngine::UsesISOLevel5Shift() const {
  // NOTIMPLEMENTED();
  return false;
}

bool XkbKeyboardLayoutEngine::UsesAltGr() const {
  // NOTIMPLEMENTED();
  return false;
}

bool XkbKeyboardLayoutEngine::Lookup(DomCode dom_code,
                                     int flags,
                                     DomKey* dom_key,
                                     base::char16* character,
                                     KeyboardCode* key_code) const {
  // Convert DOM physical key to XKB representation.
  xkb_keycode_t xkb_keycode = key_code_converter_.DomCodeToXkbKeyCode(dom_code);
  if (xkb_keycode == key_code_converter_.InvalidXkbKeyCode()) {
    LOG(ERROR) << "No XKB keycode for DomCode 0x" << std::hex
               << static_cast<int>(dom_code) << " '"
               << KeycodeConverter::DomCodeToCodeString(dom_code) << "'";
    return false;
  }
  xkb_mod_mask_t xkb_flags = EventFlagsToXkbFlags(flags);
  // Obtain keysym and character.
  xkb_keysym_t xkb_keysym;
  if (!XkbLookup(xkb_keycode, xkb_flags, &xkb_keysym, character))
    return false;
  // Classify the keysym and convert to DOM and VKEY representations.
  *dom_key = NonPrintableXkbKeySymToDomKey(xkb_keysym);
  if (*dom_key == DomKey::NONE) {
    *dom_key = CharacterToDomKey(*character);
    *key_code = AlphanumericKeyboardCode(*character);
    if (*key_code == VKEY_UNKNOWN) {
      *key_code = DifficultKeyboardCode(dom_code, flags, xkb_keycode, xkb_flags,
                                        xkb_keysym, *dom_key, *character);
      if (*key_code == VKEY_UNKNOWN)
        *key_code = DomCodeToNonLocatedKeyboardCode(dom_code);
    }
  } else if (*dom_key == DomKey::DEAD) {
    *character = DeadXkbKeySymToCombiningCharacter(xkb_keysym);
    *key_code = DomCodeToNonLocatedKeyboardCode(dom_code);
  } else {
    *key_code = NonPrintableDomKeyToKeyboardCode(*dom_key);
    if (*key_code == VKEY_UNKNOWN)
      *key_code = DomCodeToNonLocatedKeyboardCode(dom_code);
  }
  return true;
}

void XkbKeyboardLayoutEngine::SetKeymap(xkb_keymap* keymap) {
  xkb_state_.reset(xkb_state_new(keymap));
  // Update flag map.
  static const struct {
    int ui_flag;
    const char* xkb_name;
  } flags[] = {{ui::EF_CONTROL_DOWN, XKB_MOD_NAME_CTRL},
               {ui::EF_SHIFT_DOWN, XKB_MOD_NAME_SHIFT},
               {ui::EF_ALT_DOWN, XKB_MOD_NAME_ALT},
               {ui::EF_CAPS_LOCK_DOWN, XKB_MOD_NAME_CAPS},
               {ui::EF_COMMAND_DOWN, XKB_MOD_NAME_LOGO},
               {ui::EF_NUMPAD_KEY, XKB_MOD_NAME_NUM},
               {ui::EF_MOD3_DOWN, "Mod3"},
               {ui::EF_ALTGR_DOWN, "Mod5"}};
  xkb_flag_map_.clear();
  xkb_flag_map_.resize(arraysize(flags));
  for (size_t i = 0; i < arraysize(flags); ++i) {
    xkb_mod_index_t index = xkb_keymap_mod_get_index(keymap, flags[i].xkb_name);
    if (index == XKB_MOD_INVALID) {
      DVLOG(3) << "XKB keyboard layout does not contain " << flags[i].xkb_name;
    } else {
      xkb_mod_mask_t flag = static_cast<xkb_mod_mask_t>(1) << index;
      XkbFlagMapEntry e = {flags[i].ui_flag, flag};
      xkb_flag_map_.push_back(e);
    }
  }
}

xkb_mod_mask_t XkbKeyboardLayoutEngine::EventFlagsToXkbFlags(
    int ui_flags) const {
  xkb_mod_mask_t xkb_flags = 0;
  for (const auto& entry : xkb_flag_map_) {
    if (ui_flags & entry.ui_flag)
      xkb_flags |= entry.xkb_flag;
  }
  return xkb_flags;
}

bool XkbKeyboardLayoutEngine::XkbLookup(xkb_keycode_t xkb_keycode,
                                        xkb_mod_mask_t xkb_flags,
                                        xkb_keysym_t* xkb_keysym,
                                        base::char16* character) const {
  if (!xkb_state_) {
    LOG(ERROR) << "No current XKB state";
    return false;
  }
  xkb_state_update_mask(xkb_state_.get(), xkb_flags, 0, 0, 0, 0, 0);
  *xkb_keysym = xkb_state_key_get_one_sym(xkb_state_.get(), xkb_keycode);
  if (*xkb_keysym == XKB_KEY_NoSymbol)
    return false;
  uint32_t c = xkb_state_key_get_utf32(xkb_state_.get(), xkb_keycode);
  DLOG_IF(ERROR, c != (c & 0xFFFF)) << "Non-BMP character:" << c;
  *character = static_cast<base::char16>(c);
  return true;
}

KeyboardCode XkbKeyboardLayoutEngine::DifficultKeyboardCode(
    DomCode dom_code,
    int ui_flags,
    xkb_keycode_t xkb_keycode,
    xkb_mod_mask_t xkb_flags,
    xkb_keysym_t xkb_keysym,
    DomKey dom_key,
    base::char16 character) const {
  // Get the layout interpretation without modifiers, so that
  // e.g. Ctrl+D correctly generates VKEY_D.
  xkb_keysym_t plain_keysym;
  base::char16 plain_character;
  if (!XkbLookup(xkb_keycode, 0, &plain_keysym, &plain_character))
    return VKEY_UNKNOWN;

  // If the plain key is non-printable, that determines the VKEY.
  DomKey plain_key = NonPrintableXkbKeySymToDomKey(plain_keysym);
  if (plain_key != ui::DomKey::NONE)
    return NonPrintableDomKeyToKeyboardCode(dom_key);

  // Plain ASCII letters and digits map directly to VKEY values.
  KeyboardCode key_code = AlphanumericKeyboardCode(plain_character);
  if (key_code != VKEY_UNKNOWN)
    return key_code;

  // Check the multi-character tables.
  const PrintableMultiEntry* multi_end = kMultiMap + arraysize(kMultiMap);
  const PrintableMultiEntry* multi =
      std::lower_bound(kMultiMap, multi_end, plain_character,
                       [](const PrintableMultiEntry& e, base::char16 c) {
        return e.plain_character < c;
      });
  if ((multi != multi_end) && (multi->plain_character == plain_character)) {
    const base::char16 kNonCharacter = kAny;
    base::char16 shift_character = kNonCharacter;
    base::char16 altgr_character = kNonCharacter;
    for (size_t i = 0; i < multi->subtable_size; ++i) {
      if (multi->subtable[i].dom_code != dom_code)
        continue;
      if (multi->subtable[i].test_shift) {
        if (shift_character == kNonCharacter) {
          shift_character = XkbSubCharacter(xkb_keycode, xkb_flags, character,
                                            ui::EF_SHIFT_DOWN);
        }
        if (shift_character != multi->subtable[i].shift_character)
          continue;
      }
      if (multi->subtable[i].test_altgr) {
        if (altgr_character == kNonCharacter) {
          altgr_character = XkbSubCharacter(xkb_keycode, xkb_flags, character,
                                            ui::EF_ALTGR_DOWN);
        }
        if (altgr_character != multi->subtable[i].altgr_character)
          continue;
      }
      return multi->subtable[i].key_code;
    }
  }

  // Check the simple character table.
  const PrintableSimpleEntry* simple_end = kSimpleMap + arraysize(kSimpleMap);
  const PrintableSimpleEntry* simple =
      std::lower_bound(kSimpleMap, simple_end, plain_character,
                       [](const PrintableSimpleEntry& e, base::char16 c) {
        return e.plain_character < c;
      });
  if ((simple != simple_end) && (simple->plain_character == plain_character))
    return simple->key_code;

  return VKEY_UNKNOWN;
}

base::char16 XkbKeyboardLayoutEngine::XkbSubCharacter(
    xkb_keycode_t xkb_keycode,
    xkb_mod_mask_t base_flags,
    base::char16 base_character,
    int ui_flags) const {
  xkb_mod_mask_t flags = EventFlagsToXkbFlags(ui_flags);
  if (flags == base_flags)
    return base_character;
  xkb_keysym_t keysym;
  base::char16 character = 0;
  if (!XkbLookup(xkb_keycode, flags, &keysym, &character))
    character = kNone;
  return character;
}

}  // namespace ui
