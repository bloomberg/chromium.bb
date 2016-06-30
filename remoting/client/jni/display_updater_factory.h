// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_JNI_DISPLAY_UPDATER_FACTORY_H_
#define REMOTING_CLIENT_JNI_DISPLAY_UPDATER_FACTORY_H_

#include <memory>

#include "base/macros.h"

namespace remoting {

namespace protocol {
class CursorShapeStub;
class VideoRenderer;
}  // namespace protocol

// Interface for creating objects to update image (desktop frame or cursor
// shape) to display. Factory functions can be called on any thread but the
// returned object should be used on the network thread.
class DisplayUpdaterFactory {
 public:
  virtual ~DisplayUpdaterFactory() {}

  virtual std::unique_ptr<protocol::CursorShapeStub>
  CreateCursorShapeStub() = 0;
  virtual std::unique_ptr<protocol::VideoRenderer> CreateVideoRenderer() = 0;

 protected:
  DisplayUpdaterFactory() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(DisplayUpdaterFactory);
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_JNI_DISPLAY_UPDATER_FACTORY_H_
