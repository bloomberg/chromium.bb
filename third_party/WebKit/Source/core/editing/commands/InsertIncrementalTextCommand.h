// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InsertIncrementalTextCommand_h
#define InsertIncrementalTextCommand_h

#include "core/editing/commands/InsertTextCommand.h"

namespace blink {

class CORE_EXPORT InsertIncrementalTextCommand final
    : public InsertTextCommand {
 public:
  static InsertIncrementalTextCommand* Create(
      Document&,
      const String&,
      RebalanceType = kRebalanceLeadingAndTrailingWhitespaces);

 private:
  InsertIncrementalTextCommand(Document&,
                               const String& text,
                               RebalanceType);
  void DoApply(EditingState*) override;
};

}  // namespace blink

#endif  // InsertIncrementalTextCommand_h
