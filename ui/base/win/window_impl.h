// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_WIN_WINDOW_IMPL_H_
#define UI_BASE_WIN_WINDOW_IMPL_H_
#pragma once

#include <atlbase.h>
#include <atlapp.h>
#include <atlmisc.h>
#include <atlcrack.h>

#include <string>

#include "base/logging.h"
#include "gfx/native_widget_types.h"
#include "gfx/rect.h"

namespace ui {

// An interface implemented by classes that use message maps.
// ProcessWindowMessage is implemented by the BEGIN_MESSAGE_MAP_EX macro.
class MessageMapInterface {
 public:
  // Processes one message from the window's message queue.
  virtual BOOL ProcessWindowMessage(HWND window,
                                    UINT message,
                                    WPARAM w_param,
                                    LPARAM l_param,
                                    LRESULT& result,
                                    DWORD msg_mad_id = 0) = 0;
};

///////////////////////////////////////////////////////////////////////////////
//
// WindowImpl
//  A convenience class that encapsulates the details of creating and
//  destroying a HWND.  This class also hosts the windows procedure used by all
//  Windows.
//
///////////////////////////////////////////////////////////////////////////////
class WindowImpl : public MessageMapInterface {
 public:
  WindowImpl();
  virtual ~WindowImpl();

  // Initializes the Window with a parent and an initial desired size.
  void Init(HWND parent, const gfx::Rect& bounds);

  // Retrieves the default window icon to use for windows if none is specified.
  virtual HICON GetDefaultWindowIcon() const;

  // Returns the HWND associated with this Window.
  HWND hwnd() const { return hwnd_; }

  // Sets the window styles. This is ONLY used when the window is created.
  // In other words, if you invoke this after invoking Init, nothing happens.
  void set_window_style(DWORD style) { window_style_ = style; }
  DWORD window_style() const { return window_style_; }

  // Sets the extended window styles. See comment about |set_window_style|.
  void set_window_ex_style(DWORD style) { window_ex_style_ = style; }
  DWORD window_ex_style() const { return window_ex_style_; }

  // Sets the class style to use. The default is CS_DBLCLKS.
  void set_initial_class_style(UINT class_style) {
    // We dynamically generate the class name, so don't register it globally!
    DCHECK_EQ((class_style & CS_GLOBALCLASS), 0u);
    class_style_ = class_style;
  }
  UINT initial_class_style() const { return class_style_; }

  // Returns true if the specified |hwnd| is a WindowImpl.
  static bool IsWindowImpl(HWND hwnd);

 protected:
  // Handles the WndProc callback for this object.
  virtual LRESULT OnWndProc(UINT message, WPARAM w_param, LPARAM l_param);

 private:
  friend class ClassRegistrar;

  // The window procedure used by all Windows.
  static LRESULT CALLBACK WndProc(HWND window,
                                  UINT message,
                                  WPARAM w_param,
                                  LPARAM l_param);

  // Gets the window class name to use when creating the corresponding HWND.
  // If necessary, this registers the window class.
  std::wstring GetWindowClassName();

  // All classes registered by WidgetWin start with this name.
  static const wchar_t* const kBaseClassName;

  // Window Styles used when creating the window.
  DWORD window_style_;

  // Window Extended Styles used when creating the window.
  DWORD window_ex_style_;

  // Style of the class to use.
  UINT class_style_;

  // Our hwnd.
  HWND hwnd_;

  DISALLOW_COPY_AND_ASSIGN(WindowImpl);
};

}  // namespace ui

#endif  // UI_BASE_WIN_WINDOW_IMPL_H_
