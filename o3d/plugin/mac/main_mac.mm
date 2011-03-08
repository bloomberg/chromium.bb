/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


// This file implements the platform specific parts of the plugin for
// the Macintosh platform.

#include "plugin/cross/main.h"

#import <Cocoa/Cocoa.h>
#include <Carbon/Carbon.h>
#include <OpenGL/OpenGL.h>
#include <AGL/agl.h>
#include <AGL/aglRenderers.h>

#include "base/logging.h"
#include "core/cross/event.h"
#include "core/mac/display_window_mac.h"
#include "plugin/cross/config.h"
#include "plugin/cross/plugin_metrics.h"
#include "plugin/mac/plugin_mac.h"
#include "plugin/mac/graphics_utils_mac.h"
#include "plugin/mac/fullscreen_window_mac.h"
#import "plugin/mac/o3d_layer.h"

using glue::_o3d::PluginObject;
using glue::StreamManager;
using o3d::Bitmap;
using o3d::DisplayWindowMac;
using o3d::Event;
using o3d::FullscreenWindowMac;
using o3d::Renderer;

namespace {

#define CFTIMER
// #define DEFERRED_DRAW_ON_NULLEVENTS


// Helper that extracts the O3DLayer obj c object from the PluginObject
// and coerces it to the right type.  The code can't live in the PluginObject
// since it's c++ code and doesn't know about objective c types, and it saves
// lots of casts elsewhere in the code.
static O3DLayer* ObjO3DLayer(PluginObject* obj) {
  return static_cast<O3DLayer*>(obj ? obj->gl_layer_ : nil);
}

void DrawPlugin(PluginObject* obj, bool send_callback, CGContextRef context) {
  obj->client()->RenderClient(send_callback);
  Renderer* renderer = obj->renderer();
  if (obj->IsOffscreenRenderingEnabled() && renderer && context) {
    DCHECK_EQ(obj->drawing_model_, NPDrawingModelCoreGraphics);
    DCHECK(obj->mac_cgl_pbuffer_);
    // We need to read back the framebuffer and draw it to the screen using
    // CoreGraphics.
    renderer->StartRendering();
    Bitmap::Ref bitmap = obj->GetOffscreenBitmap();
    obj->GetOffscreenRenderSurface()->GetIntoBitmap(bitmap);
    bitmap->FlipVertically();
    renderer->FinishRendering();
    uint8* data = bitmap->GetMipData(0);
    unsigned width = bitmap->width();
    unsigned height = bitmap->height();
    int rowBytes = width * 4;
    CGContextSaveGState(context);

    CGDataProviderRef dataProvider =
        CGDataProviderCreateWithData(0, data, rowBytes * height, 0);
    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
    // We need to use kCGImageAlphaNoneSkipFirst to discard the alpha channel.
    // O3D's output is currently semantically opaque.
    CGImageRef cgImage = CGImageCreate(width,
                                       height,
                                       8,
                                       32,
                                       rowBytes,
                                       colorSpace,
                                       kCGImageAlphaNoneSkipFirst |
                                       kCGBitmapByteOrder32Host,
                                       dataProvider,
                                       0,
                                       false,
                                       kCGRenderingIntentDefault);
    CGRect rect = CGRectMake(0, 0, width, height);
    // We want to completely overwrite the previous frame's
    // rendering results.
    CGContextSetBlendMode(context, kCGBlendModeCopy);
    CGContextSetInterpolationQuality(context, kCGInterpolationNone);
    CGContextDrawImage(context, rect, cgImage);
    CGImageRelease(cgImage);
    CGColorSpaceRelease(colorSpace);
    CGDataProviderRelease(dataProvider);
    CGContextRestoreGState(context);
  }
}

unsigned char GetMacEventKeyChar(const EventRecord *the_event) {
  unsigned char the_char = the_event->message & charCodeMask;
  return the_char;
}

unsigned char GetMacEventKeyCode(const EventRecord *the_event) {
  unsigned char the_key_code = (the_event->message & keyCodeMask) >> 8;
  return the_key_code;
}


// Cocoa key events for things like arrow key presses can have strange unicode
// values in the 0xF700-0xF747 range, eg NSUpArrowFunctionKey is 0xF700.
// These values are defined in NSEvent.h.
// Map the ones we care about to the more commonly understood values in
// the Mac system header Events.h, eg kUpArrowCharCode is 30.
int TranslateMacUnicodeControlChar(int theChar) {
  switch(theChar) {
    case NSUpArrowFunctionKey:
      return kUpArrowCharCode;
    case NSDownArrowFunctionKey:
      return kDownArrowCharCode;
    case NSLeftArrowFunctionKey:
      return kLeftArrowCharCode;
    case NSRightArrowFunctionKey:
      return kRightArrowCharCode;
  }
  return theChar;
}


// The Mac standard char codes for arrow keys are different from the
// web standard.
// Also in the browser world the enter key gets mapped to be the same as the
// return key.
int TranslateMacControlCharToWebChar(int theChar) {
  switch(theChar) {
    case kUpArrowCharCode:
      return 38;
    case kDownArrowCharCode:
      return 40;
    case kLeftArrowCharCode:
      return 37;
    case kRightArrowCharCode:
      return 39;
    case kEnterCharCode:
      return kReturnCharCode;
  }
  return theChar;
}


// Given an instance, and some event data, calls Javascript methods
// placed on the object tag so that the keystrokes can be handled in
// Javascript.
void DispatchKeyboardEvent(PluginObject* obj,
                           EventKind kind,
                           int theChar,
                           int theKeyCode,
                           EventModifiers mods) {
  theChar = TranslateMacUnicodeControlChar(theChar);
  theChar = TranslateMacControlCharToWebChar(theChar);
  int upperChar = (theChar >= 'a' && theChar <='z') ? theChar - 32 : theChar;

  Event::Type type = Event::TYPE_KEYDOWN;  // Init to something valid.
  switch (kind) {
    case keyDown:
      // We'll also have to send a keypress below.
      type = Event::TYPE_KEYDOWN;
      break;
    case autoKey:
      type = Event::TYPE_KEYPRESS;
      break;
    case keyUp:
      type = Event::TYPE_KEYUP;
      break;
    default:
      return;
      break;
  }
  Event event(type);

  switch (kind) {
    case keyDown:
    case keyUp:
      event.set_key_code(upperChar);
      break;
    case autoKey:
      event.set_char_code(theChar);
      break;
    default:
      LOG(FATAL) << "Unknown keyboard event: " << kind;
  }

  int modifier_state = 0;
  if (mods & controlKey) {
    modifier_state |= Event::MODIFIER_CTRL;
  }
  if (mods & shiftKey) {
    modifier_state |= Event::MODIFIER_SHIFT;
  }
  if (mods & optionKey) {
    modifier_state |= Event::MODIFIER_ALT;
  }
  if (mods & cmdKey) {
    modifier_state |= Event::MODIFIER_META;
  }
  event.set_modifier_state(modifier_state);
  obj->client()->AddEventToQueue(event);
  if (type == Event::TYPE_KEYDOWN) {
    event.clear_key_code();
    event.set_char_code(theChar);
    event.set_type(Event::TYPE_KEYPRESS);
    obj->client()->AddEventToQueue(event);
  }
}

// Given an instance, and a MacOS keyboard event, calls Javascript methods
// placed on the object tag so that the keystrokes can be handled in
// Javascript.
void DispatchMacKeyboardEvent(PluginObject* obj,
                              EventRecord* the_event) {
  DispatchKeyboardEvent(obj,
                        the_event->what,
                        GetMacEventKeyChar(the_event),
                        GetMacEventKeyCode(the_event),
                        the_event->modifiers);
}

static void HandleCarbonMouseEvent(PluginObject* obj,
                                   EventRecord* the_event) {
  DCHECK(obj);
  DCHECK(obj->client());
  int screen_x = the_event->where.h;
  int screen_y = the_event->where.v;
  static Point last_mouse_loc = {0,0};

  o3d::Event::Type type;
  switch (the_event->what) {
    case mouseDown:
      type = o3d::Event::TYPE_MOUSEDOWN;
      break;
    case mouseUp:
      type = o3d::Event::TYPE_MOUSEUP;
      break;
    case nullEvent:
    case osEvt:  // can be various things but in this context it's mouse move
      if (last_mouse_loc.h == screen_x && last_mouse_loc.v == screen_y)
        return;
      type = o3d::Event::TYPE_MOUSEMOVE;
      break;
    default:
      LOG(FATAL) << "Unknown mouse event: " << the_event->what;
      return;
  }

  last_mouse_loc = the_event->where;
  bool in_plugin = false;  // Did the event happen within our drawing region?

  int x, y;
  // now make x and y plugin relative coords
  if (obj->GetFullscreenMacWindow()) {
    Rect wBounds = o3d::CGRect2Rect(
        obj->GetFullscreenMacWindow()->GetWindowBounds());
    x = screen_x - wBounds.left;
    y = screen_y - wBounds.top;
    in_plugin = true;
  } else {
    Rect wBounds;
    GetWindowBounds(obj->mac_window_,
                    (obj->drawing_model_ == NPDrawingModelQuickDraw) ?
                        kWindowGlobalPortRgn : kWindowStructureRgn,
                    &wBounds);
    x = screen_x - wBounds.left - obj->last_plugin_loc_.h;
    y = screen_y - wBounds.top - obj->last_plugin_loc_.v;
    in_plugin = x >= 0 && y >= 0 && x < obj->width() && y < obj->height();
  }

  o3d::Event event(type);
  int modifier_state = 0;
  if (the_event->modifiers & controlKey) {
    modifier_state |= o3d::Event::MODIFIER_CTRL;
  }
  if (the_event->modifiers & shiftKey) {
    modifier_state |= o3d::Event::MODIFIER_SHIFT;
  }
  if (the_event->modifiers & optionKey) {
    modifier_state |= o3d::Event::MODIFIER_ALT;
  }
  if (the_event->modifiers & cmdKey) {
    modifier_state |= o3d::Event::MODIFIER_META;
  }

  event.set_modifier_state(modifier_state);

  // TODO: Figure out how to detect and set buttons properly.
  if (the_event->modifiers & btnState) {
    event.set_button(o3d::Event::BUTTON_LEFT);
  }

  event.set_position(x, y, screen_x, screen_y, in_plugin);
  obj->client()->AddEventToQueue(event);

  if (in_plugin && type == Event::TYPE_MOUSEDOWN &&
      obj->HitFullscreenClickRegion(x, y)) {
    obj->RequestFullscreenDisplay();
  }
}

// Handle a mouse-related NPCocoaEvent.
// These events come from the new Cocoa revision of the NPAPI spec,
// currently implemented only in Safari.
// See https://wiki.mozilla.org/Mac:NPAPI_Event_Models
static void HandleCocoaMouseEvent(PluginObject* obj,
                                  NPCocoaEvent* the_event) {
  DCHECK(obj);
  DCHECK(obj->client());
  int screen_x = the_event->data.mouse.pluginX;
  int screen_y = the_event->data.mouse.pluginY;

  o3d::Event::Type type;
  switch (the_event->type) {
    case NPCocoaEventMouseDown:
      type = o3d::Event::TYPE_MOUSEDOWN;
      break;
    case NPCocoaEventMouseUp:
      type = o3d::Event::TYPE_MOUSEUP;
      break;
    // The Mac makes a distinction between moved and dragged (ie moved with
    // the button down), but the O3D event model does not.
    case NPCocoaEventMouseMoved:
    case NPCocoaEventMouseDragged:
      type = o3d::Event::TYPE_MOUSEMOVE;
      break;
    case NPCocoaEventScrollWheel:
      type = o3d::Event::TYPE_WHEEL;
      break;
    // Don't care about these currently.
    case NPCocoaEventMouseEntered:
    case NPCocoaEventMouseExited:
    default:
      return;
  }

  int x = the_event->data.mouse.pluginX;
  int y = the_event->data.mouse.pluginY;

  // Did the event happen within our drawing region?
  bool in_plugin = x >= 0 && y >= 0 && x < obj->width() && y < obj->height();

  o3d::Event event(type);
  int modifier_state = 0;
  if (the_event->data.mouse.modifierFlags & NSControlKeyMask) {
    modifier_state |= o3d::Event::MODIFIER_CTRL;
  }
  if (the_event->data.mouse.modifierFlags &
      (NSAlphaShiftKeyMask | NSShiftKeyMask)) {
    modifier_state |= o3d::Event::MODIFIER_SHIFT;
  }
  if (the_event->data.mouse.modifierFlags & NSAlternateKeyMask) {
    modifier_state |= o3d::Event::MODIFIER_ALT;
  }
  if (the_event->data.mouse.modifierFlags & NSCommandKeyMask) {
    modifier_state |= o3d::Event::MODIFIER_META;
  }

  event.set_modifier_state(modifier_state);

  if (the_event->type == NPCocoaEventScrollWheel) {
    if (the_event->data.mouse.deltaX && the_event->data.mouse.deltaY) {
      if (abs(the_event->data.mouse.deltaX) >
          abs(the_event->data.mouse.deltaY)) {
        the_event->data.mouse.deltaY = 0;
      } else {
        the_event->data.mouse.deltaX = 0;
      }
    }
    event.set_delta(the_event->data.mouse.deltaX * 10.0,
                    the_event->data.mouse.deltaY * 10.0);
  }

  if ((the_event->type == NPCocoaEventMouseDown) ||
      (the_event->type == NPCocoaEventMouseUp)) {
    event.set_button(
        o3d::MacOSMouseButtonNumberToO3DButton(
            the_event->data.mouse.buttonNumber));
  }

  event.set_position(x, y, screen_x, screen_y, in_plugin);
  obj->client()->AddEventToQueue(event);

  if (in_plugin && type == Event::TYPE_MOUSEDOWN &&
      obj->HitFullscreenClickRegion(x, y)) {
    obj->RequestFullscreenDisplay();
  }
}




// Convert an NSEvent style modifiers field to an EventRecord style one.
// Not all bits have a representation in the target type, eg NSFunctionKeyMask
// but we just need to convert the basic modifiers that need to be passed on
// to Javascript.
EventModifiers CocoaToEventRecordModifiers(NSUInteger inMods) {
  EventModifiers outMods = 0;

  // NSEvent distinuishes between the shift key being down and the capslock key,
  // but the W3C event spec does not make this distinction.
  if (inMods & (NSAlphaShiftKeyMask | NSShiftKeyMask))
    outMods |= shiftKey;
  if (inMods & NSControlKeyMask)
    outMods |= controlKey;
  if (inMods & NSAlternateKeyMask)
    outMods |= optionKey;
  if (inMods & NSCommandKeyMask)
    outMods |= cmdKey;

  return outMods;
}

// List of message types from mozilla's nsplugindefs.h, which is more
// complete than the list in NPAPI.h.
enum nsPluginEventType {
  nsPluginEventType_GetFocusEvent = (osEvt + 16),
  nsPluginEventType_LoseFocusEvent,
  nsPluginEventType_AdjustCursorEvent,
  nsPluginEventType_MenuCommandEvent,
  nsPluginEventType_ClippingChangedEvent,
  nsPluginEventType_ScrollingBeginsEvent,
  nsPluginEventType_ScrollingEndsEvent,
  nsPluginEventType_Idle = 0
};

// When to prefer Core Animation. Safari's support in 10.5 was too
// buggy to attempt to use.
static bool PreferCoreAnimation() {
  bool isSafari = o3d::metric_browser_type.value() == o3d::BROWSER_NAME_SAFARI;
  return (!isSafari || o3d::IsMacOSTenSixOrHigher());
}
  
static void SuppressRenderModeAuto(PluginObject* obj) {
  // If the web app specified RenderMode=Auto but the best available drawing
  // model for 3D is one for which 2D is not implemented then we cannot
  // provide an automatic 2D fallback because it is not legal to change drawing
  // or event models outside of NPP_New(). To achieve a 2D fallback the web app
  // will have to watch for initialization errors and reload the plugin with
  // RenderMode=2D.
  if (obj->features()->render_mode() == Renderer::RENDER_MODE_AUTO) {
    DLOG(INFO) << "Suppressing RenderMode=Auto for incompatible drawing model";
    obj->features()->set_render_mode(Renderer::RENDER_MODE_3D);
  }
}

// Negotiates the best plugin drawing and event model, sets the
// browser to use that, and updates the PluginObject so we can
// remember which one we chose. We prefer these combinations in the
// given order:
//  - Core Animation drawing model, Cocoa event model
//  - QuickDraw drawing model, Carbon event model
//  - Core Graphics drawing model, Cocoa or Carbon event model
// If the browser doesn't even understand the question, we use the
// QuickDraw drawing model and Carbon event model for best backward
// compatibility.
//
// This ordering provides the best forward-looking behavior while
// still providing compatibility with older browsers. Even though Core Graphics
// is newer than QuickDraw, we prefer QuickDraw when available because the Core
// Graphics rendering path with OpenGL is pathologically sub-optimal.
//
// Returns NPERR_NO_ERROR (0) if successful, otherwise an NPError code.
NPError Mac_SetBestEventAndDrawingModel(NPP instance, PluginObject* obj) {
  NPError err = NPERR_NO_ERROR;
  NPBool supportsCocoaEventModel = FALSE;
  NPBool supportsCarbonEventModel = FALSE;
  NPBool supportsCoreGraphics = FALSE;
  NPBool supportsQuickDraw = FALSE;
  NPBool supportsCoreAnimation = FALSE;

  // See if browser supports Cocoa event model.
  err = NPN_GetValue(instance,
                     NPNVsupportsCocoaBool,
                     &supportsCocoaEventModel);
  if (err != NPERR_NO_ERROR) {
    supportsCocoaEventModel = FALSE;
    err = NPERR_NO_ERROR;  // browser doesn't support switchable event models
  }

  // See if browser supports Carbon event model.
  err = NPN_GetValue(instance,
                     NPNVsupportsCarbonBool,
                     &supportsCarbonEventModel);
  if (err != NPERR_NO_ERROR) {
    supportsCarbonEventModel = FALSE;
    err = NPERR_NO_ERROR;
  }

  // Test for QuickDraw support.
  err = NPN_GetValue(instance,
                     NPNVsupportsQuickDrawBool,
                     &supportsQuickDraw);
  if (err != NPERR_NO_ERROR)
    supportsQuickDraw = FALSE;

  // Test for Core Graphics support.
  err = NPN_GetValue(instance,
                     NPNVsupportsCoreGraphicsBool,
                     &supportsCoreGraphics);
  if (err != NPERR_NO_ERROR)
    supportsCoreGraphics = FALSE;

  // Test for Core Animation support.
  err = NPN_GetValue(instance,
                     NPNVsupportsCoreAnimationBool,
                     &supportsCoreAnimation);
  if (err != NPERR_NO_ERROR)
    supportsCoreAnimation = FALSE;

  // Fix up values for older browsers which don't even understand
  // these questions.
  if (!supportsCarbonEventModel && !supportsCocoaEventModel) {
    supportsCarbonEventModel = TRUE;
  }

  if (!supportsQuickDraw && !supportsCoreGraphics && !supportsCoreAnimation) {
    // Must be a very old browser such as FF2. Avoid calling
    // NPN_SetValue for the event or drawing models at all.
    obj->drawing_model_ = NPDrawingModelQuickDraw;
    obj->event_model_ = NPEventModelCarbon;
    DLOG(INFO) << "Legacy browser, assuming QuickDraw and Carbon";
    SuppressRenderModeAuto(obj);
    return NPERR_NO_ERROR;
  }

  // Now that we have our information, decide on the appropriate combination.
  NPDrawingModel drawing_model = NPDrawingModelQuickDraw;
  NPEventModel event_model = NPEventModelCarbon;

  // If the web app has requested 2D mode then we only want to select drawing
  // models supported by RendererCairo (currently only Core Graphics).
  bool wants_2d = obj->features()->render_mode() == Renderer::RENDER_MODE_2D;

  if (!wants_2d && supportsCoreAnimation && supportsCocoaEventModel &&
      PreferCoreAnimation()) {
    drawing_model = NPDrawingModelCoreAnimation;
    event_model = NPEventModelCocoa;
    DLOG(INFO) << "Selecting Core Animation and Cocoa";
    SuppressRenderModeAuto(obj);
  } else if (!wants_2d && supportsQuickDraw && supportsCarbonEventModel) {
    drawing_model = NPDrawingModelQuickDraw;
    event_model = NPEventModelCarbon;
    DLOG(INFO) << "Selecting QuickDraw and Carbon";
    SuppressRenderModeAuto(obj);
  } else if (supportsCoreGraphics && supportsCocoaEventModel) {
    drawing_model = NPDrawingModelCoreGraphics;
    event_model = NPEventModelCocoa;
    DLOG(INFO) << "Selecting Core Graphics and Cocoa";
  } else if (supportsCoreGraphics && supportsCarbonEventModel) {
    drawing_model = NPDrawingModelCoreGraphics;
    event_model = NPEventModelCarbon;
    DLOG(INFO) << "Selecting Core Graphics and Carbon";
  } else {
    // If all of the above tests failed, we are running on a browser
    // which we don't know how to handle.
    return NPERR_GENERIC_ERROR;
  }

  // Earlier versions of this code did not check the return value of
  // the call to set the event model.
  NPN_SetValue(instance, NPPVpluginEventModel,
               reinterpret_cast<void*>(event_model));
  err = NPN_SetValue(instance, NPPVpluginDrawingModel,
                     reinterpret_cast<void*>(drawing_model));

  if (err != NPERR_NO_ERROR) {
    return err;
  }

  obj->drawing_model_ = drawing_model;
  obj->event_model_ = event_model;
  return NPERR_NO_ERROR;
}
}  // end anonymous namespace

#if defined(O3D_INTERNAL_PLUGIN)
namespace o3d {
#else
extern "C" {
#endif

#if !defined(O3D_INTERNAL_PLUGIN)

// Wrapper that discards the return value to match the expected type of
// NPP_ShutdownProcPtr.
static void NPP_ShutdownWrapper() {
  NP_Shutdown();
}

// This code is needed to support browsers based on a slightly dated version
// of the NPAPI such as Firefox 2, and Camino 1.6. These browsers expect there
// to be a main() to call to do basic setup.
int main(NPNetscapeFuncs* browserFuncs,
         NPPluginFuncs* pluginFuncs,
         NPP_ShutdownProcPtr* shutdownProc) {
  HANDLE_CRASHES;
  NPError error = NP_Initialize(browserFuncs);
  if (error == NPERR_NO_ERROR)
    error = NP_GetEntryPoints(pluginFuncs);
  *shutdownProc = NPP_ShutdownWrapper;

  return error;
}

#endif  // O3D_INTERNAL_PLUGIN

}  // namespace o3d / extern "C"

namespace o3d {

NPError PlatformPreNPInitialize() {
#if !defined(O3D_INTERNAL_PLUGIN)
  o3d::InitializeBreakpad();

#ifdef CFTIMER
  o3d::gRenderTimer.Start();
#endif  // CFTIMER
#endif  // O3D_INTERNAL_PLUGIN

  return NPERR_NO_ERROR;
}

NPError PlatformPostNPInitialize() {
  return NPERR_NO_ERROR;
}

NPError PlatformPreNPShutdown() {
  return NPERR_NO_ERROR;
}

NPError PlatformPostNPShutdown() {
#if !defined(O3D_INTERNAL_PLUGIN)
#ifdef CFTIMER
  o3d::gRenderTimer.Stop();
#endif

  o3d::ShutdownBreakpad();
#endif  // O3D_INTERNAL_PLUGIN

  return NPERR_NO_ERROR;
}

NPError PlatformNPPGetValue(PluginObject *obj,
                            NPPVariable variable,
                            void *value) {
  switch (variable) {
    case NPPVpluginCoreAnimationLayer:
      if (!ObjO3DLayer(obj)) {
        // Setup layer
        O3DLayer* gl_layer = [[[O3DLayer alloc] init] retain];

        gl_layer.autoresizingMask =
            kCALayerWidthSizable + kCALayerHeightSizable;
        obj->gl_layer_ = gl_layer;

        [gl_layer setPluginObject:obj];
      }
      // Make sure to return a retained layer
      *(CALayer**)value = ObjO3DLayer(obj);
      return NPERR_NO_ERROR;
      break;
    default:
      return NPERR_INVALID_PARAM;
  }

  return NPERR_INVALID_PARAM;
}

bool HandleCarbonEvent(EventRecord* the_event, NPP instance) {
  PluginObject* obj = static_cast<PluginObject*>(instance->pdata);
  bool handled = false;
  FullscreenWindowMac* fullscreen_window = obj->GetFullscreenMacWindow();

  if (g_logger) g_logger->UpdateLogging();

  // Help the plugin keep track of when we last saw an event so the CFTimer can
  // notice if we get cut off, eg by our tab being hidden by Safari, which there
  // is no other way for us to detect.
  obj->MacEventReceived(the_event->what != nsPluginEventType_LoseFocusEvent);

  switch (the_event->what) {
    case nullEvent:
#ifdef DEFERRED_DRAW_ON_NULLEVENTS
      GLUE_PROFILE_START(instance, "forceredraw");
      NPN_ForceRedraw(instance);  // invalidate to cause a redraw
      GLUE_PROFILE_STOP(instance, "forceredraw");
#elif defined(CFTIMER)
#else
      DrawPlugin(obj, true, obj->mac_cg_context_ref_);
#endif
      // Safari tab switching recovery code.
      if (obj->mac_surface_hidden_) {
        obj->mac_surface_hidden_ = false;
        NPN_ForceRedraw(instance);  // invalidate to cause a redraw
      }

      // Auto-recovery for any situation where another window comes in front
      // of the fullscreen window and we need to exit fullscreen mode.
      // This can happen because another browser window has come forward, or
      // because another app has been called to the front.
      if (fullscreen_window && !fullscreen_window->IsActive()) {
        obj->CancelFullscreenDisplay();
      }

      // Send nullEvents to HandleCarbonMouseEvent so they can be used to
      // simulate mouse moved events. Not needed in fullscreen mode, where we
      // really do get mouse moved events. See the osEvt case below.
      if (!fullscreen_window)
        HandleCarbonMouseEvent(obj, the_event);

      handled = true;
      break;
    case updateEvt:
      // We dispatch a render callback from this point iff using Core Graphics.
      // With CoreGraphics+Carbon this is the only source of rendering, whereas
      // with QuickDraw+Carbon the main source of rendering is in
      // RenderTimer::TimerCallback().
      DrawPlugin(obj,
                 obj->drawing_model_ == NPDrawingModelCoreGraphics,
                 obj->mac_cg_context_ref_);
      handled = true;
      break;
    case osEvt:
      // These are mouse moved messages when so tagged in the high byte.
      if ((the_event->message >> 24) == mouseMovedMessage) {
        HandleCarbonMouseEvent(obj, the_event);
        handled = true;
      }
      break;
    case mouseDown:
      HandleCarbonMouseEvent(obj, the_event);
      break;
    case mouseUp:
      HandleCarbonMouseEvent(obj, the_event);
      handled = true;
      break;
    case keyDown:
      // Hard-coded trapdoor to get out of fullscreen mode if user hits escape.
      if ((GetMacEventKeyChar(the_event) == '\e') && fullscreen_window) {
        obj->CancelFullscreenDisplay();
        break;
      }  // otherwise fall through
    case autoKey:
    case keyUp: {
      DispatchMacKeyboardEvent(obj, the_event);
      handled = true;
      break;
    }
    case nsPluginEventType_ScrollingBeginsEvent:
      obj->SetScrollIsInProgress(true);
      break;
    case nsPluginEventType_ScrollingEndsEvent:
      obj->SetScrollIsInProgress(false);
      break;
    default:
      break;
  }
  return handled;
}

// Handle an NPCocoaEvent style event. The Cocoa event interface is
// a recent addition to the NAPI spec.
// See https://wiki.mozilla.org/Mac:NPAPI_Event_Models for further details.
// The principle advantages are that we can get scrollwheel messages,
// mouse-moved messages, and can tell which mouse button was pressed.
// This API will also be required for a carbon-free 64 bit version for 10.6.
bool HandleCocoaEvent(NPP instance, NPCocoaEvent* the_event,
                      bool initiated_from_browser) {
  PluginObject* obj = static_cast<PluginObject*>(instance->pdata);
  FullscreenWindowMac* fullscreen_window = obj->GetFullscreenMacWindow();
  bool handled = false;

  if (g_logger) g_logger->UpdateLogging();

  bool lostFocus = the_event->type == NPCocoaEventFocusChanged &&
      !the_event->data.focus.hasFocus;
  obj->MacEventReceived(!lostFocus);
  switch (the_event->type) {
    case NPCocoaEventDrawRect:
      if (obj->drawing_model_ == NPDrawingModelCoreAnimation) {
        O3DLayer* layer = ObjO3DLayer(obj);
        if (layer) {
          [layer setNeedsDisplay];
        }
      } else {
        // We need to call the render callback from here if we are using
        // Core Graphics because it doesn't get called anywhere else.
        CGContextRef new_mac_cg_context_ref = the_event->data.draw.context;
        if (new_mac_cg_context_ref != obj->mac_cg_context_ref_) {
          obj->mac_cg_context_ref_ = new_mac_cg_context_ref;
          // Update the Renderer's CGContextRef (only RendererCairo actually
          // uses it).
          o3d::DisplayWindowMac display;
          display.set_agl_context(obj->mac_agl_context_);
          display.set_cgl_context(obj->mac_cgl_context_);
          display.set_cg_context_ref(obj->mac_cg_context_ref_);
          obj->renderer()->ChangeDisplayWindow(display);
        }
        DrawPlugin(obj, true, obj->mac_cg_context_ref_);
      }
      handled = true;
      break;
    case NPCocoaEventMouseDown:
    case NPCocoaEventMouseUp:
    case NPCocoaEventMouseMoved:
    case NPCocoaEventMouseDragged:
    case NPCocoaEventMouseEntered:
    case NPCocoaEventMouseExited:
    case NPCocoaEventScrollWheel:
      if (the_event->type == NPCocoaEventMouseUp &&
          initiated_from_browser && fullscreen_window) {
        // The mouse-up event associated with the mouse-down that caused
        // the app to go full-screen is dispatched against the browser
        // window rather than the full-screen window. Apps that have an
        // icon which toggles full-screen and windowed mode therefore come
        // out of full-screen immediately when the icon is clicked, since
        // the mouse-up occurs at the same location as the mouse-down. Work
        // around this by squelching this mouse up event. This seems to
        // work acceptably for known apps. We could do better by
        // redispatching all mouse events through the full-screen window
        // until the first mouse up.
        break;
      }

      HandleCocoaMouseEvent(obj, the_event);
      break;
    case NPCocoaEventKeyDown:
      // Hard-coded trapdoor to get out of fullscreen mode if user hits escape.
      if (fullscreen_window) {
        NSString *chars =
            (NSString*) the_event->data.key.charactersIgnoringModifiers;

        if (chars == NULL || [chars length] == 0) {
          break;
        }

        if ([chars characterAtIndex:0] == '\e') {
          obj->CancelFullscreenDisplay();
          break;
        }
      }  // otherwise fall through
    case NPCocoaEventKeyUp: {
      EventKind eventKind = (the_event->type == NPCocoaEventKeyUp) ? keyUp :
                            (the_event->data.key.isARepeat) ? autoKey : keyDown;

      EventModifiers modifiers =
          CocoaToEventRecordModifiers(the_event->data.key.modifierFlags);

      NSString *chars =
          (NSString*) the_event->data.key.charactersIgnoringModifiers;

      if (chars == NULL || [chars length] == 0) {
        break;
      }

      DispatchKeyboardEvent(obj,
                            eventKind,
                            [chars characterAtIndex:0],
                            the_event->data.key.keyCode,
                            modifiers);
      break;
    }
    case NPCocoaEventFlagsChanged:
    case NPCocoaEventFocusChanged:
    case NPCocoaEventWindowFocusChanged:
      // Safari tab switching recovery code.
      if (obj->mac_surface_hidden_) {
        obj->mac_surface_hidden_ = false;
        NPN_ForceRedraw(instance);  // invalidate to cause a redraw
      }

      // Auto-recovery for any situation where another window comes in front
      // of the fullscreen window and we need to exit fullscreen mode.
      // This can happen because another browser window has come forward, or
      // because another app has been called to the front.
      // TODO: We'll have problems with this when dealing with e.g.
      // Japanese text input IME windows.
      // Note that we ignore focus transfer events coming from the
      // browser while in full-screen mode, because otherwise we
      // frequently disable full-screen mode almost immediately after
      // entering it.
      if (fullscreen_window && !fullscreen_window->IsActive() &&
          !initiated_from_browser) {
        obj->CancelFullscreenDisplay();
      }

      break;
    case NPCocoaEventTextInput:
      break;
  }

  return handled;
}

NPError PlatformNPPNew(NPP instance, PluginObject *obj) {
  NPError err = Mac_SetBestEventAndDrawingModel(instance, obj);
  if (err != NPERR_NO_ERROR)
    return err;
#ifdef CFTIMER
  if (obj->drawing_model_ == NPDrawingModelCoreAnimation) {
    o3d::gRenderTimer.AddInstance(instance);
  }
#endif
  return NPERR_NO_ERROR;
}

NPError PlatformNPPDestroy(NPP instance, PluginObject *obj) {
#if defined(CFTIMER)
  o3d::gRenderTimer.RemoveInstance(instance);
#endif

  // TODO(maf) / TODO(kbr): are we leaking AGL / CGL contexts?

  if (obj->drawing_model_ == NPDrawingModelCoreAnimation) {
    O3DLayer* layer = ObjO3DLayer(obj);
    if (layer) {
      // Prevent the layer from rendering any more.
      [layer setPluginObject:NULL];
    }
  }

  obj->TearDown();

  return NPERR_NO_ERROR;
}

bool CheckForAGLError() {
  return aglGetError() != AGL_NO_ERROR;
}

NPError PlatformNPPSetWindow(NPP instance,
                             PluginObject *obj,
                             NPWindow* window) {
  WindowRef new_window = NULL;

  assert(window != NULL);

  if (window->window == NULL &&
      obj->drawing_model_ != NPDrawingModelCoreGraphics &&
      obj->drawing_model_ != NPDrawingModelCoreAnimation) {
    return NPERR_NO_ERROR;
  }

  obj->last_plugin_loc_.h = window->x;
  obj->last_plugin_loc_.v = window->y;

  switch (obj->drawing_model_) {
    case NPDrawingModelCoreAnimation: {
      O3DLayer* o3dLayer = ObjO3DLayer(obj);
      [o3dLayer setWidth: window->width height: window->height];
      return NPERR_NO_ERROR;
    }
    case NPDrawingModelCoreGraphics: {
      // In some browsers (Safari 4 on 10.5, Chrome on 10.5), window->window is
      // NULL when using the Cocoa event model. In that situation we get our
      // CGContextRef in HandleCocoaEvent() instead.
      if (window->window != NULL) {
        NP_CGContext* np_cg = reinterpret_cast<NP_CGContext*>(window->window);
        if (obj->event_model_ == NPEventModelCocoa) {
          NSWindow * ns_window = reinterpret_cast<NSWindow*>(np_cg->window);
          new_window = reinterpret_cast<WindowRef>([ns_window windowRef]);
        } else {
          new_window = static_cast<OpaqueWindowPtr*>(np_cg->window);
        }
        obj->mac_cg_context_ref_ = np_cg->context;
      }
      break;
    }
    case NPDrawingModelQuickDraw: {
      NP_Port* np_qd = reinterpret_cast<NP_Port*>(window->window);
      if (np_qd->port)
        new_window = GetWindowFromPort(np_qd->port);
      // Safari 4 on Snow Leopard is sending us a spurious
      // NPP_SetWindow where we can not determine the WindowRef from
      // the port. Ignore this.
      if (obj->mac_window_ != NULL &&
          new_window == NULL)
          return NPERR_NO_ERROR;
      break;
    }
    default:
      return NPERR_INCOMPATIBLE_VERSION_ERROR;
      break;
  }

  // Whether the target window has changed.
  bool window_changed = new_window != obj->mac_window_;

  // Whether we already had a window before this call.
  bool had_a_window = obj->mac_window_ != NULL;

  obj->mac_window_ = new_window;

  if (obj->drawing_model_ == NPDrawingModelCoreAnimation) {
    if (obj->mac_cgl_context_) {
      CGLSetCurrentContext(obj->mac_cgl_context_);
    }
  } else if (obj->drawing_model_ == NPDrawingModelCoreGraphics) {
    // It's important to skip this when RenderMode=2D is specified because it
    // can make OSX on an MBP6,2 flip between the Intel and NVIDIA GPUs multiple
    // times in FF, which significantly delays our start-up.
    if (obj->features()->render_mode() != Renderer::RENDER_MODE_2D &&
        obj->mac_cgl_pbuffer_ == NULL) {
      // We initialize things with a CGL context rendering to a 1x1
      // pbuffer. Later we use the O3D RenderSurface APIs to set up the
      // framebuffer object which is used for rendering.
      CGLContextObj share_context = obj->GetFullscreenShareContext();
      CGLPixelFormatObj pixel_format = obj->GetFullscreenCGLPixelFormatObj();
      if (!share_context || !pixel_format) {
        // This can happen in FF when using Core Graphics, though curiously
        // only on certain websites.
        LOG(ERROR) << "Failed to create share context and/or pixel format";
        // We continue initializing, though without a CGLContextObj RendererGL
        // will fail to initialize. If the web app is using RenderMode=Auto
        // though then Cairo will work.
      } else {
        CGLError result;
        CGLContextObj context;
        result = CGLCreateContext(pixel_format, share_context, &context);
        if (result != kCGLNoError) {
          DLOG(ERROR) << "Error " << result << " creating context.";
          return NPERR_GENERIC_ERROR;
        }
        CGLPBufferObj pbuffer;
        if ((result = CGLCreatePBuffer(1, 1,
                                       GL_TEXTURE_2D, GL_RGBA,
                                       0, &pbuffer)) != kCGLNoError) {
          CGLDestroyContext(context);
          DLOG(ERROR) << "Error " << result << " creating pbuffer.";
          return NPERR_GENERIC_ERROR;
        }
        if ((result = CGLSetPBuffer(context, pbuffer, 0, 0, 0)) !=
            kCGLNoError) {
          CGLDestroyContext(context);
          CGLDestroyPBuffer(pbuffer);
          DLOG(ERROR) << "Error " << result << " attaching pbuffer to context.";
          return NPERR_GENERIC_ERROR;
        }
        // Must make the context current for renderer creation to succeed
        CGLSetCurrentContext(context);
        obj->mac_cgl_context_ = context;
        obj->mac_cgl_pbuffer_ = pbuffer;
      }
    }
  } else if (!had_a_window && obj->mac_agl_context_ == NULL) {
    // setup AGL context
    AGLPixelFormat myAGLPixelFormat = NULL;

  // We need to spec out a few similar but different sets of renderer
  // specifications - define some macros to lessen the duplication.
#define O3D_DEPTH_SETTINGS AGL_RGBA, AGL_DEPTH_SIZE, 24,
#define O3D_STENCIL_SETTINGS AGL_STENCIL_SIZE, 8,
#define O3D_HARDWARE_RENDERER AGL_ACCELERATED, AGL_NO_RECOVERY,
#define O3D_SOFTWARE_RENDERER AGL_RENDERER_ID, AGL_RENDERER_GENERIC_FLOAT_ID,
#define O3D_MULTISAMPLE AGL_MULTISAMPLE, \
  AGL_SAMPLE_BUFFERS_ARB, 1, AGL_SAMPLES_ARB, 4, AGL_MULTISAMPLE,
#define O3D_END AGL_NONE

#ifdef USE_AGL_DOUBLE_BUFFER
#define O3D_DOUBLE_BUFFER_SETTINGS AGL_DOUBLEBUFFER,
#else
#define O3D_DOUBLE_BUFFER_SETTINGS
#endif

    if (!UseSoftwareRenderer()) {
      // Try to create a hardware context with the following
      // specification
      GLint attributes[] = {
        O3D_DEPTH_SETTINGS
        O3D_STENCIL_SETTINGS
        O3D_DOUBLE_BUFFER_SETTINGS
        O3D_HARDWARE_RENDERER
        O3D_MULTISAMPLE
        O3D_END
      };
      myAGLPixelFormat = aglChoosePixelFormat(NULL,
                                              0,
                                              attributes);

      // If this fails, then don't try to be as ambitious
      // (so don't ask for multi-sampling), but still require hardware
      if (myAGLPixelFormat == NULL) {
        GLint low_end_attributes[] = {
          O3D_DEPTH_SETTINGS
          O3D_STENCIL_SETTINGS
          O3D_DOUBLE_BUFFER_SETTINGS
          O3D_HARDWARE_RENDERER
          O3D_END
        };
        myAGLPixelFormat = aglChoosePixelFormat(NULL,
                                                0,
                                                low_end_attributes);
      }
    }

    if (myAGLPixelFormat == NULL) {
      // Fallback to software renderer either if the vendorID/gpuID
      // is known to be problematic, or if the hardware context failed
      // to get created
      //
      // Note that we don't request multisampling since it's too slow.
      GLint software_renderer_attributes[] = {
        O3D_DEPTH_SETTINGS
        O3D_STENCIL_SETTINGS
        O3D_DOUBLE_BUFFER_SETTINGS
        O3D_SOFTWARE_RENDERER
        O3D_END
      };
      myAGLPixelFormat = aglChoosePixelFormat(NULL,
                                              0,
                                              software_renderer_attributes);

      obj->SetRendererIsSoftware(true);
    }

    if (myAGLPixelFormat == NULL || CheckForAGLError())
      return NPERR_MODULE_LOAD_FAILED_ERROR;

    obj->mac_agl_context_ = aglCreateContext(myAGLPixelFormat, NULL);

    if (CheckForAGLError())
      return NPERR_MODULE_LOAD_FAILED_ERROR;

    aglDestroyPixelFormat(myAGLPixelFormat);

    if (!SetWindowForAGLContext(obj->mac_agl_context_, obj->mac_window_))
      return NPERR_MODULE_LOAD_FAILED_ERROR;

    aglSetCurrentContext(obj->mac_agl_context_);

    GetOpenGLMetrics();

#ifdef USE_AGL_DOUBLE_BUFFER
    GLint swapInterval = 1;   // request synchronization
    aglSetInteger(obj->mac_agl_context_, AGL_SWAP_INTERVAL, &swapInterval);
#endif
  }

  int clipHeight = window->clipRect.bottom - window->clipRect.top;
  int clipWidth = window->clipRect.right - window->clipRect.left;

  int x_offset = window->clipRect.left - window->x;
  int y_offset = window->clipRect.top - window->y;
  int gl_x_origin = x_offset;
  int gl_y_origin = window->clipRect.bottom - (window->y + window->height);

  // Firefox calls us with an empty cliprect all the time, toggling our
  // cliprect back and forth between empty and the normal value, particularly
  // during scrolling.
  // As we need to make our AGL surface track the clip rect, honoring all of
  // these calls would result in spectacular flashing.
  // The goal here is to try to spot the calls we can safely ignore.
  // The bogus empty cliprects always have left and top of the real clip.
  // A null cliprect should always be honored ({0,0,0,0} means a hidden tab),
  // as should the first ever call to this function,
  // or an attempt to change the window.
  // The call at the end of a scroll step is also honored.
  // Otherwise, skip this change and do not hide our context.
  bool is_empty_cliprect = (clipHeight <= 0 || clipWidth <= 0);
  bool is_null_cliprect = (window->clipRect.bottom == 0 &&
                           window->clipRect.top == 0 &&
                           window->clipRect.left == 0 &&
                           window->clipRect.right == 0);

  if (is_empty_cliprect && (!is_null_cliprect) &&
      had_a_window && (!window_changed) && !obj->ScrollIsInProgress()) {
    return NPERR_NO_ERROR;
  }

  // Workaround - the Apple software renderer crashes if you set the drawing
  // area to 0x0, so use 1x1.
  if (is_empty_cliprect && obj->RendererIsSoftware())
    clipWidth = clipHeight = 1;

  // update size and location of the agl context
  if (obj->mac_agl_context_) {

    // We already had a window, and now we've got a different window -
    // this can happen when the user drags out a tab in Safari into its own
    // window.
    if (had_a_window && window_changed)
      SetWindowForAGLContext(obj->mac_agl_context_, obj->mac_window_);

    aglSetCurrentContext(obj->mac_agl_context_);

    Rect windowRect = {0, 0, 0, 0};
    GetWindowBounds(obj->mac_window_,
                    (obj->drawing_model_ == NPDrawingModelQuickDraw) ?
                        kWindowContentRgn : kWindowStructureRgn,
                    &windowRect);

    int windowHeight = windowRect.bottom - windowRect.top;

    // These are in window coords, the weird bit being that agl wants the
    // location of the bottom of the GL context in y flipped coords,
    // eg 100 would mean the bottom of the GL context was 100 up from the
    // bottom of the window.
    obj->last_buffer_rect_[0] = window->x + x_offset;
    obj->last_buffer_rect_[1] = (windowHeight - (window->y + clipHeight))
                                - y_offset;  // this param is y flipped
    obj->last_buffer_rect_[2] = clipWidth;
    obj->last_buffer_rect_[3] = clipHeight;
    obj->mac_surface_hidden_ = false;

    // If in fullscreen, just remember the desired change in geometry so
    // we restore to the right rectangle.
    if (obj->GetFullscreenMacWindow() != NULL)
      return NPERR_NO_ERROR;

    aglSetInteger(obj->mac_agl_context_, AGL_BUFFER_RECT,
                    obj->last_buffer_rect_);
    aglEnable(obj->mac_agl_context_, AGL_BUFFER_RECT);
  }

  if (obj->renderer()) {
    // Renderer is already initialized from a previous call to this function,
    // just update size and position and return.
    if (obj->drawing_model_ == NPDrawingModelCoreGraphics) {
      if (!obj->renderer()->SupportsCoreGraphics()) {
        obj->EnableOffscreenRendering();
      }
      obj->Resize(window->width, window->height);
    } else if (had_a_window) {
      obj->renderer()->SetClientOriginOffset(gl_x_origin, gl_y_origin);
      obj->Resize(window->width, window->height);
    }
    return NPERR_NO_ERROR;
  }

  // Else this is the first call.

  // Create and assign the graphics context.
  o3d::DisplayWindowMac default_display;
  default_display.set_agl_context(obj->mac_agl_context_);
  default_display.set_cgl_context(obj->mac_cgl_context_);
  default_display.set_cg_context_ref(obj->mac_cg_context_ref_);

  obj->CreateRenderer(default_display);

  // if the renderer cannot be created (maybe the features are not supported)
  // then we can proceed no further
  if (!obj->renderer()) {
    if (obj->mac_agl_context_) {
      ::aglDestroyContext(obj->mac_agl_context_);
      obj->mac_agl_context_ = NULL;
    }
  }

  obj->client()->Init();

  if (obj->renderer()) {
    if (obj->drawing_model_ == NPDrawingModelCoreGraphics) {
      if (!obj->renderer()->SupportsCoreGraphics()) {
        // Browser is using Core Graphics but renderer doesn't support it, so we
        // must render off-screen and then read back into software to re-render
        // with Core Graphics.
        obj->EnableOffscreenRendering();
      }
    } else {
      obj->renderer()->SetClientOriginOffset(gl_x_origin, gl_y_origin);
    }

    obj->Resize(window->width, window->height);
#ifdef CFTIMER
    // now that the graphics context is setup, add this instance to the timer
    // list so it gets drawn repeatedly
    gRenderTimer.AddInstance(instance);
#endif  // CFTIMER
  }

  return NPERR_NO_ERROR;
}

void PlatformNPPStreamAsFile(StreamManager *stream_manager,
                             NPStream *stream,
                             const char *fname) {
  // Some browsers give us an absolute HFS path in fname, some give us an
  // absolute Posix path, so convert to Posix if needed.
  if ((!fname) || (fname[0] == '/') || !fname[0]) {
    stream_manager->SetStreamFile(stream, fname);
  } else {
    const char* converted_fname = CreatePosixFilePathFromHFSFilePath(fname);
    if (converted_fname == NULL)
      return;  // TODO should also log error if we ever get here
    stream_manager->SetStreamFile(stream, converted_fname);
    delete converted_fname;
  }
}

int16 PlatformNPPHandleEvent(NPP instance, PluginObject *obj, void *event) {
  if (obj->event_model_ == NPEventModelCarbon) {
    EventRecord* theEvent = static_cast<EventRecord*>(event);
    return HandleCarbonEvent(theEvent, instance) ? 1 : 0;
  } else if (obj->event_model_ == NPEventModelCocoa){
    return HandleCocoaEvent(instance, (NPCocoaEvent*)event, true) ? 1 : 0;
  }
  return 0;
}

}  //  namespace o3d
