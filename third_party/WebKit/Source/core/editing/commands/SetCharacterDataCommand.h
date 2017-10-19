// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SetCharacterDataCommand_h
#define SetCharacterDataCommand_h

#include "core/editing/commands/EditCommand.h"

namespace blink {

class CORE_EXPORT SetCharacterDataCommand final : public SimpleEditCommand {
 public:
  static SetCharacterDataCommand* Create(Text* node,
                                         unsigned offset,
                                         unsigned count,
                                         const String& text) {
    return new SetCharacterDataCommand(node, offset, count, text);
  }

  virtual void Trace(blink::Visitor*);

 private:
  SetCharacterDataCommand(Text* node,
                          unsigned offset,
                          unsigned count,
                          const String& text);

  // EditCommand implementation
  void DoApply(EditingState*) final;
  void DoUnapply() final;

  const Member<Text> node_;
  const unsigned offset_;
  const unsigned count_;
  String previous_text_for_undo_;
  const String new_text_;
};

}  // namespace blink

#endif  // SetCharacterDataCommand_h
