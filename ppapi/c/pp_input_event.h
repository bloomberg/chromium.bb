/* Copyright (c) 2011 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef PPAPI_C_PP_INPUT_EVENT_H_
#define PPAPI_C_PP_INPUT_EVENT_H_

/**
 * @file
 * This file defines the API used to handle mouse and keyboard input events.
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

/**
 * This enumeration contains constants representing each mouse button.
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
 * @addtogroup Enums
 * @{
 */

/**
 * This enumeration contains mouse and keyboard event constants.
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
 * @addtogroup Enums
 * @{
 */

/**
 * This enumeration contains event modifier constants.
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
 * The PP_InputEvent_Key struct represents a key up or key down event.
 *
 * Key up and key down events correspond to physical keys on the keyboard. The
 * actual character that the user typed (if any) will be delivered in a
 * "character" event.
 *
 * If the user loses focus on the module while a key is down, a key up
 * event might not occur. For example, if the module has focus and the user
 * presses and holds the shift key, the module will see a "shift down" message.
 * Then if the user clicks elsewhere on the web page, the module's focus will
 * be lost and no more input events will be delivered.
 *
 * If your module depends on receiving key up events, it should also handle
 * "lost focus" as the equivalent of "all keys up."
 */
struct PP_InputEvent_Key {
  /** This value is a bit field combination of the EVENT_MODIFIER flags. */
  uint32_t modifier;

  /**
   * The key code.
   */

// TODO(brettw) define what these actually are.
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
 * The PP_InputEvent_Character struct represents a typed character event.
 *
 * Normally, the program will receive a key down event, followed by a character
 * event, followed by a key up event. The character event will have any
 * modifier keys applied. Obvious examples are symbols, where Shift-5 gives you
 * a '%'. The key down and up events will give you the scan code for the "5"
 * key, and the character event will give you the '%' character.
 *
 * You may not get a character event for all key down events if the key doesn't
 * generate a character. Likewise, you may actually get multiple character
 * events in a row. For example, some locales have an accent key that modifies
 * the next character typed. You might get this stream of events: accent down,
 * accent up (it didn't generate a character), letter key down, letter with
 * accent character event (it was modified by the previous accent key), letter
 * key up.  If the letter can't be combined with the accent, like an umlaut and
 * an 'R', the system might send unlaut down, umlaut up, 'R' key down, umlaut
 * character (can't combine it with 'R', so just send the raw unlaut so it
 * isn't lost"), 'R' character event, 'R' key up.
 */
struct PP_InputEvent_Character {
  /** A combination of the EVENT_MODIFIER flags. */
  uint32_t modifier;

  /**
   * This value represents the typed character as a single null-terminated UTF-8
   * character. Any unused bytes will be filled with null bytes. Since the
   * maximum UTF-8 character is 4 bytes, there will always be at least one null
   * at the end so you can treat this as a null-termianted UTF-8 string.
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

/**
 * The PP_InputEvent_Mouse struct represents all mouse events except
 * mouse wheel events.
 */
struct PP_InputEvent_Mouse {
  /** This value is a bit field combination of the EVENT_MODIFIER flags. */
  uint32_t modifier;

  /**
   * This value represents the button that changed for mouse down or up events.
   * This value will be PP_EVENT_MOUSEBUTTON_NONE for mouse move, enter, and
   * leave events.
   */
  PP_InputEvent_MouseButton button;

  /**
   * This values represents the x coordinate of the mouse when the event
   * occurred.
   *
   * In most, but not all, cases these coordinates will just be integers.
   * For example, the plugin element might be arbitrarily scaled or transformed
   * in the DOM, and translating a mouse event into the coordinate space of the
   * plugin will give non-integer values.
   */
  float x;
  /**
   * This values represents the y coordinate of the mouse when the event
   * occurred.
   *
   * In most, but not all, cases these coordinates will just be integers.
   * For example, the plugin element might be arbitrarily scaled or transformed
   * in the DOM, and translating a mouse event into the coordinate space of the
   * plugin will give non-integer values.
   */
  float y;

  // TODO(brettw) figure out exactly what this means.
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

/**
 * The PP_InputEvent_Wheel struct represents all mouse wheel events.
 */
struct PP_InputEvent_Wheel {
  /** This value represents a combination of the EVENT_MODIFIER flags. */
  uint32_t modifier;

  /**
   * Indicates the amount vertically and horizontally the user has requested
   * to scroll by with their mouse wheel. A scroll down or to the right (where
   * the content moves up or left) is represented as positive values, and
   * a scroll up or to the left (where the content moves down or right) is
   * represented as negative values.
   *
   * The units are either in pixels (when scroll_by_page is false) or pages
   * (when scroll_by_page is true). For example, delta_y = -3 means scroll up 3
   * pixels when scroll_by_page is false, and scroll up 3 pages when
   * scroll_by_page is true.
   *
   * This amount is system dependent and will take into account the user's
   * preferred scroll sensitivity and potentially also nonlinear acceleration
   * based on the speed of the scrolling.
   *
   * Devices will be of varying resolution. Some mice with large detents will
   * only generate integer scroll amounts. But fractional values are also
   * possible, for example, on some trackpads and newer mice that don't have
   * "clicks".
   */
  float delta_x;

  /** This value represents */
  float delta_y;

  /**
   * The number of "clicks" of the scroll wheel that have produced the
   * event. The value may have system-specific acceleration applied to it,
   * depending on the device. The positive and negative meanings are the same
   * as for |delta|.
   *
   * If you are scrolling, you probably want to use the delta values above.
   * These tick events can be useful if you aren't doing actual scrolling and
   * don't want or pixel values. An example may be cycling between different
   * items in a game.
   *
   * You may receive fractional values for the wheel ticks if the mouse wheel
   * is high resolution or doesn't have "clicks". If your program wants
   * discrete events (as in the "picking items" example) you should accumulate
   * fractional click values from multiple messages until the total value
   * reaches positive or negative one. This should represent a similar amount
   * of scrolling as for a mouse that has a discrete mouse wheel.
   */
  float wheel_ticks_x;

  /** This value represents */
  float wheel_ticks_y;

  /**
   * Indicates if the scroll delta_x/delta_y indicates pages or lines to
   * scroll by. When true, the user is requesting to scroll by pages.
   */
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

/**
 * The PP_InputEvent struct represents all input events.
 */
struct PP_InputEvent {
  /** This value represents the type of the event. */
  PP_InputEvent_Type type;

  /** This value ensure the time_stamp is aligned on an 8-byte boundary
   * relative to the start of the struct. Some compilers align doubles
   * on 8-byte boundaries for 32-bit x86, and some align on 4-byte boundaries.
   */
  int32_t padding;

  /**
   * This value represents the time that this event was generated. This value
   * is not relative to any particular epoch; the most you can do is compare
   * time stamps.
   */
  PP_TimeTicks time_stamp;

  /** This value represents the event type and its specific data. */
  union {
    struct PP_InputEvent_Key key;
    struct PP_InputEvent_Character character;
    struct PP_InputEvent_Mouse mouse;
    struct PP_InputEvent_Wheel wheel;

    /**
     * This value allows new events to be added without changing the size of
     * this struct.
     */
    char padding[64];
  } u;
};
PP_COMPILE_ASSERT_STRUCT_SIZE_IN_BYTES(PP_InputEvent, 80);
/**
 * @}
 */

#endif  /* PPAPI_C_PP_INPUT_EVENT_H_ */
