// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class is an implementation of the ChromotingView for Pepper.  It is
// callable only on the Pepper thread.

#ifndef REMOTING_CLIENT_PLUGIN_PEPPER_VIEW_H_
#define REMOTING_CLIENT_PLUGIN_PEPPER_VIEW_H_

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
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
  // Constructs a PepperView for the |instance|. The |instance| and
  // |context| must outlive this class.
  PepperView(ChromotingInstance* instance, ClientContext* context);
  virtual ~PepperView();

  // ChromotingView implementation.
  virtual bool Initialize() OVERRIDE;
  virtual void TearDown() OVERRIDE;
  virtual void Paint() OVERRIDE;
  virtual void SetSolidFill(uint32 color) OVERRIDE;
  virtual void UnsetSolidFill() OVERRIDE;
  virtual void SetConnectionState(
      protocol::ConnectionToHost::State state,
      protocol::ConnectionToHost::Error error) OVERRIDE;

  // FrameConsumer implementation.
  virtual void AllocateFrame(media::VideoFrame::Format format,
                             const SkISize& size,
                             scoped_refptr<media::VideoFrame>* frame_out,
                             const base::Closure& done) OVERRIDE;
  virtual void ReleaseFrame(media::VideoFrame* frame) OVERRIDE;
  virtual void OnPartialFrameOutput(media::VideoFrame* frame,
                                    RectVector* rects,
                                    const base::Closure& done) OVERRIDE;

  // Sets the display size of this view.  Returns true if plugin size has
  // changed, false otherwise.
  bool SetViewSize(const SkISize& plugin_size);

  // Return the client view and original host dimensions.
  const SkISize& get_view_size() const {
    return view_size_;
  }
  const SkISize& get_host_size() const {
    return host_size_;
  }

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
  SkISize view_size_;

  // The size of the host screen.
  SkISize host_size_;

  bool is_static_fill_;
  uint32 static_fill_color_;

  base::WeakPtrFactory<PepperView> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PepperView);
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_PLUGIN_PEPPER_VIEW_H_
