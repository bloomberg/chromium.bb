// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/fenced_frame/fenced_frame_mparch_delegate.h"

#include "mojo/public/cpp/bindings/associated_remote.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/fenced_frame/fenced_frame.mojom-blink.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/remote_frame.h"
#include "third_party/blink/renderer/core/html/fenced_frame/document_fenced_frames.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

FencedFrameMPArchDelegate::FencedFrameMPArchDelegate(
    HTMLFencedFrameElement* outer_element)
    : HTMLFencedFrameElement::FencedFrameDelegate(outer_element) {
  DCHECK_EQ(outer_element->GetDocument()
                .GetFrame()
                ->GetPage()
                ->FencedFramesImplementationType()
                .value(),
            features::FencedFramesImplementationType::kMPArch);

  DocumentFencedFrames::GetOrCreate(GetElement().GetDocument())
      .RegisterFencedFrame(&GetElement());
  mojo::PendingAssociatedRemote<mojom::blink::FencedFrameOwnerHost> remote;
  mojo::PendingAssociatedReceiver<mojom::blink::FencedFrameOwnerHost> receiver =
      remote.InitWithNewEndpointAndPassReceiver();
  remote_.Bind(std::move(remote));

  RemoteFrame* remote_frame =
      GetElement().GetDocument().GetFrame()->Client()->CreateFencedFrame(
          &GetElement(), std::move(receiver), GetElement().GetMode());
  DCHECK_EQ(remote_frame, GetElement().ContentFrame());
}

void FencedFrameMPArchDelegate::Navigate(const KURL& url) {
  DCHECK(remote_);
  const auto navigation_start_time = base::TimeTicks::Now();
  remote_->Navigate(url, navigation_start_time);
}

void FencedFrameMPArchDelegate::Dispose() {
  DCHECK(remote_);
  remote_.reset();
  auto* fenced_frames = DocumentFencedFrames::Get(GetElement().GetDocument());
  DCHECK(fenced_frames);
  fenced_frames->DeregisterFencedFrame(&GetElement());
}

void FencedFrameMPArchDelegate::AttachLayoutTree() {
  if (GetElement().GetLayoutEmbeddedContent() && GetElement().ContentFrame()) {
    GetElement().SetEmbeddedContentView(GetElement().ContentFrame()->View());
  }
}

bool FencedFrameMPArchDelegate::SupportsFocus() {
  return true;
}

void FencedFrameMPArchDelegate::FreezeFrameSize() {
  // With MPArch, mark the layout as stale. Do this unconditionally because
  // we are rounding the size.
  GetElement().GetLayoutObject()->SetNeedsLayoutAndFullPaintInvalidation(
      "Froze MPArch fenced frame");

  // Stop the `ResizeObserver`. It is needed only to compute the
  // frozen size in MPArch. ShadowDOM stays subscribed in order to
  // update the CSS on the inner iframe element as the outer container's
  // size changes.
  GetElement().StopResizeObserver();
}

}  // namespace blink
