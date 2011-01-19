// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/win/window_impl.h"

#include <list>

#include "ui/base/win/hwnd_util.h"
#include "base/singleton.h"
#include "base/string_number_conversions.h"

namespace ui {

static const DWORD kWindowDefaultChildStyle =
    WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
static const DWORD kWindowDefaultStyle = WS_OVERLAPPEDWINDOW;
static const DWORD kWindowDefaultExStyle = 0;

///////////////////////////////////////////////////////////////////////////////
// WindowImpl class tracking.

// Several external scripts rely explicitly on this base class name for
// acquiring the window handle and will break if this is modified!
// static
const wchar_t* const WindowImpl::kBaseClassName = L"Chrome_WidgetWin_";

// WindowImpl class information used for registering unique windows.
struct ClassInfo {
  UINT style;
  HBRUSH background;

  explicit ClassInfo(int style)
      : style(style),
        background(NULL) {}

  // Compares two ClassInfos. Returns true if all members match.
  bool Equals(const ClassInfo& other) const {
    return (other.style == style && other.background == background);
  }
};

class ClassRegistrar {
 public:
  static ClassRegistrar* GetInstance() {
    return Singleton<ClassRegistrar>::get();
  }

  ~ClassRegistrar() {
    for (RegisteredClasses::iterator i = registered_classes_.begin();
         i != registered_classes_.end(); ++i) {
      UnregisterClass(i->name.c_str(), NULL);
    }
  }

  // Puts the name for the class matching |class_info| in |class_name|, creating
  // a new name if the class is not yet known.
  // Returns true if this class was already known, false otherwise.
  bool RetrieveClassName(const ClassInfo& class_info, std::wstring* name) {
    for (RegisteredClasses::const_iterator i = registered_classes_.begin();
         i != registered_classes_.end(); ++i) {
      if (class_info.Equals(i->info)) {
        name->assign(i->name);
        return true;
      }
    }

    name->assign(string16(WindowImpl::kBaseClassName) +
        base::IntToString16(registered_count_++));
    return false;
  }

  void RegisterClass(const ClassInfo& class_info,
                     const std::wstring& name,
                     ATOM atom) {
    registered_classes_.push_back(RegisteredClass(class_info, name, atom));
  }

 private:
  // Represents a registered window class.
  struct RegisteredClass {
    RegisteredClass(const ClassInfo& info,
                    const std::wstring& name,
                    ATOM atom)
        : info(info),
          name(name),
          atom(atom) {
    }

    // Info used to create the class.
    ClassInfo info;

    // The name given to the window.
    std::wstring name;

    // The ATOM returned from creating the window.
    ATOM atom;
  };

  ClassRegistrar() : registered_count_(0) { }
  friend struct DefaultSingletonTraits<ClassRegistrar>;

  typedef std::list<RegisteredClass> RegisteredClasses;
  RegisteredClasses registered_classes_;

  // Counter of how many classes have been registered so far.
  int registered_count_;

  DISALLOW_COPY_AND_ASSIGN(ClassRegistrar);
};

///////////////////////////////////////////////////////////////////////////////
// WindowImpl, public

WindowImpl::WindowImpl()
    : window_style_(0),
      window_ex_style_(kWindowDefaultExStyle),
      class_style_(CS_DBLCLKS),
      hwnd_(NULL) {
}

WindowImpl::~WindowImpl() {
}

void WindowImpl::Init(HWND parent, const gfx::Rect& bounds) {
  if (window_style_ == 0)
    window_style_ = parent ? kWindowDefaultChildStyle : kWindowDefaultStyle;

  // Ensures the parent we have been passed is valid, otherwise CreateWindowEx
  // will fail.
  if (parent && !::IsWindow(parent)) {
    NOTREACHED() << "invalid parent window specified.";
    parent = NULL;
  }

  int x, y, width, height;
  if (bounds.IsEmpty()) {
    x = y = width = height = CW_USEDEFAULT;
  } else {
    x = bounds.x();
    y = bounds.y();
    width = bounds.width();
    height = bounds.height();
  }

  hwnd_ = CreateWindowEx(window_ex_style_, GetWindowClassName().c_str(), NULL,
                         window_style_, x, y, width, height,
                         parent, NULL, NULL, this);
  DCHECK(hwnd_);

  // The window procedure should have set the data for us.
  DCHECK(ui::GetWindowUserData(hwnd_) == this);
}

HICON WindowImpl::GetDefaultWindowIcon() const {
  return NULL;
}

// static
bool WindowImpl::IsWindowImpl(HWND hwnd) {
  wchar_t tmp[128];
  if (!::GetClassName(hwnd, tmp, 128))
    return false;

  std::wstring class_name(tmp);
  return class_name.find(kBaseClassName) == 0;
}

LRESULT WindowImpl::OnWndProc(UINT message, WPARAM w_param, LPARAM l_param) {
  LRESULT result = 0;

  // Handle the message if it's in our message map; otherwise, let the system
  // handle it.
  if (!ProcessWindowMessage(hwnd_, message, w_param, l_param, result))
    result = DefWindowProc(hwnd_, message, w_param, l_param);

  return result;
}

// static
LRESULT CALLBACK WindowImpl::WndProc(HWND hwnd,
                                     UINT message,
                                     WPARAM w_param,
                                     LPARAM l_param) {
  if (message == WM_NCCREATE) {
    CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(l_param);
    WindowImpl* window = reinterpret_cast<WindowImpl*>(cs->lpCreateParams);
    DCHECK(window);
    ui::SetWindowUserData(hwnd, window);
    window->hwnd_ = hwnd;
    return TRUE;
  }

  WindowImpl* window = reinterpret_cast<WindowImpl*>(
      ui::GetWindowUserData(hwnd));
  if (!window)
    return 0;

  return window->OnWndProc(message, w_param, l_param);
}

std::wstring WindowImpl::GetWindowClassName() {
  ClassInfo class_info(initial_class_style());
  std::wstring name;
  if (ClassRegistrar::GetInstance()->RetrieveClassName(class_info, &name))
    return name;

  // No class found, need to register one.
  WNDCLASSEX class_ex;
  class_ex.cbSize = sizeof(WNDCLASSEX);
  class_ex.style = class_info.style;
  class_ex.lpfnWndProc = &WindowImpl::WndProc;
  class_ex.cbClsExtra = 0;
  class_ex.cbWndExtra = 0;
  class_ex.hInstance = NULL;
  class_ex.hIcon = GetDefaultWindowIcon();
  class_ex.hCursor = LoadCursor(NULL, IDC_ARROW);
  class_ex.hbrBackground = reinterpret_cast<HBRUSH>(class_info.background + 1);
  class_ex.lpszMenuName = NULL;
  class_ex.lpszClassName = name.c_str();
  class_ex.hIconSm = class_ex.hIcon;
  ATOM atom = RegisterClassEx(&class_ex);
  DCHECK(atom);

  ClassRegistrar::GetInstance()->RegisterClass(class_info, name, atom);

  return name;
}

}  // namespace ui
