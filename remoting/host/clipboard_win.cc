// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/clipboard.h"

#include <windows.h>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/process_util.h"
#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "base/win/wrapped_window_proc.h"
#include "remoting/base/constants.h"
#include "remoting/proto/event.pb.h"

namespace {

const WCHAR kWindowClassName[] = L"clipboardWindowClass";
const WCHAR kWindowName[] = L"clipboardWindow";

// A scoper class that opens and closes the clipboard.
// This class was adapted from the ScopedClipboard class in
// ui/base/clipboard/clipboard_win.cc.
class ScopedClipboard {
 public:
  ScopedClipboard() : opened_(false) {
  }

  ~ScopedClipboard() {
    if (opened_) {
      ::CloseClipboard();
    }
  }

  bool Init(HWND owner) {
    const int kMaxAttemptsToOpenClipboard = 5;
    const base::TimeDelta kSleepTimeBetweenAttempts =
        base::TimeDelta::FromMilliseconds(5);

    if (opened_) {
      NOTREACHED();
      return true;
    }

    // This code runs on the desktop thread, so blocking briefly is acceptable.
    for (int attempt = 0; attempt < kMaxAttemptsToOpenClipboard; ++attempt) {
      if (attempt > 0) {
        base::PlatformThread::Sleep(kSleepTimeBetweenAttempts);
      }
      if (::OpenClipboard(owner)) {
        opened_ = true;
        return true;
      }
    }
    return false;
  }

  BOOL Empty() {
    if (!opened_) {
      NOTREACHED();
      return false;
    }
    return ::EmptyClipboard();
  }

  void SetData(UINT uFormat, HANDLE hMem) {
    if (!opened_) {
      NOTREACHED();
      return;
    }
    // The caller must not close the handle that ::SetClipboardData returns.
    ::SetClipboardData(uFormat, hMem);
  }

 private:
  bool opened_;
};

}  // namespace

namespace remoting {

class ClipboardWin : public Clipboard {
 public:
  ClipboardWin();

  virtual void Start() OVERRIDE;
  virtual void InjectClipboardEvent(
      const protocol::ClipboardEvent& event) OVERRIDE;
  virtual void Stop() OVERRIDE;

 private:
  static LRESULT CALLBACK WndProc(HWND hwmd, UINT msg, WPARAM wParam,
                                  LPARAM lParam);

  static bool RegisterWindowClass();

  HWND hwnd_;

  DISALLOW_COPY_AND_ASSIGN(ClipboardWin);
};

ClipboardWin::ClipboardWin() : hwnd_(NULL) {
}

void ClipboardWin::Start() {
  if (!RegisterWindowClass()) {
    LOG(FATAL) << "Couldn't register clipboard window class.";
    return;
  }
  hwnd_ = ::CreateWindow(kWindowClassName,
                         kWindowName,
                         0, 0, 0, 0, 0,
                         HWND_MESSAGE,
                         NULL,
                         base::GetModuleFromAddress(&WndProc),
                         this);
  if (!hwnd_) {
    LOG(FATAL) << "Couldn't create clipboard window.";
    return;
  }
}

void ClipboardWin::Stop() {
  if (hwnd_) {
    ::DestroyWindow(hwnd_);
    hwnd_ = NULL;
  }
}

void ClipboardWin::InjectClipboardEvent(
    const protocol::ClipboardEvent& event) {
  if (!hwnd_) {
    return;
  }
  // Currently we only handle UTF-8 text.
  if (event.mime_type().compare(kMimeTypeTextUtf8)) {
    return;
  }
  string16 text = UTF8ToUTF16(event.data());

  ScopedClipboard clipboard;
  if (!clipboard.Init(hwnd_)) {
    LOG(WARNING) << "Couldn't open the clipboard.";
    return;
  }

  clipboard.Empty();

  HGLOBAL text_global =
      ::GlobalAlloc(GMEM_MOVEABLE, (text.size() + 1) * sizeof(WCHAR));
  if (!text_global) {
    LOG(WARNING) << "Couldn't allocate global memory.";
    return;
  }

  LPWSTR text_global_locked =
      reinterpret_cast<LPWSTR>(::GlobalLock(text_global));
  memcpy(text_global_locked, text.data(), text.size() * sizeof(WCHAR));
  text_global_locked[text.size()] = (WCHAR)0;
  ::GlobalUnlock(text_global);

  clipboard.SetData(CF_UNICODETEXT, text_global);
}

LRESULT CALLBACK ClipboardWin::WndProc(HWND hwnd, UINT msg, WPARAM wParam,
                                       LPARAM lParam) {
  return ::DefWindowProc(hwnd, msg, wParam, lParam);
}

bool ClipboardWin::RegisterWindowClass() {
  // This method is only called on the desktop thread, so it doesn't matter
  // that the following test is not thread-safe.
  static bool registered = false;
  if (registered) {
    return true;
  }

  WNDCLASSEX window_class;
  base::win::InitializeWindowClass(
      kWindowClassName,
      base::win::WrappedWindowProc<WndProc>,
      0, 0, 0, NULL, NULL, NULL, NULL, NULL,
      &window_class);
  if (!::RegisterClassEx(&window_class)) {
    return false;
  }

  registered = true;
  return true;
}

scoped_ptr<Clipboard> Clipboard::Create() {
  return scoped_ptr<Clipboard>(new ClipboardWin());
}

}  // namespace remoting
