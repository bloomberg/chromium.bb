// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_PLUGIN_PEPPER_VIEW_H_
#define REMOTING_CLIENT_PLUGIN_PEPPER_VIEW_H_

#include "base/scoped_ptr.h"
#include "base/task.h"
#include "remoting/client/chromoting_view.h"
#include "third_party/npapi/bindings/npapi.h"
#include "third_party/npapi/bindings/npapi_extensions.h"

class MessageLoop;

namespace remoting {

class Decoder;

class PepperView : public ChromotingView {
 public:
  // Constructs a PepperView that draw to the |rendering_device|. The
  // |rendering_device| instance must outlive this class.
  //
  // TODO(ajwong): This probably needs to synchronize with the pepper thread
  // to be safe.
  PepperView(MessageLoop* message_loop, NPDevice* rendering_device,
             NPP plugin_instance);
  virtual ~PepperView();

  // ChromotingView implementation.
  virtual void Paint();
  virtual void SetSolidFill(uint32 color);
  virtual void UnsetSolidFill();
  virtual void SetViewport(int x, int y, int width, int height);
  virtual void SetBackingStoreSize(int width, int height);
  virtual void HandleBeginUpdateStream(HostMessage* msg);
  virtual void HandleUpdateStreamPacket(HostMessage* msg);
  virtual void HandleEndUpdateStream(HostMessage* msg);

 private:
  void DoPaint();
  void DoSetSolidFill(uint32 color);
  void DoUnsetSolidFill();
  void DoSetViewport(int x, int y, int width, int height);
  void DoSetBackingStoreSize(int width, int height);
  void DoHandleBeginUpdateStream(HostMessage* msg);
  void DoHandleUpdateStreamPacket(HostMessage* msg);
  void DoHandleEndUpdateStream(HostMessage* msg);

  // Synchronization and thread handling objects.
  MessageLoop* message_loop_;

  // Handles to Pepper objects needed for drawing to the screen.
  NPDevice* rendering_device_;
  NPP plugin_instance_;

  int backing_store_width_;
  int backing_store_height_;

  int viewport_x_;
  int viewport_y_;
  int viewport_width_;
  int viewport_height_;

  bool is_static_fill_;
  uint32 static_fill_color_;

  scoped_ptr<Decoder> decoder_;

  DISALLOW_COPY_AND_ASSIGN(PepperView);
};

}  // namespace remoting

DISABLE_RUNNABLE_METHOD_REFCOUNT(remoting::PepperView);

#endif  // REMOTING_CLIENT_PLUGIN_PEPPER_VIEW_H_
