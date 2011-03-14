// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class is an implementation of the ChromotingView using Pepper devices
// as the backing stores.  This class is used only on pepper thread.
// Chromoting objects access this object through PepperViewProxy which
// delegates method calls on the pepper thread.

#ifndef REMOTING_CLIENT_PLUGIN_PEPPER_VIEW_H_
#define REMOTING_CLIENT_PLUGIN_PEPPER_VIEW_H_

#include "base/scoped_ptr.h"
#include "base/task.h"
#include "media/base/video_frame.h"
#include "ppapi/cpp/graphics_2d.h"
#include "remoting/client/chromoting_view.h"
#include "remoting/client/frame_consumer.h"

namespace remoting {

class ChromotingInstance;
class ClientContext;

class PepperView : public ChromotingView,
                   public FrameConsumer {
 public:
  // Constructs a PepperView that draws to the |rendering_device|. The
  // |rendering_device| instance must outlive this class.
  PepperView(ChromotingInstance* instance, ClientContext* context);
  virtual ~PepperView();

  // ChromotingView implementation.
  virtual bool Initialize();
  virtual void TearDown();
  virtual void Paint();
  virtual void SetSolidFill(uint32 color);
  virtual void UnsetSolidFill();
  virtual void SetConnectionState(ConnectionState state);
  virtual void UpdateLoginStatus(bool success, const std::string& info);
  virtual void SetViewport(int x, int y, int width, int height);

  // FrameConsumer implementation.
  virtual void AllocateFrame(media::VideoFrame::Format format,
                             size_t width,
                             size_t height,
                             base::TimeDelta timestamp,
                             base::TimeDelta duration,
                             scoped_refptr<media::VideoFrame>* frame_out,
                             Task* done);
  virtual void ReleaseFrame(media::VideoFrame* frame);
  virtual void OnPartialFrameOutput(media::VideoFrame* frame,
                                    UpdatedRects* rects,
                                    Task* done);

 private:
  void OnPaintDone();
  void PaintFrame(media::VideoFrame* frame, UpdatedRects* rects);

  // Reference to the creating plugin instance. Needed for interacting with
  // pepper.  Marking explciitly as const since it must be initialized at
  // object creation, and never change.
  ChromotingInstance* const instance_;

  // Context should be constant for the lifetime of the plugin.
  ClientContext* const context_;

  pp::Graphics2D graphics2d_;

  // A backing store that saves the current desktop image.
  scoped_ptr<pp::ImageData> backing_store_;

  int viewport_width_;
  int viewport_height_;

  bool is_static_fill_;
  uint32 static_fill_color_;

  ScopedRunnableMethodFactory<PepperView> task_factory_;

  DISALLOW_COPY_AND_ASSIGN(PepperView);
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_PLUGIN_PEPPER_VIEW_H_
