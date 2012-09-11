// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#define STRSAFE_NO_DEPRECATE
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "webkit/plugins/npapi/test/plugin_windowless_test.h"
#include "webkit/plugins/npapi/test/plugin_client.h"

#if defined(TOOLKIT_GTK)
#include <gdk/gdkx.h>
#endif

namespace NPAPIClient {

// Remember the first plugin instance for tests involving multiple instances
WindowlessPluginTest* g_other_instance = NULL;

void OnFinishTest(void* data) {
  static_cast<WindowlessPluginTest*>(data)->SignalTestCompleted();
}

WindowlessPluginTest::WindowlessPluginTest(NPP id,
                                           NPNetscapeFuncs *host_functions)
    : PluginTest(id, host_functions),
      paint_counter_(0) {
  if (!g_other_instance)
    g_other_instance = this;
}

static bool IsPaintEvent(NPEvent* np_event) {
#if defined(OS_WIN)
  return WM_PAINT == np_event->event;
#elif defined(OS_MACOSX)
  NPCocoaEvent* cocoa_event = reinterpret_cast<NPCocoaEvent*>(np_event);
  return cocoa_event->type == NPCocoaEventDrawRect;
#elif defined(TOOLKIT_GTK)
  return np_event->type == GraphicsExpose;
#else
  NOTIMPLEMENTED();
  return false;
#endif
}

static bool IsMouseUpEvent(NPEvent* np_event) {
#if defined(OS_WIN)
  return WM_LBUTTONUP == np_event->event;
#elif defined(OS_MACOSX)
  NPCocoaEvent* cocoa_event = reinterpret_cast<NPCocoaEvent*>(np_event);
  return cocoa_event->type == NPCocoaEventMouseUp;
#else
  NOTIMPLEMENTED();
  return false;
#endif
}

#if defined(OS_MACOSX)
static bool IsWindowActivationEvent(NPEvent* np_event) {
  NPCocoaEvent* cocoa_event = reinterpret_cast<NPCocoaEvent*>(np_event);
  return cocoa_event->type == NPCocoaEventWindowFocusChanged &&
         cocoa_event->data.focus.hasFocus;
}
#endif

bool WindowlessPluginTest::IsWindowless() const {
  return true;
}

NPError WindowlessPluginTest::New(uint16 mode, int16 argc,
                                 const char* argn[], const char* argv[],
                                 NPSavedData* saved) {
  NPError error = PluginTest::New(mode, argc, argn, argv, saved);

  if (test_name() == "invoke_js_function_on_create") {
    ExecuteScript(
        NPAPIClient::PluginClient::HostFunctions(), g_other_instance->id(),
        "PluginCreated();", NULL);
  }

  return error;
}

int16 WindowlessPluginTest::HandleEvent(void* event) {
  NPNetscapeFuncs* browser = NPAPIClient::PluginClient::HostFunctions();

  NPBool supports_windowless = 0;
  NPError result = browser->getvalue(id(), NPNVSupportsWindowless,
                                     &supports_windowless);
  if ((result != NPERR_NO_ERROR) || (!supports_windowless)) {
    SetError("Failed to read NPNVSupportsWindowless value");
    SignalTestCompleted();
    return PluginTest::HandleEvent(event);
  }

  NPEvent* np_event = reinterpret_cast<NPEvent*>(event);
  if (IsPaintEvent(np_event)) {
    paint_counter_++;
#if defined(OS_WIN)
    HDC paint_dc = reinterpret_cast<HDC>(np_event->wParam);
    if (paint_dc == NULL) {
      SetError("Invalid Window DC passed to HandleEvent for WM_PAINT");
      SignalTestCompleted();
      return NPERR_GENERIC_ERROR;
    }

    HRGN clipping_region = CreateRectRgn(0, 0, 0, 0);
    if (!GetClipRgn(paint_dc, clipping_region)) {
      SetError("No clipping region set in window DC");
      DeleteObject(clipping_region);
      SignalTestCompleted();
      return NPERR_GENERIC_ERROR;
    }

    DeleteObject(clipping_region);
#endif

    if (test_name() == "execute_script_delete_in_paint") {
      ExecuteScriptDeleteInPaint(browser);
    } else if (test_name() == "multiple_instances_sync_calls") {
      MultipleInstanceSyncCalls(browser);
    } else if (test_name() == "resize_during_paint") {
      if (paint_counter_ == 1) {
        // So that we get another paint later.
        browser->invalidaterect(id(), NULL);
      } else if (paint_counter_ == 2) {
        // Do this in the second paint since that's asynchronous. The first
        // paint will always be synchronous (since the renderer process doesn't
        // have a cache of the plugin yet). If we try calling NPN_Evaluate while
        // WebKit is painting, it will assert since style recalc is happening
        // during painting.
        ExecuteScriptResizeInPaint(browser);

        // So that we can exit the test after the message loop is unrolled.
        browser->pluginthreadasynccall(id(), OnFinishTest, this);
      }
    }
#if defined(OS_MACOSX)
  } else if (IsWindowActivationEvent(np_event) &&
             test_name() == "convert_point") {
      ConvertPoint(browser);
#endif
  } else if (IsMouseUpEvent(np_event) &&
             test_name() == "execute_script_delete_in_mouse_up") {
    ExecuteScript(browser, id(), "DeletePluginWithinScript();", NULL);
    SignalTestCompleted();
  } else if (IsMouseUpEvent(np_event) &&
             test_name() == "delete_frame_test") {
    ExecuteScript(
        browser, id(),
        "parent.document.getElementById('frame').outerHTML = ''", NULL);
  }
  // If this test failed, then we'd have crashed by now.
  return PluginTest::HandleEvent(event);
}

NPError WindowlessPluginTest::ExecuteScript(NPNetscapeFuncs* browser, NPP id,
    const std::string& script, NPVariant* result) {
  std::string script_url = "javascript:";
  script_url += script;

  size_t script_length = script_url.length();
  if (script_length != static_cast<uint32_t>(script_length)) {
    return NPERR_GENERIC_ERROR;
  }

  NPString script_string = { script_url.c_str(),
                             static_cast<uint32_t>(script_length) };
  NPObject *window_obj = NULL;
  browser->getvalue(id, NPNVWindowNPObject, &window_obj);

  NPVariant unused_result;
  if (!result)
    result = &unused_result;

  return browser->evaluate(id, window_obj, &script_string, result);
}

void WindowlessPluginTest::ExecuteScriptDeleteInPaint(
    NPNetscapeFuncs* browser) {
  const NPUTF8* urlString = "javascript:DeletePluginWithinScript()";
  const NPUTF8* targetString = NULL;
  browser->geturl(id(), urlString, targetString);
  SignalTestCompleted();
}

void WindowlessPluginTest::ExecuteScriptResizeInPaint(
    NPNetscapeFuncs* browser) {
  ExecuteScript(browser, id(), "ResizePluginWithinScript();", NULL);
}

void WindowlessPluginTest::MultipleInstanceSyncCalls(NPNetscapeFuncs* browser) {
  if (this == g_other_instance)
    return;

  DCHECK(g_other_instance);
  ExecuteScript(browser, g_other_instance->id(), "TestCallback();", NULL);
  SignalTestCompleted();
}

#if defined(OS_MACOSX)
std::string StringForPoint(int x, int y) {
  std::string point_string("(");
  point_string.append(base::IntToString(x));
  point_string.append(", ");
  point_string.append(base::IntToString(y));
  point_string.append(")");
  return point_string;
}
#endif

void WindowlessPluginTest::ConvertPoint(NPNetscapeFuncs* browser) {
#if defined(OS_MACOSX)
  // First, just sanity-test that round trips work.
  NPCoordinateSpace spaces[] = { NPCoordinateSpacePlugin,
                                 NPCoordinateSpaceWindow,
                                 NPCoordinateSpaceFlippedWindow,
                                 NPCoordinateSpaceScreen,
                                 NPCoordinateSpaceFlippedScreen };
  for (unsigned int i = 0; i < arraysize(spaces); ++i) {
    for (unsigned int j = 0; j < arraysize(spaces); ++j) {
      double x, y, round_trip_x, round_trip_y;
      if (!(browser->convertpoint(id(), 0, 0, spaces[i], &x, &y, spaces[j])) ||
          !(browser->convertpoint(id(), x, y, spaces[j], &round_trip_x,
                                  &round_trip_y, spaces[i]))) {
        SetError("Conversion failed");
        SignalTestCompleted();
        return;
      }
      if (i != j && x == 0 && y == 0) {
        SetError("Converting a coordinate should change it");
        SignalTestCompleted();
        return;
      }
      if (round_trip_x != 0 || round_trip_y != 0) {
        SetError("Round-trip conversion should return the original point");
        SignalTestCompleted();
        return;
      }
    }
  }

  // Now, more extensive testing on a single point.
  double screen_x, screen_y;
  browser->convertpoint(id(), 0, 0, NPCoordinateSpacePlugin,
                        &screen_x, &screen_y, NPCoordinateSpaceScreen);
  double flipped_screen_x, flipped_screen_y;
  browser->convertpoint(id(), 0, 0, NPCoordinateSpacePlugin,
                        &flipped_screen_x, &flipped_screen_y,
                        NPCoordinateSpaceFlippedScreen);
  double window_x, window_y;
  browser->convertpoint(id(), 0, 0, NPCoordinateSpacePlugin,
                        &window_x, &window_y, NPCoordinateSpaceWindow);
  double flipped_window_x, flipped_window_y;
  browser->convertpoint(id(), 0, 0, NPCoordinateSpacePlugin,
                        &flipped_window_x, &flipped_window_y,
                        NPCoordinateSpaceFlippedWindow);

  CGRect main_display_bounds = CGDisplayBounds(CGMainDisplayID());

  // Check that all the coordinates are right. The constants below are based on
  // the window frame set in the UI test and the content offset in the test
  // html. Y-coordinates are not checked exactly so that the test is robust
  // against toolbar changes, info and bookmark bar visibility, etc.
  const int kWindowHeight = 400;
  const int kWindowXOrigin = 50;
  const int kWindowYOrigin = 50;
  const int kPluginXContentOffset = 50;
  const int kPluginYContentOffset = 50;
  const int kChromeYTolerance = 200;

  std::string error_string;
  if (screen_x != flipped_screen_x)
    error_string = "Flipping screen coordinates shouldn't change x";
  else if (flipped_screen_y != main_display_bounds.size.height - screen_y)
    error_string = "Flipped screen coordinates should be flipped vertically";
  else if (screen_x != kWindowXOrigin + kPluginXContentOffset)
    error_string = "Screen x location is wrong";
  else if (flipped_screen_y < kWindowYOrigin + kPluginYContentOffset ||
           flipped_screen_y > kWindowYOrigin + kPluginYContentOffset +
                              kChromeYTolerance)
    error_string = "Screen y location is wrong";
  else if (window_x != flipped_window_x)
    error_string = "Flipping window coordinates shouldn't change x";
  else if (flipped_window_y != kWindowHeight - window_y)
    error_string = "Flipped window coordinates should be flipped vertically";
  else if (window_x != kPluginXContentOffset)
    error_string = "Window x location is wrong";
  else if (flipped_window_y < kPluginYContentOffset ||
           flipped_window_y > kPluginYContentOffset + kChromeYTolerance)
    error_string = "Window y location is wrong";

  if (!error_string.empty()) {
    error_string.append(" - ");
    error_string.append(StringForPoint(screen_x, screen_y));
    error_string.append(" - ");
    error_string.append(StringForPoint(flipped_screen_x, flipped_screen_y));
    error_string.append(" - ");
    error_string.append(StringForPoint(window_x, window_y));
    error_string.append(" - ");
    error_string.append(StringForPoint(flipped_window_x, flipped_window_y));
    SetError(error_string);
  }
#else
  SetError("Unimplemented");
#endif
  SignalTestCompleted();
}

} // namespace NPAPIClient
