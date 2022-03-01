// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fragment_directive/fragment_directive_utils.h"

#include "components/shared_highlighting/core/common/fragment_directives_utils.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"

namespace blink {

// static
void FragmentDirectiveUtils::RemoveSelectorsFromUrl(LocalFrame* frame) {
  KURL url(shared_highlighting::RemoveFragmentSelectorDirectives(
      frame->Loader().GetDocumentLoader()->GetHistoryItem()->Url()));

  // Replace the current history entry with the new url, so that the text
  // fragment shown in the URL matches the state of the highlight on the page.
  // This is equivalent to history.replaceState in javascript.
  frame->DomWindow()->document()->Loader()->RunURLAndHistoryUpdateSteps(
      url, mojom::blink::SameDocumentNavigationType::kFragment,
      /*data=*/nullptr, WebFrameLoadType::kReplaceCurrentItem,
      mojom::blink::ScrollRestorationType::kAuto);
}

}  // namespace blink
