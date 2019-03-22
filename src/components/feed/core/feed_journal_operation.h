// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_FEED_JOURNAL_OPERATION_H_
#define COMPONENTS_FEED_CORE_FEED_JOURNAL_OPERATION_H_

#include <string>

#include "base/macros.h"

namespace feed {

// Native counterpart of JournalOperation.java.
class JournalOperation {
 public:
  enum Type {
    JOURNAL_APPEND,
    JOURNAL_COPY,
    JOURNAL_DELETE,
  };

  static JournalOperation CreateAppendOperation(std::string value);
  static JournalOperation CreateCopyOperation(std::string to_journal_name);
  static JournalOperation CreateDeleteOperation();

  JournalOperation(JournalOperation&& operation);

  Type type();
  const std::string& value();
  const std::string& to_journal_name();

 private:
  JournalOperation(Type type, std::string value, std::string to_journal_name);

  const Type type_;

  // Used for JOURNAL_APPEND, |value_| will be append to the end of journal.
  const std::string value_;

  // Used for JOURNAL_COPY, copy current journal to |to_journal_name_|.
  const std::string to_journal_name_;

  DISALLOW_COPY_AND_ASSIGN(JournalOperation);
};

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_FEED_JOURNAL_OPERATION_H_
