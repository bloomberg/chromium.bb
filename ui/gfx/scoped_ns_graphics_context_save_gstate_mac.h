// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_SCOPED_NS_GRAPHICS_CONTEXT_SAVE_GSTATE_MAC_H_
#define UI_GFX_SCOPED_NS_GRAPHICS_CONTEXT_SAVE_GSTATE_MAC_H_

#include "ui/ui_api.h"
#include "base/basictypes.h"
#include "base/memory/scoped_nsobject.h"

@class NSGraphicsContext;

namespace gfx {

class UI_API ScopedNSGraphicsContextSaveGState {
 public:
  // If |context| is nil, it will use the |+currentContext|.
  explicit ScopedNSGraphicsContextSaveGState(NSGraphicsContext* context = nil);
  ~ScopedNSGraphicsContextSaveGState();

 private:
  scoped_nsobject<NSGraphicsContext> context_;

  DISALLOW_COPY_AND_ASSIGN(ScopedNSGraphicsContextSaveGState);
};

}  // namespace gfx

#endif  // UI_GFX_SCOPED_NS_GRAPHICS_CONTEXT_SAVE_GSTATE_MAC_H_
