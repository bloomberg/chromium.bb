// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SimDisplayItemList_h
#define SimDisplayItemList_h

#include "platform/graphics/paint/PaintRecord.h"
#include "public/platform/WebDisplayItemList.h"
#include "web/tests/sim/SimCanvas.h"
#include "wtf/text/WTFString.h"

namespace blink {

class SimDisplayItemList final : public WebDisplayItemList {
 public:
  SimDisplayItemList();

  void appendDrawingItem(const WebRect&, sk_sp<const PaintRecord>) override;

  int drawCount() const { return m_commands.size(); }

  bool contains(SimCanvas::CommandType,
                const String& colorString = String()) const;

 private:
  Vector<SimCanvas::Command> m_commands;
};

}  // namespace blink

#endif
