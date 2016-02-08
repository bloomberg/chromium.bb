// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EditingState_h
#define EditingState_h

#include "wtf/Allocator.h"
#include "wtf/Assertions.h"
#include "wtf/Noncopyable.h"

namespace blink {

// EditingState represents current editing command running state for propagating
// DOM tree mutation operation failure to callers.
//
// Example usage:
//  EditingState editingState;
//  ...
//  functionMutatesDOMTree(..., &editingState);
//  if (editingState.isAborted())
//      return;
//
class EditingState final {
    STACK_ALLOCATED();
    WTF_MAKE_NONCOPYABLE(EditingState);
public:
    EditingState();
    ~EditingState();

    void abort();
    bool isAborted() const { return m_isAborted; }

private:
    bool m_isAborted = false;
};


// TODO(yosin): Once all commands aware |EditingState|, we get rid of
// |IgnorableEditingAbortState | class
class IgnorableEditingAbortState final {
    STACK_ALLOCATED();
    WTF_MAKE_NONCOPYABLE(IgnorableEditingAbortState);

    EditingState* editingState() { return &m_editingState; }

public:
    IgnorableEditingAbortState();
    ~IgnorableEditingAbortState();

private:
    EditingState m_editingState;
};

#if ENABLE(ASSERT)
// TODO(yosin): Once all commands aware |EditingState|, we get rid of
// |NoEditingAbortChecker| class
// This class is inspired by |NoExceptionStateAssertionChecke|.
class NoEditingAbortChecker final {
    STACK_ALLOCATED();
    WTF_MAKE_NONCOPYABLE(NoEditingAbortChecker);
public:
    NoEditingAbortChecker(const char* file, int line);
    ~NoEditingAbortChecker();

    EditingState* editingState() { return &m_editingState; }

private:
    EditingState m_editingState;
    const char* const m_file;
    int const m_line;
};

// All assertion in "core/editing/commands" should use
// |ASSERT_IN_EDITING_COMMAND()| instead of |ASSERT()|.
#define ASSERT_IN_EDITING_COMMAND(expr) \
    do { \
        if (!(expr)) { \
            editingState->abort(); \
            return; \
        } \
        break; \
    } while (true)

// A macro for default parameter of |EditingState*| parameter.
// TODO(yosin): Once all commands aware |EditingState|, we get rid of
// |ASSERT_NO_EDITING_ABORT| macro.
#define ASSERT_NO_EDITING_ABORT (NoEditingAbortChecker(__FILE__, __LINE__).editingState())
#else
#define ASSERT_NO_EDITING_ABORT (IgnorableEditingAbortState().editingState())
#endif

} // namespace blink

#endif // EditingState_h
