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
#include "base/win/scoped_hglobal.h"
#include "base/win/windows_version.h"
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

    // This code runs on the UI thread, so we can block only very briefly.
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

  // The caller must not free the handle. The caller should lock the handle,
  // copy the clipboard data, and unlock the handle. All this must be done
  // before this ScopedClipboard is destroyed.
  HANDLE GetData(UINT format) {
    if (!opened_) {
      NOTREACHED();
      return NULL;
    }
    return ::GetClipboardData(format);
  }

 private:
  bool opened_;
};

typedef BOOL (WINAPI AddClipboardFormatListenerFn)(HWND);
typedef BOOL (WINAPI RemoveClipboardFormatListenerFn)(HWND);

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
  void OnClipboardUpdate();
  bool HaveClipboardListenerApi();

  static bool RegisterWindowClass();
  static LRESULT CALLBACK WndProc(HWND hwmd, UINT msg, WPARAM wParam,
                                  LPARAM lParam);

  HWND hwnd_;
  AddClipboardFormatListenerFn* add_clipboard_format_listener_;
  RemoveClipboardFormatListenerFn* remove_clipboard_format_listener_;
  bool load_functions_tried_;

  DISALLOW_COPY_AND_ASSIGN(ClipboardWin);
};

ClipboardWin::ClipboardWin()
    : hwnd_(NULL),
      add_clipboard_format_listener_(NULL),
      remove_clipboard_format_listener_(NULL),
      load_functions_tried_(false) {
}

void ClipboardWin::Start() {
  if (!load_functions_tried_) {
    load_functions_tried_ = true;
    HMODULE user32_module = ::GetModuleHandle(L"user32.dll");
    if (!user32_module) {
      LOG(WARNING) << "Couldn't find user32.dll.";
    } else {
      add_clipboard_format_listener_ =
          reinterpret_cast<AddClipboardFormatListenerFn*>(
              ::GetProcAddress(user32_module, "AddClipboardFormatListener"));
      remove_clipboard_format_listener_ =
          reinterpret_cast<RemoveClipboardFormatListenerFn*>(
              ::GetProcAddress(user32_module, "RemoveClipboardFormatListener"));
      if (!HaveClipboardListenerApi()) {
        LOG(WARNING) << "Couldn't load AddClipboardFormatListener or "
                     << "RemoveClipboardFormatListener.";
      }
    }
  }

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

  if (HaveClipboardListenerApi()) {
    if (!(*add_clipboard_format_listener_)(hwnd_)) {
      LOG(WARNING) << "AddClipboardFormatListener() failed: " << GetLastError();
    }
  }
}

void ClipboardWin::Stop() {
  if (hwnd_) {
    if (HaveClipboardListenerApi()) {
      (*remove_clipboard_format_listener_)(hwnd_);
    }
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

void ClipboardWin::OnClipboardUpdate() {
  DCHECK(hwnd_);

  if (::IsClipboardFormatAvailable(CF_UNICODETEXT)) {
    string16 text;
    // Add a scope, so that we keep the clipboard open for as short a time as
    // possible.
    {
      ScopedClipboard clipboard;
      if (!clipboard.Init(hwnd_)) {
        LOG(WARNING) << "Couldn't open the clipboard." << GetLastError();
        return;
      }

      HGLOBAL text_global = clipboard.GetData(CF_UNICODETEXT);
      if (!text_global) {
        LOG(WARNING) << "Couldn't get data from the clipboard: "
                     << GetLastError();
        return;
      }

      base::win::ScopedHGlobal<WCHAR> text_lock(text_global);
      if (!text_lock.get()) {
        LOG(WARNING) << "Couldn't lock clipboard data: " << GetLastError();
        return;
      }
      text.assign(text_lock.get());
    }

    protocol::ClipboardEvent event;
    event.set_mime_type(kMimeTypeTextUtf8);
    event.set_data(UTF16ToUTF8(text));

    // TODO(simonmorris): Send the event to the client.
  }
}

bool ClipboardWin::HaveClipboardListenerApi() {
  return add_clipboard_format_listener_ && remove_clipboard_format_listener_;
}

bool ClipboardWin::RegisterWindowClass() {
  // This method is only called on the UI thread, so it doesn't matter
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

LRESULT CALLBACK ClipboardWin::WndProc(HWND hwnd, UINT msg, WPARAM wparam,
                                       LPARAM lparam) {
  if (msg == WM_CREATE) {
    CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lparam);
    ::SetWindowLongPtr(hwnd,
                       GWLP_USERDATA,
                       reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
    return 0;
  }
  ClipboardWin* clipboard =
      reinterpret_cast<ClipboardWin*>(::GetWindowLongPtr(hwnd, GWLP_USERDATA));
  switch (msg) {
    case WM_CLIPBOARDUPDATE:
      clipboard->OnClipboardUpdate();
      return 0;
  }
  return ::DefWindowProc(hwnd, msg, wparam, lparam);
}

scoped_ptr<Clipboard> Clipboard::Create() {
  return scoped_ptr<Clipboard>(new ClipboardWin());
}

}  // namespace remoting
