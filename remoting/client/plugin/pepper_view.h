// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class is an implementation of the ChromotingView using Pepper devices
// as the backing stores.  The public APIs to this class are thread-safe.
// Calls will dispatch any interaction with the pepper API onto the pepper
// main thread.
//
// TODO(ajwong): We need to better understand the threading semantics of this
// class.  Currently, we're just going to always run everything on the pepper
// main thread.  Is this smart?

#ifndef REMOTING_CLIENT_PLUGIN_PEPPER_VIEW_H_
#define REMOTING_CLIENT_PLUGIN_PEPPER_VIEW_H_

#include "base/scoped_ptr.h"
#include "base/task.h"
#include "media/base/video_frame.h"
#include "remoting/base/decoder.h"
#include "remoting/client/chromoting_view.h"
#include "third_party/ppapi/cpp/device_context_2d.h"

namespace remoting {

class ChromotingPlugin;
class Decoder;

class PepperView : public ChromotingView {
 public:
  // Constructs a PepperView that draws to the |rendering_device|. The
  // |rendering_device| instance must outlive this class.
  //
  // TODO(ajwong): This probably needs to synchronize with the pepper thread
  // to be safe.
  explicit PepperView(ChromotingPlugin* plugin);
  virtual ~PepperView();

  // ChromotingView implementation.
  virtual bool Initialize();
  virtual void TearDown();
  virtual void Paint();
  virtual void SetSolidFill(uint32 color);
  virtual void UnsetSolidFill();
  virtual void SetViewport(int x, int y, int width, int height);
  virtual void SetHostScreenSize(int width, int height);
  virtual void HandleBeginUpdateStream(HostMessage* msg);
  virtual void HandleUpdateStreamPacket(HostMessage* msg);
  virtual void HandleEndUpdateStream(HostMessage* msg);

 private:
  void OnPaintDone();
  void OnPartialDecodeDone();
  void OnDecodeDone();

  // Reference to the creating plugin instance. Needed for interacting with
  // pepper.  Marking explciitly as const since it must be initialized at
  // object creation, and never change.
  ChromotingPlugin* const plugin_;

  pp::DeviceContext2D device_context_;

  int backing_store_width_;
  int backing_store_height_;

  int viewport_x_;
  int viewport_y_;
  int viewport_width_;
  int viewport_height_;

  bool is_static_fill_;
  uint32 static_fill_color_;

  scoped_refptr<media::VideoFrame> frame_;
  UpdatedRects update_rects_;
  UpdatedRects all_update_rects_;

  scoped_ptr<Decoder> decoder_;

  DISALLOW_COPY_AND_ASSIGN(PepperView);
};

}  // namespace remoting

DISABLE_RUNNABLE_METHOD_REFCOUNT(remoting::PepperView);

#endif  // REMOTING_CLIENT_PLUGIN_PEPPER_VIEW_H_
