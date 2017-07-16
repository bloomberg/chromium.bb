// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/exported/WebViewBase.h"

#include <memory>

#include "core/page/ScopedPageSuspender.h"
#include "platform/wtf/HashSet.h"
#include "platform/wtf/StdLibExtras.h"
#include "platform/wtf/Vector.h"

namespace blink {

// Used to defer all page activity in cases where the embedder wishes to run
// a nested event loop. Using a stack enables nesting of message loop
// invocations.
static Vector<std::unique_ptr<ScopedPageSuspender>>& PageSuspenderStack() {
  DEFINE_STATIC_LOCAL(Vector<std::unique_ptr<ScopedPageSuspender>>,
                      suspender_stack, ());
  return suspender_stack;
}

void WebView::WillEnterModalLoop() {
  PageSuspenderStack().push_back(WTF::MakeUnique<ScopedPageSuspender>());
}

void WebView::DidExitModalLoop() {
  DCHECK(PageSuspenderStack().size());
  PageSuspenderStack().pop_back();
}

// static
HashSet<WebViewBase*>& WebViewBase::AllInstances() {
  DEFINE_STATIC_LOCAL(HashSet<WebViewBase*>, all_instances, ());
  return all_instances;
}

}  // namespace blink
