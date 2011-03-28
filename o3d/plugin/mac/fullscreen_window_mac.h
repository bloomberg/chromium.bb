/*
 * Copyright 2010, Google Inc.
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

#ifndef O3D_PLUGIN_MAC_FULLSCREEN_WINDOW_MAC_H_
#define O3D_PLUGIN_MAC_FULLSCREEN_WINDOW_MAC_H_

#include <OpenGL/OpenGL.h>

#include "base/basictypes.h"
#include "base/scoped_ptr.h"

#include "plugin/mac/overlay_window_mac.h"

// Abstract base class for the full-screen window. Provides the
// ability to easily choose between Carbon and Cocoa implementations.

namespace glue {
namespace _o3d {
class PluginObject;
}  // namespace _o3d
}  // namespace glue

namespace o3d {

class FullscreenWindowMac {
 public:
  static FullscreenWindowMac* Create(glue::_o3d::PluginObject* obj,
                                     int target_width,
                                     int target_height);
  virtual ~FullscreenWindowMac();

  virtual bool Initialize(int target_width,
                          int target_height);
  virtual void IdleCallback();
  virtual bool Shutdown(const GLint* last_buffer_rect);

  virtual CGRect GetWindowBounds() const = 0;
  virtual bool IsActive() const = 0;
  virtual void PrepareToRender() const = 0;
  virtual void FinishRendering() const = 0;

 protected:
  FullscreenWindowMac() {}

 private:
#ifdef O3D_PLUGIN_ENABLE_FULLSCREEN_MSG
  scoped_ptr<OverlayWindowMac> overlay_window_;
#endif

  DISALLOW_COPY_AND_ASSIGN(FullscreenWindowMac);
};

}  // namespace o3d

#endif  // O3D_PLUGIN_MAC_FULLSCREEN_WINDOW_MAC_H_

