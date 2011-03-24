/*
 * Copyright 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

// This file provives a utilty function to translate and SDL event structure
// into a PPAPI event structure.
// This is very adhoc and especially the keyboard event part is very
// incomplete.

#include <stdint.h>
#include <SDL/SDL_keysym.h>
#include <SDL/SDL_events.h>
#include "ppapi/c/pp_input_event.h"

#include "native_client/src/trusted/sel_universal/multimedia.h"

const int PP_INPUTEVENT_USER = 88;
const int PP_INPUTEVENT_TERMINATION = 90;

// ppapi does not have a user defined event notion.
// c.f. ppapi/c/pp_input_event.h
typedef struct {
  int code;
  int data1;
  int data2;
}  PP_InputEvent_User;

static PP_InputEvent_User* GetUserEvent(PP_InputEvent* event) {
  return reinterpret_cast<PP_InputEvent_User*>(&event->u);
}

void MakeInvalidEvent(PP_InputEvent* event) {
  event->type = PP_INPUTEVENT_TYPE_UNDEFINED;
}


bool IsInvalidEvent(PP_InputEvent* event) {
  return event->type == PP_INPUTEVENT_USER;
}


bool IsTerminationEvent(PP_InputEvent* event) {
  return event->type == PP_INPUTEVENT_TERMINATION;
}


bool IsUserEvent(PP_InputEvent* event) {
  return event->type == PP_INPUTEVENT_USER;
}


int GetCodeFromUserEvent(PP_InputEvent* event) {
  return GetUserEvent(event)->code;
}

int GetData1FromUserEvent(PP_InputEvent* event) {
  return  GetUserEvent(event)->data1;
}


int GetData2FromUserEvent(PP_InputEvent* event) {
  return  GetUserEvent(event)->data2;
}

void MakeUserEvent(PP_InputEvent* event, int code, int data1, int data2) {
  event->type = (PP_InputEvent_Type) PP_INPUTEVENT_USER;
  PP_InputEvent_User* user = GetUserEvent(event);
  user->code = code;
  user->data1 = data1;
  user->data2 = data2;
}


static PP_InputEvent_MouseButton SDLButtonToPPButton(uint32_t button) {
  switch (button) {
    case SDL_BUTTON_LEFT:
      return PP_INPUTEVENT_MOUSEBUTTON_LEFT;
    case SDL_BUTTON_MIDDLE:
      return PP_INPUTEVENT_MOUSEBUTTON_MIDDLE;
    case SDL_BUTTON_RIGHT:
      return PP_INPUTEVENT_MOUSEBUTTON_RIGHT;
    default:
      return PP_INPUTEVENT_MOUSEBUTTON_NONE;
  }
}

// These are reverse engineered from:
// https://github.com/eugenis/sdl-nacl/blob/master/src/video/nacl/SDL_naclevents.cc
// There are likely many mistakes here.
// Ideally we would reuse a ppapi enum but that does not seem to exist.
const uint32_t PP_K_F1 = 112;
const uint32_t PP_K_KP0 = 96;
const uint32_t PP_K_LSHIFT = 16;
const uint32_t PP_K_LCTRL = 17;
const uint32_t PP_K_LALT = 18;

const uint32_t PP_K_PAGEUP = 33;
const uint32_t PP_K_PAGEDOWN = 34;
const uint32_t PP_K_END = 35;
const uint32_t PP_K_HOME = 36;
const uint32_t PP_K_LEFT = 37;
const uint32_t PP_K_UP = 38;
const uint32_t PP_K_RIGHT = 39;
const uint32_t PP_K_DOWN = 40;

const uint32_t PP_K_KP_MULTIPLY = 106;
const uint32_t PP_K_KP_PLUS = 107;
const uint32_t PP_K_KP_MINUS = 109;
const uint32_t PP_K_KP_PERIOD = 110;
const uint32_t PP_K_KP_DIVIDE = 111;

const uint32_t PP_K_INSERT = 45;
const uint32_t PP_K_DELETE = 46;

const uint32_t PP_K_MINUS = 189;
const uint32_t PP_K_EQUALS = 187;
const uint32_t PP_K_LEFTBRACKET = 219;
const uint32_t PP_K_RIGHTBRACKET = 221;
const uint32_t PP_K_SEMICOLON = 186;
const uint32_t PP_K_QUOTE = 222;
const uint32_t PP_K_BACKSLASH = 220;
const uint32_t PP_K_COMMA = 188;
const uint32_t PP_K_PERIOD = 190;
const uint32_t PP_K_SLASH = 191;
const uint32_t PP_K_BACKQUOTE = 192;


static uint32_t SDLKeyToPPKey(uint32_t key) {
  // pp favor upper case
  if (SDLK_a <= key && key <= SDLK_z) {
    return key - SDLK_a + 'A';
  }

  // a bunch of key code are the same
  if ((SDLK_0 <= key && key <= SDLK_9) ||
      SDLK_BACKSPACE == key ||
      SDLK_TAB == key ||
      SDLK_PAUSE == key ||
      SDLK_ESCAPE == key) {
    return key;
  }

  if (SDLK_F1 <= key && key <= SDLK_F12) {
    return key - SDLK_F1 + PP_K_F1;
  }

  if (SDLK_KP0 <= key && key <=  SDLK_KP9) {
    return key - SDLK_KP0 + PP_K_KP0;
  }

  switch (key) {
    case SDLK_LSHIFT: return PP_K_LSHIFT;
    case SDLK_LCTRL:  return PP_K_LCTRL;
    case SDLK_LALT:  return PP_K_LALT;
    case SDLK_LEFT:  return PP_K_LEFT;
    case SDLK_UP:  return PP_K_LEFT;
    case SDLK_RIGHT:  return PP_K_RIGHT;
    case SDLK_DOWN:  return PP_K_DOWN;
    case SDLK_KP_MULTIPLY:  return PP_K_KP_MULTIPLY;
    case SDLK_KP_PLUS:  return PP_K_KP_PLUS;
    case SDLK_KP_MINUS:  return PP_K_KP_MINUS;
    case SDLK_KP_PERIOD:  return PP_K_KP_PERIOD;
    case SDLK_KP_DIVIDE:  return PP_K_KP_DIVIDE;
    case SDLK_INSERT:  return PP_K_INSERT;
    case SDLK_DELETE:  return PP_K_DELETE;
    case SDLK_HOME:  return PP_K_HOME;
    case SDLK_END:  return PP_K_END;
    case SDLK_PAGEUP:  return PP_K_PAGEUP;
    case SDLK_PAGEDOWN:  return PP_K_PAGEDOWN;
    case SDLK_MINUS:  return PP_K_MINUS;
    case SDLK_EQUALS:  return PP_K_EQUALS;
    case SDLK_LEFTBRACKET:  return PP_K_LEFTBRACKET;
    case SDLK_RIGHTBRACKET:  return PP_K_RIGHTBRACKET;
    case SDLK_SEMICOLON:  return PP_K_SEMICOLON;
    case SDLK_QUOTE:  return PP_K_QUOTE;
    case SDLK_BACKSLASH:  return PP_K_BACKSLASH;
    case SDLK_COMMA:  return PP_K_COMMA;
    case SDLK_PERIOD:  return PP_K_PERIOD;
    case SDLK_SLASH:  return PP_K_SLASH;
    case SDLK_BACKQUOTE:  return PP_K_BACKQUOTE;
    default:
      return 0;
  }
}


static int ptr2int(void* p) {
  return static_cast<int>(reinterpret_cast<uintptr_t>(p));
}

// Convert the SDL event, sdl_event, into the PPAPI event, pp_event.
// Returns false if the event is not supported or cannot be processed.
bool ConvertSDLEventToPPAPI(
  const SDL_Event& sdl_event, PP_InputEvent* pp_event) {
  switch (sdl_event.type) {
    case SDL_KEYDOWN:
      pp_event->type = PP_INPUTEVENT_TYPE_KEYDOWN;
      pp_event->u.key.key_code = SDLKeyToPPKey(sdl_event.key.keysym.sym);
     return true;

    case SDL_KEYUP:
      pp_event->type = PP_INPUTEVENT_TYPE_KEYUP;
      pp_event->u.key.key_code = SDLKeyToPPKey(sdl_event.key.keysym.sym);
     return true;

    case SDL_MOUSEMOTION:
      pp_event->type = PP_INPUTEVENT_TYPE_MOUSEMOVE;
      pp_event->u.mouse.x = sdl_event.motion.x;
      pp_event->u.mouse.y = sdl_event.motion.y;
      return true;

    case SDL_MOUSEBUTTONDOWN:
      pp_event->type = PP_INPUTEVENT_TYPE_MOUSEDOWN;
      pp_event->u.mouse.button = SDLButtonToPPButton(sdl_event.button.button);
      return true;

    case SDL_MOUSEBUTTONUP:
      pp_event->type = PP_INPUTEVENT_TYPE_MOUSEUP;
      pp_event->u.mouse.button = SDLButtonToPPButton(sdl_event.button.button);
      return true;

    case SDL_USEREVENT:
      MakeUserEvent(pp_event,
                    sdl_event.user.code,
                    ptr2int(sdl_event.user.data1),
                    ptr2int(sdl_event.user.data2));
      return true;
    case SDL_QUIT:
      pp_event->type = (PP_InputEvent_Type) PP_INPUTEVENT_TERMINATION;
      return true;
    case SDL_ACTIVEEVENT:
    case SDL_SYSWMEVENT:
    case SDL_VIDEORESIZE:
    case SDL_VIDEOEXPOSE:

    case SDL_JOYAXISMOTION:
    case SDL_JOYBALLMOTION:
    case SDL_JOYHATMOTION:
    case SDL_JOYBUTTONDOWN:
    case SDL_JOYBUTTONUP:

    default:
      return false;
  }
}
