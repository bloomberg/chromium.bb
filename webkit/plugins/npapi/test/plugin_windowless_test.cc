// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#define STRSAFE_NO_DEPRECATE
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "webkit/plugins/npapi/test/plugin_windowless_test.h"
#include "webkit/plugins/npapi/test/plugin_client.h"

#if defined(OS_MACOSX)
#include <ApplicationServices/ApplicationServices.h>
#include <Carbon/Carbon.h>
#endif

namespace NPAPIClient {

// Remember the first plugin instance for tests involving multiple instances
WindowlessPluginTest* g_other_instance = NULL;

WindowlessPluginTest::WindowlessPluginTest(NPP id,
                                           NPNetscapeFuncs *host_functions)
    : PluginTest(id, host_functions) {
  if (!g_other_instance)
    g_other_instance = this;
}

static bool IsPaintEvent(NPEvent* np_event) {
#if defined(OS_WIN)
  return WM_PAINT == np_event->event;
#elif defined(OS_MACOSX)
  return np_event->what == updateEvt;
#endif
}

static bool IsMouseMoveEvent(NPEvent* np_event) {
#if defined(OS_WIN)
  return WM_MOUSEMOVE == np_event->event;
#elif defined(OS_MACOSX)
  return np_event->what == nullEvent;
#endif
}

static bool IsMouseUpEvent(NPEvent* np_event) {
#if defined(OS_WIN)
  return WM_LBUTTONUP == np_event->event;
#elif defined(OS_MACOSX)
  return np_event->what == mouseUp;
#endif
}

static bool IsWindowActivationEvent(NPEvent* np_event) {
#if defined(OS_WIN)
  NOTIMPLEMENTED();
  return false;
#elif defined(OS_MACOSX)
  return np_event->what == activateEvt;
#endif
}

bool WindowlessPluginTest::IsWindowless() const {
  return true;
}

int16 WindowlessPluginTest::HandleEvent(void* event) {

  NPNetscapeFuncs* browser = NPAPIClient::PluginClient::HostFunctions();

  NPBool supports_windowless = 0;
  NPError result = browser->getvalue(id(), NPNVSupportsWindowless,
                                     &supports_windowless);
  if ((result != NPERR_NO_ERROR) || (supports_windowless != TRUE)) {
    SetError("Failed to read NPNVSupportsWindowless value");
    SignalTestCompleted();
    return PluginTest::HandleEvent(event);
  }

  NPEvent* np_event = reinterpret_cast<NPEvent*>(event);
  if (IsPaintEvent(np_event)) {
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
    }
#if OS_MACOSX
  } else if (IsWindowActivationEvent(np_event) &&
             test_name() == "convert_point") {
      ConvertPoint(browser);
#endif
  } else if (IsMouseMoveEvent(np_event) &&
             test_name() == "execute_script_delete_in_mouse_move") {
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

  NPString script_string = { script_url.c_str(), script_url.length() };
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
