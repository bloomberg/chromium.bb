// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class is an implementation of the ChromotingView using Pepper devices
// as the backing stores.  This class is used only on pepper thread.
// Chromoting objects access this object through PepperViewProxy which
// delegates method calls on the pepper thread.

#ifndef REMOTING_CLIENT_PLUGIN_PEPPER_VIEW_H_
#define REMOTING_CLIENT_PLUGIN_PEPPER_VIEW_H_

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
  virtual void SetViewport(int x, int y, int width, int height) OVERRIDE;

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

  // Converts screen co-ordinates to host co-ordinates, and clips to the host
  // screen.
  pp::Point ConvertScreenToHost(const pp::Point& p) const;

  // Sets the size of the visible screen area that this object can render into.
  void SetScreenSize(int width, int height);

  // Sets whether the host screen is scaled to fit the visible screen area.
  void SetScaleToFit(bool enabled);

 private:
  void OnPaintDone(base::Time paint_start);
  void OnRefreshPaintDone();
  void PaintFrame(media::VideoFrame* frame, UpdatedRects* rects);

  // Blits the pixels in |r| in the backing store into the corresponding
  // rectangle in the scaled backing store. Returns that rectangle.
  pp::Rect UpdateScaledBackingStore(const pp::Rect& r);

  // Blanks out a rectangle in an image.
  void BlankRect(pp::ImageData& image_data, const pp::Rect& rect);

  // Converts host co-ordinates to screen co-ordinates.
  pp::Point ConvertHostToScreen(const pp::Point& p) const;

  // Sets the screen scale. A value of 1.0 indicates no scaling.
  void SetScreenScale(double screen_scale);

  // Paints the entire host screen.
  // This is called, for example, when the screen scale is changed.
  void RefreshPaint();

  // Resizes internals of this object after the host screen size has changed,
  // or the scale applied to that screen has changed.
  // More specifically, pepper's graphics object, the backing stores, and the
  // scaling cache are resized and/or recalculated.
  void ResizeInternals();

  // Whether to scale the screen.
  bool DoScaling() const;

  // Reference to the creating plugin instance. Needed for interacting with
  // pepper.  Marking explicitly as const since it must be initialized at
  // object creation, and never change.
  ChromotingInstance* const instance_;

  // Context should be constant for the lifetime of the plugin.
  ClientContext* const context_;

  pp::Graphics2D graphics2d_;

  // A backing store that saves the current desktop image.
  scoped_ptr<pp::ImageData> backing_store_;

  // A scaled copy of the backing store.
  scoped_ptr<pp::ImageData> scaled_backing_store_;

  // The factor by which to scale the host screen. A value of 1.0 indicates
  // no scaling.
  double screen_scale_;

  // An array containing the x co-ordinate of the point on the host screen
  // corresponding to the point (i, 0) on the client screen.
  scoped_array<int> screen_x_to_host_x_;

  // Whether to scale to fit.
  bool scale_to_fit_;

  // The size of the host screen.
  int host_width_;
  int host_height_;

  // The size of the visible area on the client screen that can be used to
  // display the host screen.
  int client_width_;
  int client_height_;

  bool is_static_fill_;
  uint32 static_fill_color_;

  ScopedRunnableMethodFactory<PepperView> task_factory_;

  DISALLOW_COPY_AND_ASSIGN(PepperView);
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_PLUGIN_PEPPER_VIEW_H_
