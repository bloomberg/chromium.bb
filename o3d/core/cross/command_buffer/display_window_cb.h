/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#ifndef O3D_CORE_WIN_DISPLAY_WINDOW_CB_H_
#define O3D_CORE_WIN_DISPLAY_WINDOW_CB_H_

#include "core/cross/display_window.h"
#include "gpu_plugin/np_utils/np_object_pointer.h"

namespace o3d {

// DisplayWindow subclass without information needed to connect to and use
// an out-of-process command buffer renderer.
class DisplayWindowCB : public DisplayWindow {
 public:
  DisplayWindowCB() : npp_(NULL), width_(0), height_(0) {}
  virtual ~DisplayWindowCB() {}

  NPP npp() const {
    return npp_;
  }

  void set_npp(NPP npp) {
    npp_ = npp;
  }

  gpu_plugin::NPObjectPointer<NPObject> command_buffer() const {
    return command_buffer_;
  }

  void set_command_buffer(
      const gpu_plugin::NPObjectPointer<NPObject> command_buffer) {
    command_buffer_ = command_buffer;
  }

  int width() const {
    return width_;
  }

  void set_width(int width) {
    width_ = width;
  }

  int height() const {
    return height_;
  }

  void set_height(int height) {
    height_ = height;
  }

 private:
  NPP npp_;
  gpu_plugin::NPObjectPointer<NPObject> command_buffer_;
  int width_;
  int height_;
  DISALLOW_COPY_AND_ASSIGN(DisplayWindowCB);
};
}  // end namespace o3d

#endif  // O3D_CORE_WIN_DISPLAY_WINDOW_CB_H_
