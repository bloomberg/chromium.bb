// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/widget/child_window_message_processor.h"

#include "app/win/scoped_prop.h"

namespace views {

static const wchar_t* kChildWindowKey = L"__CHILD_WINDOW_MESSAGE_PROCESSOR__";

// static
app::win::ScopedProp* ChildWindowMessageProcessor::Register(
    HWND hwnd,
    ChildWindowMessageProcessor* processor) {
  DCHECK(processor);
  return new app::win::ScopedProp(hwnd, kChildWindowKey, processor);
}

// static
ChildWindowMessageProcessor* ChildWindowMessageProcessor::Get(HWND hwnd) {
  return reinterpret_cast<ChildWindowMessageProcessor*>(
      ::GetProp(hwnd, kChildWindowKey));
}

}  // namespace
