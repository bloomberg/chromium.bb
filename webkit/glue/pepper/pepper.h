// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_PEPPER_PEPPER_H_
#define WEBKIT_GLUE_PEPPER_PEPPER_H_

#ifdef PEPPER_APIS_ENABLED

#include "base/basictypes.h"
#include "third_party/npapi/bindings/npapi.h"

/*
 * A fake "enum" value for getting Pepper extensions.
 * The variable returns a pointer to an NPPepperExtensions structure
 */
#define NPNVPepperExtensions ((NPNVariable) 4000)

typedef enum {
  NPMouseButton_None    = -1,
  NPMouseButton_Left    = 0,
  NPMouseButton_Middle  = 1,
  NPMouseButton_Right   = 2,
} NPMouseButtons;

typedef enum {
  NPEventType_Undefined   = -1,
  NPEventType_MouseDown   = 0,
  NPEventType_MouseUp     = 1,
  NPEventType_MouseMove   = 2,
  NPEventType_MouseEnter  = 3,
  NPEventType_MouseLeave  = 4,
  NPEventType_MouseWheel  = 5,
  NPEventType_RawKeyDown  = 6,
  NPEventType_KeyDown     = 7,
  NPEventType_KeyUp       = 8,
  NPEventType_Char        = 9,
  NPEventType_Minimize    = 10,
  NPEventType_Focus       = 11,
  NPEventType_Device      = 12
} NPEventTypes;

typedef enum {
  NPEventModifier_ShiftKey         = 1 << 0,
  NPEventModifier_ControlKey       = 1 << 1,
  NPEventModifier_AltKey           = 1 << 2,
  NPEventModifier_MetaKey          = 1 << 3,
  NPEventModifier_IsKeyPad         = 1 << 4,
  NPEventModifier_IsAutoRepeat     = 1 << 5,
  NPEventModifier_LeftButtonDown   = 1 << 6,
  NPEventModifier_MiddleButtonDown = 1 << 7,
  NPEventModifier_RightButtonDown  = 1 << 8
} NPEventModifiers;

typedef struct _NPKeyEvent
{
  uint32 modifier;
  uint32 normalizedKeyCode;
} NPKeyEvent;

typedef struct _NPCharacterEvent
{
  uint32 modifier;
  uint16 text[4];
  uint16 unmodifiedText[4];
} NPCharacterEvent;

typedef struct _NPMouseEvent
{
  uint32 modifier;
  int32 button;
  int32 x;
  int32 y;
  int32 clickCount;
} NPMouseEvent;

typedef struct _NPMouseWheelEvent
{
  uint32 modifier;
  float deltaX;
  float deltaY;
  float wheelTicksX;
  float wheelTicksY;
  uint32 scrollByPage;
} NPMouseWheelEvent;

typedef struct _NPDeviceEvent {
  uint32 device_uid;
  uint32 subtype;
  /* uint8 generic[0]; */
} NPDeviceEvent;

typedef struct _NPMinimizeEvent {
  int32 value;
} NPMinimizeEvent;

typedef struct _NPFocusEvent {
  int32 value;
} NPFocusEvent;

typedef struct _NPPepprEvent
{
  uint32 size;
  int32 type;
  double timeStampSeconds;
  union {
    NPKeyEvent key;
    NPCharacterEvent character;
    NPMouseEvent mouse;
    NPMouseWheelEvent wheel;
    NPMinimizeEvent minimize;
    NPFocusEvent focus;
    NPDeviceEvent device;
  } u;
} NPPepperEvent;

typedef struct _NPPepperRegion
{
  int32 x;
  int32 y;
  int32 w;
  int32 h;
} NPPepperRegion;

typedef enum _NPRenderType
{
  NPRenderGraphicsRGBA
} NPRenderType;

typedef struct _NPRenderContext
{
  union {
    struct {
      void* region;
      int32 stride;
    } graphicsRgba;
  } u;
} NPRenderContext;

typedef void (*NPFlushRenderContextCallbackPtr)(NPRenderContext* context,
                                                NPError err,
                                                void* userData);
typedef NPError (*NPInitializeRenderContextPtr)(NPP instance,
                                                NPRenderType type,
                                                NPRenderContext* context);
typedef NPError (*NPFlushRenderContextPtr)(NPP instance,
                                           NPRenderContext* context,
                                           NPFlushRenderContextCallbackPtr callback,
                                           void* userData);
typedef NPError (*NPDestroyRenderContextPtr)(NPP instance,
                                             NPRenderContext* context);
typedef NPError (*NPOpenFilePtr)(NPP instance, const char* fileName, void** handle);

typedef struct _NPPepperExtensions
{
  /* Renderer extensions */
  NPInitializeRenderContextPtr initializeRender;
  NPFlushRenderContextPtr flushRender;
  NPDestroyRenderContextPtr destroyRender;
  /* Shared memory extensions */

  /* I/O extensions */
  NPOpenFilePtr openFile;
} NPPepperExtensions;

#endif  /* PEPPER_APIS_ENABLED */

#endif  /* WEBKIT_GLUE_PEPPER_PEPPER_H_ */
