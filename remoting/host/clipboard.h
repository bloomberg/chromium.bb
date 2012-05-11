// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CLIPBOARD_H_
#define REMOTING_HOST_CLIPBOARD_H_

#include <string>

#include "base/callback.h"

namespace remoting {

namespace protocol {
class ClipboardEvent;
}  // namespace protocol

class Clipboard {
 public:
  virtual ~Clipboard() {};

  // Initialises any objects needed to read from or write to the clipboard.
  // This method must be called on the desktop thread.
  virtual void Start() = 0;

  // Destroys any objects initialised by Start().
  // This method must be called on the desktop thread.
  virtual void Stop() = 0;

  // Writes an item to the clipboard.
  // This method must be called on the desktop thread, after Start().
  virtual void InjectClipboardEvent(const protocol::ClipboardEvent& event) = 0;

  static scoped_ptr<Clipboard> Create();
};

}  // namespace remoting

#endif  // REMOTING_HOST_CLIPBOARD_H_
