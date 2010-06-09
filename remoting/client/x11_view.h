// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_X11_VIEW_H_
#define REMOTING_CLIENT_X11_VIEW_H_

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "media/base/video_frame.h"
#include "remoting/client/decoder.h"
#include "remoting/client/chromoting_view.h"

typedef struct _XDisplay Display;

namespace remoting {

// A ChromotingView implemented using X11 and XRender.
class X11View : public ChromotingView {
 public:
  X11View(Display* display, int window, int width, int height);

  virtual ~X11View();

  // ChromotingView implementations.
  virtual void Paint();
  virtual void HandleBeginUpdateStream(HostMessage* msg);
  virtual void HandleUpdateStreamPacket(HostMessage* msg);
  virtual void HandleEndUpdateStream(HostMessage* msg) ;

 private:
  void InitPaintTarget();
  void OnPartialDecodeDone();
  void OnDecodeDone();

  Display* display_;
  int window_;
  int width_;
  int height_;

  // A picture created in the X server that represents drawing area of the
  // window.
  int picture_;

  scoped_refptr<media::VideoFrame> frame_;
  UpdatedRects update_rects_;
  UpdatedRects all_update_rects_;

  scoped_ptr<Decoder> decoder_;

  DISALLOW_COPY_AND_ASSIGN(X11View);
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_X11_VIEW_H_
