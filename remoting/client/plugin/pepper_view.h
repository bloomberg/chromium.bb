// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class is an implementation of the ChromotingView using Pepper devices
// as the backing stores.  This class is used only on pepper thread.
// Chromoting objects access this object through PepperViewProxy which
// delegates method calls on the pepper thread.

#ifndef REMOTING_CLIENT_PLUGIN_PEPPER_VIEW_H_
#define REMOTING_CLIENT_PLUGIN_PEPPER_VIEW_H_

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/task.h"
#include "media/base/video_frame.h"
#include "ppapi/cpp/graphics_2d.h"
#include "ppapi/cpp/point.h"
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
  virtual bool Initialize() OVERRIDE;
  virtual void TearDown() OVERRIDE;
  virtual void Paint() OVERRIDE;
  virtual void SetSolidFill(uint32 color) OVERRIDE;
  virtual void UnsetSolidFill() OVERRIDE;
  virtual void SetConnectionState(ConnectionState state) OVERRIDE;
  virtual void UpdateLoginStatus(bool success, const std::string& info)
      OVERRIDE;
  virtual double GetHorizontalScaleRatio() const OVERRIDE;
  virtual double GetVerticalScaleRatio() const OVERRIDE;

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
                                    RectVector* rects,
                                    Task* done);

  // This is called when the dimension of the plugin element has changed.
  // Return true if plugin size has changed, false otherwise.
  bool SetPluginSize(const SkISize& plugin_size);

 private:
  void OnPaintDone(base::Time paint_start);

  // Set the dimension of the entire host screen.
  void SetHostSize(const SkISize& host_size);

  void PaintFrame(media::VideoFrame* frame, RectVector* rects);

  // Render the rectangle of |frame| to the backing store.
  // Returns true if this rectangle is not clipped.
  bool PaintRect(media::VideoFrame* frame, const SkIRect& rect);

  // Blanks out a rectangle in an image.
  void BlankRect(pp::ImageData& image_data, const pp::Rect& rect);

  // Perform a flush on the graphics context.
  void FlushGraphics(base::Time paint_start);

  // Reference to the creating plugin instance. Needed for interacting with
  // pepper.  Marking explicitly as const since it must be initialized at
  // object creation, and never change.
  ChromotingInstance* const instance_;

  // Context should be constant for the lifetime of the plugin.
  ClientContext* const context_;

  pp::Graphics2D graphics2d_;

  // A backing store that saves the current desktop image.
  scoped_ptr<pp::ImageData> backing_store_;

  // True if there is pending paint commands in Pepper's queue. This is set to
  // true if the last flush returns a PP_ERROR_INPROGRESS error.
  bool flush_blocked_;

  // The size of the plugin element.
  SkISize plugin_size_;

  // The size of the host screen.
  SkISize host_size_;

  bool is_static_fill_;
  uint32 static_fill_color_;

  ScopedRunnableMethodFactory<PepperView> task_factory_;

  DISALLOW_COPY_AND_ASSIGN(PepperView);
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_PLUGIN_PEPPER_VIEW_H_
