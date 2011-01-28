/* Copyright (c) 2011 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef PPAPI_C_PP_INPUT_EVENT_H_
#define PPAPI_C_PP_INPUT_EVENT_H_

/**
 * @file
 * Defines the API ...
 */

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/pp_time.h"

/**
 *
 * @addtogroup Enums
 * @{
 */
typedef enum {
  PP_INPUTEVENT_MOUSEBUTTON_NONE   = -1,
  PP_INPUTEVENT_MOUSEBUTTON_LEFT   = 0,
  PP_INPUTEVENT_MOUSEBUTTON_MIDDLE = 1,
  PP_INPUTEVENT_MOUSEBUTTON_RIGHT  = 2
} PP_InputEvent_MouseButton;
PP_COMPILE_ASSERT_SIZE_IN_BYTES(PP_InputEvent_MouseButton, 4);
/**
 * @}
 */

/**
 *
 * @addtogroup Enums
 * @{
 */
typedef enum {
  PP_INPUTEVENT_TYPE_UNDEFINED   = -1,
  PP_INPUTEVENT_TYPE_MOUSEDOWN   = 0,  // PP_InputEvent_Mouse
  PP_INPUTEVENT_TYPE_MOUSEUP     = 1,  // PP_InputEvent_Mouse
  PP_INPUTEVENT_TYPE_MOUSEMOVE   = 2,  // PP_InputEvent_Mouse
  PP_INPUTEVENT_TYPE_MOUSEENTER  = 3,  // PP_InputEvent_Mouse
  PP_INPUTEVENT_TYPE_MOUSELEAVE  = 4,  // PP_InputEvent_Mouse
  PP_INPUTEVENT_TYPE_MOUSEWHEEL  = 5,  // PP_InputEvent_Wheel
  PP_INPUTEVENT_TYPE_RAWKEYDOWN  = 6,  // PP_InputEvent_Key
  PP_INPUTEVENT_TYPE_KEYDOWN     = 7,  // PP_InputEvent_Key
  PP_INPUTEVENT_TYPE_KEYUP       = 8,  // PP_InputEvent_Key
  PP_INPUTEVENT_TYPE_CHAR        = 9,  // PP_InputEvent_Character
  PP_INPUTEVENT_TYPE_CONTEXTMENU = 10  // PP_InputEvent_Mouse
} PP_InputEvent_Type;
PP_COMPILE_ASSERT_SIZE_IN_BYTES(PP_InputEvent_Type, 4);
/**
 * @}
 */

/**
 *
 * @addtogroup Enums
 * @{
 */
typedef enum {
  PP_INPUTEVENT_MODIFIER_SHIFTKEY         = 1 << 0,
  PP_INPUTEVENT_MODIFIER_CONTROLKEY       = 1 << 1,
  PP_INPUTEVENT_MODIFIER_ALTKEY           = 1 << 2,
  PP_INPUTEVENT_MODIFIER_METAKEY          = 1 << 3,
  PP_INPUTEVENT_MODIFIER_ISKEYPAD         = 1 << 4,
  PP_INPUTEVENT_MODIFIER_ISAUTOREPEAT     = 1 << 5,
  PP_INPUTEVENT_MODIFIER_LEFTBUTTONDOWN   = 1 << 6,
  PP_INPUTEVENT_MODIFIER_MIDDLEBUTTONDOWN = 1 << 7,
  PP_INPUTEVENT_MODIFIER_RIGHTBUTTONDOWN  = 1 << 8,
  PP_INPUTEVENT_MODIFIER_CAPSLOCKKEY      = 1 << 9,
  PP_INPUTEVENT_MODIFIER_NUMLOCKKEY       = 1 << 10
} PP_InputEvent_Modifier;
PP_COMPILE_ASSERT_SIZE_IN_BYTES(PP_InputEvent_Modifier, 4);
/**
 * @}
 */

/**
 * @addtogroup Structs
 * @{
 */

/**
 * An event representing a key up or down event.
 *
 * Key up and down events correspond to physical keys on the keyboard. The
 * actual character that the user typed (if any) will be delivered in a
 * "character" event.
 *
 * If the user kills focus on the plugin while a key is down, you may not get
 * a key up event. For example, if the plugin has focus and the user presses
 * and holds shift, the plugin will see a "shift down" message. Then if they
 * click elsewhere on the web page, the plugin focus will be lost and no more
 * input events will be delivered. If you depend on getting key up events, you
 * will also want to handle "lost focus" as the equivalent of "all keys up."
 *
 */
struct PP_InputEvent_Key {
  /** A combination of the EVENT_MODIFIER flags. */
  uint32_t modifier;

  /**
   * The key code.
   *
   * TODO(brettw) define what these actually are.
   */
  uint32_t key_code;
};
PP_COMPILE_ASSERT_STRUCT_SIZE_IN_BYTES(PP_InputEvent_Key, 8);
/**
 * @}
 */

/**
 * @addtogroup Structs
 * @{
 */

/**
 * An event representing a typed character.
 *
 * Normally, the program will receive a key down event, followed by a character
 * event, followed by a key up event. The character event will have any
 * modifier keys applied. Obvious examples are symbols, where Shift-5 gives you
 * a '%'. The key down and up events will give you the scan code for the "5"
 * key, and the character event will give you the '%' character.
 *
 * You may not get a character event for all key down if the key doesn't
 * generate a character. Likewise, you may actually get multiple character
 * events in a row. For example, some locales have an accent key that modifies
 * the next character typed. You might get this stream of events: accent down,
 * accent up (it didn't generate a character), letter key down, letter with
 * accent character event (it was modified by the previous accent key), letter
 * key up.  If the letter can't be combined with the accent, like an umlaut and
 * an 'R', the system might send unlaut down, umlaut up, 'R' key down, umlaut
 * character ("whoops, I can't combine it with 'R', I better just send the raw
 * unlaut so it isn't lost"), 'R' character event, 'R' key up.
 *
 */
struct PP_InputEvent_Character {
  /** A combination of the EVENT_MODIFIER flags. */
  uint32_t modifier;

  /**
   * The character the user typed, as a single null-terminated UTF-8 character.
   * Any unused bytes will be filled with null bytes. Since the maximum UTF-8
   * character is 4 bytes, there will always be at least one null at the end
   * so you can treat this as a null-termianted UTF-8 string.
   */
  char text[5];
};
PP_COMPILE_ASSERT_STRUCT_SIZE_IN_BYTES(PP_InputEvent_Character, 12);
/**
 * @}
 */

/**
 * @addtogroup Structs
 * @{
 */

/** Represents a mouse event for everything other than the mouse wheel. */
struct PP_InputEvent_Mouse {
  /** A combination of the EVENT_MODIFIER flags. */
  uint32_t modifier;

  /**
   * Which button changed in the case of mouse down or up events. For mouse
   * move, enter, and leave events, this will be PP_EVENT_MOUSEBUTTON_NONE.
   */
  PP_InputEvent_MouseButton button;

  /**
   * The coordinates of the mouse when the event occurred.
   *
   * In most cases these coordinates will just be integers, but they may not
   * be in some cases. For example, the plugin element might be arbitrarily
   * scaled or transformed in the DOM, and translating a mouse event into the
   * coordinate space of the plugin will give non-integer values.
   */
  float x;
  float y;

  /** TODO(brettw) figure out exactly what this means. */
  int32_t click_count;
};
PP_COMPILE_ASSERT_STRUCT_SIZE_IN_BYTES(PP_InputEvent_Mouse, 20);
/**
 * @}
 */

/**
 * @addtogroup Structs
 * @{
 */
struct PP_InputEvent_Wheel {
  /** A combination of the EVENT_MODIFIER flags. */
  uint32_t modifier;

  float delta_x;
  float delta_y;
  float wheel_ticks_x;
  float wheel_ticks_y;

  PP_Bool scroll_by_page;
};
PP_COMPILE_ASSERT_STRUCT_SIZE_IN_BYTES(PP_InputEvent_Wheel, 24);
/**
 * @}
 */

/**
 *
 * @addtogroup Structs
 * @{
 */
struct PP_InputEvent {
  /** Identifies the type of the event. */
  PP_InputEvent_Type type;

  /* Ensure the time_stamp is aligned on an 8-byte boundary relative to the
     start of the struct. Some compilers align doubles on 8-byte boundaries
     for 32-bit x86, and some align on 4-byte boundaries. */
  int32_t padding;

  /**
   * When this event was generated. This is not relative to any particular
   * epoch, the most you can do is compare time stamps.
   */
  PP_TimeTicks time_stamp;

  /** Event-specific data. */
  union {
    struct PP_InputEvent_Key key;
    struct PP_InputEvent_Character character;
    struct PP_InputEvent_Mouse mouse;
    struct PP_InputEvent_Wheel wheel;

    /**
     * Allows new events to be added without changing the size of this
     * struct.
     */
    char padding[64];
  } u;
};
PP_COMPILE_ASSERT_STRUCT_SIZE_IN_BYTES(PP_InputEvent, 80);
/**
 * @}
 */

#endif  /* PPAPI_C_PP_INPUT_EVENT_H_ */
