// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SimDisplayItemList_h
#define SimDisplayItemList_h

#include "core/testing/sim/SimCanvas.h"
#include "platform/graphics/paint/PaintRecord.h"
#include "platform/wtf/text/WTFString.h"
#include "public/platform/WebDisplayItemList.h"

namespace blink {

class SimDisplayItemList final : public WebDisplayItemList {
 public:
  SimDisplayItemList();

  void AppendDrawingItem(const WebRect& visual_rect,
                         sk_sp<const PaintRecord>,
                         const WebRect& record_bounds) override;

  int DrawCount() const { return commands_.size(); }

  bool Contains(SimCanvas::CommandType,
                const String& color_string = String()) const;

 private:
  Vector<SimCanvas::Command> commands_;
};

}  // namespace blink

#endif
