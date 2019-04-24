//===- Win32/Win32.h - Common Win32 Include File ----------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file defines things specific to Win32 implementations.
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
//=== WARNING: Implementation here must contain only generic Win32 code that
//===          is guaranteed to work on *all* Win32 variants.
//===----------------------------------------------------------------------===//

// mingw-w64 tends to define it as 0x0502 in its headers.
#undef _WIN32_WINNT

// Require at least Windows XP(5.1) API.
#define _WIN32_WINNT 0x0501
#define _WIN32_IE    0x0600 // MinGW at it again.
#define WIN32_LEAN_AND_MEAN

#include "llvm/Config/config.h" // Get build system configuration settings
#include <windows.h>
#include <shlobj.h>
#include <cassert>
#include <string>

inline bool MakeErrMsg(std::string* ErrMsg, const std::string& prefix) {
  if (!ErrMsg)
    return true;
  char *buffer = NULL;
  FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM,
      NULL, GetLastError(), 0, (LPSTR)&buffer, 1, NULL);
  *ErrMsg = prefix + buffer;
  LocalFree(buffer);
  return true;
}

class AutoHandle {
  HANDLE handle;

public:
  AutoHandle(HANDLE h) : handle(h) {}

  ~AutoHandle() {
    if (handle)
      CloseHandle(handle);
  }

  operator HANDLE() {
    return handle;
  }

  AutoHandle &operator=(HANDLE h) {
    handle = h;
    return *this;
  }
};

template <class HandleType, uintptr_t InvalidHandle,
          class DeleterType, DeleterType D>
class ScopedHandle {
  HandleType Handle;

public:
  ScopedHandle() : Handle(InvalidHandle) {}
  ScopedHandle(HandleType handle) : Handle(handle) {}

  ~ScopedHandle() {
    if (Handle != HandleType(InvalidHandle))
      D(Handle);
  }

  HandleType take() {
    HandleType temp = Handle;
    Handle = HandleType(InvalidHandle);
    return temp;
  }

  operator HandleType() const { return Handle; }

  ScopedHandle &operator=(HandleType handle) {
    Handle = handle;
    return *this;
  }

  typedef void (*unspecified_bool_type)();
  static void unspecified_bool_true() {}

  // True if Handle is valid.
  operator unspecified_bool_type() const {
    return Handle == HandleType(InvalidHandle) ? 0 : unspecified_bool_true;
  }

  bool operator!() const {
    return Handle == HandleType(InvalidHandle);
  }
};

typedef ScopedHandle<HANDLE, uintptr_t(-1),
                      BOOL (WINAPI*)(HANDLE), ::FindClose>
  ScopedFindHandle;

namespace llvm {
template <class T>
class SmallVectorImpl;

template <class T>
typename SmallVectorImpl<T>::const_pointer
c_str(SmallVectorImpl<T> &str) {
  str.push_back(0);
  str.pop_back();
  return str.data();
}
} // end namespace llvm.
