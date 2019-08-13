// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/frame_impl.h"

#include <utility>

#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/web/web_surrounding_text.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

FrameImpl::FrameImpl(WebLocalFrameImpl& frame,
                     InterfaceRegistry* interface_registry)
    : frame_(&frame) {
  if (!interface_registry)
    return;
  // TODO(crbug.com/800641): Use InterfaceValidator when it works for associated
  // interfaces.
  interface_registry->AddAssociatedInterface(
      WTF::BindRepeating(&FrameImpl::BindToReceiver, WrapWeakPersistent(this)));
}

void FrameImpl::BindToReceiver(
    mojo::PendingAssociatedReceiver<mojom::blink::Frame> receiver) {
  receiver_.Bind(std::move(receiver),
                 frame_->GetTaskRunner(blink::TaskType::kInternalDefault));
}

void FrameImpl::GetTextSurroundingSelection(
    uint32_t max_length,
    GetTextSurroundingSelectionCallback callback) {
  blink::WebSurroundingText surrounding_text(frame_, max_length);

  // |surrounding_text| might not be correctly initialized, for example if
  // |frame_->SelectionRange().IsNull()|, in other words, if there was no
  // selection.
  if (surrounding_text.IsEmpty()) {
    // Don't use WTF::String's default constructor so that we make sure that we
    // always send a valid empty string over the wire instead of a null pointer.
    std::move(callback).Run(g_empty_string, 0, 0);
    return;
  }

  std::move(callback).Run(surrounding_text.TextContent(),
                          surrounding_text.StartOffsetInTextContent(),
                          surrounding_text.EndOffsetInTextContent());
}

}  // namespace blink
