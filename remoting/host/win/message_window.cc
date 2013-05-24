// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/win/message_window.h"

#include "base/logging.h"
#include "base/process_util.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "base/win/wrapped_window_proc.h"

const char kClassNameFormat[] = "Chromoting_MessageWindow_%p";

namespace remoting {
namespace win {

MessageWindow::MessageWindow()
    : atom_(0),
      instance_(NULL),
      window_(NULL) {
  class_name_ = base::StringPrintf(kClassNameFormat, this);
  instance_ = base::GetModuleFromAddress(static_cast<WNDPROC>(
      &base::win::WrappedWindowProc<WindowProc>));
}

MessageWindow::MessageWindow(const std::string& class_name, HINSTANCE instance)
    : atom_(0),
      class_name_(class_name),
      instance_(instance),
      window_(NULL) {
}

MessageWindow::~MessageWindow() {
  DCHECK(CalledOnValidThread());

  if (window_ != NULL) {
    DestroyWindow(window_);
    window_ = NULL;
  }

  if (atom_ != 0) {
    UnregisterClass(MAKEINTATOM(atom_), instance_);
    atom_ = 0;
  }
}

bool MessageWindow::Create(Delegate* delegate) {
  DCHECK(CalledOnValidThread());
  DCHECK(!atom_);
  DCHECK(!window_);

  // Register a separate window class for each instance of |MessageWindow|.
  string16 class_name = UTF8ToUTF16(class_name_);
  WNDCLASSEX window_class;
  window_class.cbSize = sizeof(window_class);
  window_class.style = 0;
  window_class.lpfnWndProc = &base::win::WrappedWindowProc<WindowProc>;
  window_class.cbClsExtra = 0;
  window_class.cbWndExtra = 0;
  window_class.hInstance = instance_;
  window_class.hIcon = NULL;
  window_class.hCursor = NULL;
  window_class.hbrBackground = NULL;
  window_class.lpszMenuName = NULL;
  window_class.lpszClassName = class_name.c_str();
  window_class.hIconSm = NULL;
  atom_ = RegisterClassEx(&window_class);
  if (atom_ == 0) {
    LOG_GETLASTERROR(ERROR)
        << "Failed to register the window class '" << class_name_ << "'";
    return false;
  }

  window_ = CreateWindow(MAKEINTATOM(atom_), 0, 0, 0, 0, 0, 0, HWND_MESSAGE, 0,
                         instance_, delegate);
  if (!window_) {
    LOG_GETLASTERROR(ERROR) << "Failed to create a message-only window";
    return false;
  }

  return true;
}

// static
LRESULT CALLBACK MessageWindow::WindowProc(HWND hwnd,
                                           UINT message,
                                           WPARAM wparam,
                                           LPARAM lparam) {
  Delegate* delegate = NULL;

  // Set up the delegate before handling WM_CREATE.
  if (message == WM_CREATE) {
    CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lparam);
    delegate = reinterpret_cast<Delegate*>(cs->lpCreateParams);

    // Store pointer to the delegate to the window's user data.
    SetLastError(ERROR_SUCCESS);
    LONG_PTR result = SetWindowLongPtr(
        hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(delegate));
    CHECK(result != 0 || GetLastError() == ERROR_SUCCESS);
  } else {
    delegate = reinterpret_cast<Delegate*>(GetWindowLongPtr(hwnd,
                                                            GWLP_USERDATA));
  }

  // Handle the message.
  if (delegate) {
    LRESULT message_result;
    if (delegate->HandleMessage(hwnd, message, wparam, lparam, &message_result))
      return message_result;
  }

  return DefWindowProc(hwnd, message, wparam, lparam);
}

}  // namespace win
}  // namespace remoting
