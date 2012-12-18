// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CAPTURER_WIN_DESKTOP_H_
#define REMOTING_CAPTURER_WIN_DESKTOP_H_

#include <windows.h>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/string16.h"

namespace remoting {

class Desktop {
 public:
  ~Desktop();

  // Returns the name of the desktop represented by the object. Return false if
  // quering the name failed for any reason.
  bool GetName(string16* desktop_name_out) const;

  // Returns true if |other| has the same name as this desktop. Returns false
  // in any other case including failing Win32 APIs and uninitialized desktop
  // handles.
  bool IsSame(const Desktop& other) const;

  // Assigns the desktop to the current thread. Returns false is the operation
  // failed for any reason.
  bool SetThreadDesktop() const;

  // Returns the desktop by its name or NULL if an error occurs.
  static scoped_ptr<Desktop> GetDesktop(const wchar_t* desktop_name);

  // Returns the desktop currently receiving user input or NULL if an error
  // occurs.
  static scoped_ptr<Desktop> GetInputDesktop();

  // Returns the desktop currently assigned to the calling thread or NULL if
  // an error occurs.
  static scoped_ptr<Desktop> GetThreadDesktop();

 private:
  Desktop(HDESK desktop, bool own);

  // The desktop handle.
  HDESK desktop_;

  // True if |desktop_| must be closed on teardown.
  bool own_;

  DISALLOW_COPY_AND_ASSIGN(Desktop);
};

}  // namespace remoting

#endif  // REMOTING_CAPTURER_WIN_DESKTOP_H_
