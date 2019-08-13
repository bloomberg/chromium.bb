// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_FRAME_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_FRAME_IMPL_H_

#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "third_party/blink/public/mojom/frame/frame.mojom-blink.h"
#include "third_party/blink/public/platform/interface_registry.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

class WebLocalFrameImpl;

// Implementation of blink::mojom::Frame
class CORE_EXPORT FrameImpl final : public GarbageCollectedFinalized<FrameImpl>,
                                    public mojom::blink::Frame {
 public:
  // TODO(crbug.com/980151): Construct FrameImpl this way only while we need to
  // rely in blink::WebSurroundingText to implement GetTextSurroundingSelection,
  // and remove the dependency once WebSurroundingText is out of the public API.
  FrameImpl(WebLocalFrameImpl& frame, InterfaceRegistry* interface_registry);

  void BindToReceiver(
      mojo::PendingAssociatedReceiver<mojom::blink::Frame> receiver);

  void GetTextSurroundingSelection(
      uint32_t max_length,
      GetTextSurroundingSelectionCallback callback) final;

  void Trace(blink::Visitor* visitor) { visitor->Trace(frame_); }

 private:
  const Member<WebLocalFrameImpl> frame_;

  mojo::AssociatedReceiver<mojom::blink::Frame> receiver_{this};

  DISALLOW_COPY_AND_ASSIGN(FrameImpl);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_FRAME_IMPL_H_
