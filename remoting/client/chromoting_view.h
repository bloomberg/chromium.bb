// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_CHROMOTING_VIEW_H_
#define REMOTING_CLIENT_CHROMOTING_VIEW_H_

#include "base/ref_counted.h"

namespace remoting {

class HostMessage;

// ChromotingView defines the behavior of an object that draws a view of the
// remote desktop. Its main function is to choose the right decoder and render
// the update stream onto the screen.
class ChromotingView : public base::RefCountedThreadSafe<ChromotingView> {
 public:
  virtual ~ChromotingView() {}

  // Tells the ChromotingView to paint the current image on the screen.
  // TODO(hclam): Add rects as parameter if needed.
  virtual void Paint() = 0;

  // Handle the BeginUpdateStream message.
  virtual void HandleBeginUpdateStream(HostMessage* msg) = 0;

  // Handle the UpdateStreamPacket message.
  virtual void HandleUpdateStreamPacket(HostMessage* msg) = 0;

  // Handle the EndUpdateStream message.
  virtual void HandleEndUpdateStream(HostMessage* msg) = 0;
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_CHROMOTING_VIEW_H_
