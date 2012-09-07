// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WIN8_DELEGATE_EXECUTE_DELEGATE_EXECUTE_OPERATION_H_
#define WIN8_DELEGATE_EXECUTE_DELEGATE_EXECUTE_OPERATION_H_

#include <atldef.h>

#include "base/basictypes.h"
#include "base/file_path.h"

class CommandLine;

namespace delegate_execute {

// Parses a portion of the DelegateExecute handler's command line to determine
// the desired operation.
class DelegateExecuteOperation {
 public:
  enum OperationType {
    EXE_MODULE,
    RELAUNCH_CHROME,
  };

  DelegateExecuteOperation();
  ~DelegateExecuteOperation();

  void Initialize(const CommandLine* command_line);

  OperationType operation_type() const {
    return operation_type_;
  }

  // Returns the argument to the --relaunch-shortcut switch.  Valid only when
  // the operation is RELAUNCH_CHROME.
  const FilePath& relaunch_shortcut() const {
    ATLASSERT(operation_type_ == RELAUNCH_CHROME);
    return relaunch_shortcut_;
  }

 private:
  void Clear();

  OperationType operation_type_;
  FilePath relaunch_shortcut_;
  DISALLOW_COPY_AND_ASSIGN(DelegateExecuteOperation);
};

}  // namespace delegate_execute

#endif  // WIN8_DELEGATE_EXECUTE_DELEGATE_EXECUTE_OPERATION_H_
