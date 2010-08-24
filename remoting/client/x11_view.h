// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_X11_VIEW_H_
#define REMOTING_CLIENT_X11_VIEW_H_

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "media/base/video_frame.h"
#include "remoting/client/chromoting_view.h"

typedef unsigned long XID;
typedef struct _XDisplay Display;

namespace remoting {

// A ChromotingView implemented using X11 and XRender.
class X11View : public ChromotingView {
 public:
  X11View();
  virtual ~X11View();

  // ChromotingView implementations.
  virtual bool Initialize();
  virtual void TearDown();
  virtual void Paint();
  virtual void SetSolidFill(uint32 color);
  virtual void UnsetSolidFill();
  virtual void SetViewport(int x, int y, int width, int height);
  virtual void SetHostScreenSize(int width, int height);
  virtual void HandleBeginUpdateStream(ChromotingHostMessage* msg);
  virtual void HandleUpdateStreamPacket(ChromotingHostMessage* msg);
  virtual void HandleEndUpdateStream(ChromotingHostMessage* msg);

  Display* display() { return display_; }

 private:
  void InitPaintTarget();
  void OnPartialDecodeDone();
  void OnDecodeDone();

  Display* display_;
  XID window_;

  // A picture created in the X server that represents drawing area of the
  // window.
  XID picture_;

  DISALLOW_COPY_AND_ASSIGN(X11View);
};

}  // namespace remoting

DISABLE_RUNNABLE_METHOD_REFCOUNT(remoting::X11View);

#endif  // REMOTING_CLIENT_X11_VIEW_H_
