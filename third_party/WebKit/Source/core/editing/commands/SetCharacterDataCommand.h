// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SetCharacterDataCommand_h
#define SetCharacterDataCommand_h

#include "core/editing/commands/EditCommand.h"

namespace blink {

class CORE_EXPORT SetCharacterDataCommand final : public SimpleEditCommand {
 public:
  static SetCharacterDataCommand* create(Text* node,
                                         unsigned offset,
                                         unsigned count,
                                         const String& text) {
    return new SetCharacterDataCommand(node, offset, count, text);
  }

  DECLARE_VIRTUAL_TRACE();

 private:
  SetCharacterDataCommand(Text* node,
                          unsigned offset,
                          unsigned count,
                          const String& text);

  // EditCommand implementation
  void doApply(EditingState*) final;
  void doUnapply() final;

  const Member<Text> m_node;
  const unsigned m_offset;
  const unsigned m_count;
  String m_previousTextForUndo;
  const String m_newText;
};

}  // namespace blink

#endif  // SetCharacterDataCommand_h
