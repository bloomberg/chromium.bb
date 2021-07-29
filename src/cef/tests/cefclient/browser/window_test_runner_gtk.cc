// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/cefclient/browser/window_test_runner_gtk.h"

#include <gtk/gtk.h>

#include "include/wrapper/cef_helpers.h"
#include "tests/cefclient/browser/root_window.h"
#include "tests/cefclient/browser/util_gtk.h"
#include "tests/shared/browser/main_message_loop.h"

namespace client {
namespace window_test {

namespace {

GtkWindow* GetWindow(CefRefPtr<CefBrowser> browser) {
  scoped_refptr<RootWindow> root_window =
      RootWindow::GetForBrowser(browser->GetIdentifier());
  if (root_window) {
    GtkWindow* window = GTK_WINDOW(root_window->GetWindowHandle());
    if (!window)
      LOG(ERROR) << "No GtkWindow for browser";
    return window;
  }
  return nullptr;
}

bool IsMaximized(GtkWindow* window) {
  GdkWindow* gdk_window = gtk_widget_get_window(GTK_WIDGET(window));
  gint state = gdk_window_get_state(gdk_window);
  return (state & GDK_WINDOW_STATE_MAXIMIZED) ? true : false;
}

void SetPosImpl(CefRefPtr<CefBrowser> browser,
                int x,
                int y,
                int width,
                int height) {
  REQUIRE_MAIN_THREAD();
  ScopedGdkThreadsEnter scoped_gdk_threads;

  GtkWindow* window = GetWindow(browser);
  if (!window)
    return;
  GdkWindow* gdk_window = gtk_widget_get_window(GTK_WIDGET(window));

  // Make sure the window isn't minimized or maximized.
  if (IsMaximized(window))
    gtk_window_unmaximize(window);
  else
    gtk_window_present(window);

  // Retrieve information about the display that contains the window.
  GdkScreen* screen = gdk_screen_get_default();
  gint monitor = gdk_screen_get_monitor_at_window(screen, gdk_window);
  GdkRectangle rect;
  gdk_screen_get_monitor_geometry(screen, monitor, &rect);

  // Make sure the window is inside the display.
  CefRect display_rect(rect.x, rect.y, rect.width, rect.height);
  CefRect window_rect(x, y, width, height);
  WindowTestRunner::ModifyBounds(display_rect, window_rect);

  gdk_window_move_resize(gdk_window, window_rect.x, window_rect.y,
                         window_rect.width, window_rect.height);
}

void MinimizeImpl(CefRefPtr<CefBrowser> browser) {
  REQUIRE_MAIN_THREAD();
  ScopedGdkThreadsEnter scoped_gdk_threads;

  GtkWindow* window = GetWindow(browser);
  if (!window)
    return;

  // Unmaximize the window before minimizing so restore behaves correctly.
  if (IsMaximized(window))
    gtk_window_unmaximize(window);

  gtk_window_iconify(window);
}

void MaximizeImpl(CefRefPtr<CefBrowser> browser) {
  REQUIRE_MAIN_THREAD();
  ScopedGdkThreadsEnter scoped_gdk_threads;

  GtkWindow* window = GetWindow(browser);
  if (!window)
    return;
  gtk_window_maximize(window);
}

void RestoreImpl(CefRefPtr<CefBrowser> browser) {
  REQUIRE_MAIN_THREAD();
  ScopedGdkThreadsEnter scoped_gdk_threads;

  GtkWindow* window = GetWindow(browser);
  if (!window)
    return;
  if (IsMaximized(window))
    gtk_window_unmaximize(window);
  else
    gtk_window_present(window);
}

}  // namespace

WindowTestRunnerGtk::WindowTestRunnerGtk() {}

void WindowTestRunnerGtk::SetPos(CefRefPtr<CefBrowser> browser,
                                 int x,
                                 int y,
                                 int width,
                                 int height) {
  MAIN_POST_CLOSURE(base::BindOnce(SetPosImpl, browser, x, y, width, height));
}

void WindowTestRunnerGtk::Minimize(CefRefPtr<CefBrowser> browser) {
  MAIN_POST_CLOSURE(base::BindOnce(MinimizeImpl, browser));
}

void WindowTestRunnerGtk::Maximize(CefRefPtr<CefBrowser> browser) {
  MAIN_POST_CLOSURE(base::BindOnce(MaximizeImpl, browser));
}

void WindowTestRunnerGtk::Restore(CefRefPtr<CefBrowser> browser) {
  MAIN_POST_CLOSURE(base::BindOnce(RestoreImpl, browser));
}

}  // namespace window_test
}  // namespace client
