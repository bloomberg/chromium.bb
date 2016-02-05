// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/commands/EditingState.h"

namespace blink {

EditingState::EditingState()
{
}

EditingState::~EditingState()
{
}

void EditingState::abort()
{
    ASSERT(!m_isAborted);
    m_isAborted = true;
}

// ---

NoEditingAbortChecker::NoEditingAbortChecker(const char* file, int line)
    : m_file(file)
    , m_line(line) { }

NoEditingAbortChecker::~NoEditingAbortChecker()
{
    ASSERT_AT(!m_editingState.isAborted(), m_file, m_line, "");
}


} // namespace blink
