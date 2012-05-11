// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/clipboard.h"

#include "base/logging.h"

namespace remoting {

class ClipboardLinux : public Clipboard {
 public:
  ClipboardLinux();

  // Must be called on the UI thread.
  virtual void Start() OVERRIDE;
  virtual void InjectClipboardEvent(
      const protocol::ClipboardEvent& event) OVERRIDE;
  virtual void Stop() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(ClipboardLinux);
};

void ClipboardLinux::Start() {
  NOTIMPLEMENTED();
}

void ClipboardLinux::InjectClipboardEvent(
    const protocol::ClipboardEvent& event) {
  NOTIMPLEMENTED();
}

void ClipboardLinux::Stop() {
  NOTIMPLEMENTED();
}

scoped_ptr<Clipboard> Clipboard::Create() {
  return scoped_ptr<Clipboard>(new ClipboardLinux());
}

}  // namespace remoting
