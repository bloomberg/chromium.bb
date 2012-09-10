// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/win/window_impl.h"

#include <list>

#include "base/debug/alias.h"
#include "base/memory/singleton.h"
#include "base/string_number_conversions.h"
#include "base/win/wrapped_window_proc.h"
#include "ui/base/win/hwnd_util.h"

namespace ui {

static const DWORD kWindowDefaultChildStyle =
    WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
static const DWORD kWindowDefaultStyle = WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN;
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
  HICON icon;

  ClassInfo(int style, HICON icon)
      : style(style),
        icon(icon) {}

  // Compares two ClassInfos. Returns true if all members match.
  bool Equals(const ClassInfo& other) const {
    return (other.style == style && other.icon == icon);
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
      if (!UnregisterClass(MAKEINTATOM(i->atom), i->instance)) {
        LOG(ERROR) << "Failed to unregister class " << i->name.c_str()
                   << ". Error = " << GetLastError();
      }
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
                     ATOM atom,
                     HMODULE instance) {
    registered_classes_.push_back(
        RegisteredClass(class_info, name, atom, instance));
  }

 private:
  // Represents a registered window class.
  struct RegisteredClass {
    RegisteredClass(const ClassInfo& info,
                    const std::wstring& name,
                    ATOM atom,
                    HMODULE instance)
        : info(info),
          name(name),
          atom(atom),
          instance(instance) {
    }

    // Info used to create the class.
    ClassInfo info;

    // The name given to the window class.
    std::wstring name;

    // The ATOM returned from registering the window class.
    ATOM atom;

    // The handle of the module containing the window procedure.
    HMODULE instance;
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
      hwnd_(NULL),
      got_create_(false),
      got_valid_hwnd_(false),
      destroyed_(NULL) {
}

WindowImpl::~WindowImpl() {
  if (destroyed_)
    *destroyed_ = true;
  ClearUserData();
}

void WindowImpl::Init(HWND parent, const gfx::Rect& bounds) {
  if (window_style_ == 0)
    window_style_ = parent ? kWindowDefaultChildStyle : kWindowDefaultStyle;

  if (parent == HWND_DESKTOP) {
    // Only non-child windows can have HWND_DESKTOP (0) as their parent.
    CHECK((window_style_ & WS_CHILD) == 0);
    parent = GetWindowToParentTo(false);
  } else if (parent == ::GetDesktopWindow()) {
    // Any type of window can have the "Desktop Window" as their parent.
    parent = GetWindowToParentTo(true);
  } else if (parent != HWND_MESSAGE) {
    CHECK(::IsWindow(parent));
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

  std::wstring name(GetWindowClassName());
  bool destroyed = false;
  destroyed_ = &destroyed;
  HWND hwnd = CreateWindowEx(window_ex_style_, name.c_str(), NULL,
                             window_style_, x, y, width, height,
                             parent, NULL, NULL, this);
  if (!hwnd_ && GetLastError() == 0) {
    base::debug::Alias(&destroyed);
    base::debug::Alias(&hwnd);
    bool got_create = got_create_;
    base::debug::Alias(&got_create);
    bool got_valid_hwnd = got_valid_hwnd_;
    base::debug::Alias(&got_valid_hwnd);
    WNDCLASSEX class_info;
    memset(&class_info, 0, sizeof(WNDCLASSEX));
    class_info.cbSize = sizeof(WNDCLASSEX);
    BOOL got_class = GetClassInfoEx(
        GetModuleHandle(NULL), name.c_str(), &class_info);
    base::debug::Alias(&got_class);
    bool procs_match = got_class && class_info.lpfnWndProc ==
        base::win::WrappedWindowProc<&WindowImpl::WndProc>;
    base::debug::Alias(&procs_match);
    CHECK(false);
  }
  if (!destroyed)
    destroyed_ = NULL;

  CheckWindowCreated(hwnd_);

  // The window procedure should have set the data for us.
  CHECK_EQ(this, ui::GetWindowUserData(hwnd));
}

HICON WindowImpl::GetDefaultWindowIcon() const {
  return NULL;
}

LRESULT WindowImpl::OnWndProc(UINT message, WPARAM w_param, LPARAM l_param) {
  LRESULT result = 0;

  // Handle the message if it's in our message map; otherwise, let the system
  // handle it.
  if (!ProcessWindowMessage(hwnd_, message, w_param, l_param, result))
    result = DefWindowProc(hwnd_, message, w_param, l_param);

  return result;
}

void WindowImpl::ClearUserData() {
  if (::IsWindow(hwnd_))
    ui::SetWindowUserData(hwnd_, NULL);
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
    window->got_create_ = true;
    if (hwnd)
      window->got_valid_hwnd_ = true;
    return TRUE;
  }

  WindowImpl* window = reinterpret_cast<WindowImpl*>(
      ui::GetWindowUserData(hwnd));
  if (!window)
    return 0;

  return window->OnWndProc(message, w_param, l_param);
}

std::wstring WindowImpl::GetWindowClassName() {
  HICON icon = GetDefaultWindowIcon();
  ClassInfo class_info(initial_class_style(), icon);
  std::wstring name;
  if (ClassRegistrar::GetInstance()->RetrieveClassName(class_info, &name))
    return name;

  // No class found, need to register one.
  HBRUSH background = NULL;
  WNDCLASSEX window_class;
  base::win::InitializeWindowClass(
      name.c_str(),
      &base::win::WrappedWindowProc<WindowImpl::WndProc>,
      class_info.style,
      0,
      0,
      NULL,
      reinterpret_cast<HBRUSH>(background + 1),
      NULL,
      icon,
      icon,
      &window_class);
  HMODULE instance = window_class.hInstance;
  ATOM atom = RegisterClassEx(&window_class);
  CHECK(atom) << GetLastError();

  ClassRegistrar::GetInstance()->RegisterClass(
      class_info, name, atom, instance);

  return name;
}

}  // namespace ui
