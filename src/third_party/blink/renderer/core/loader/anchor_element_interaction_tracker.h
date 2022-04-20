// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_ANCHOR_ELEMENT_INTERACTION_TRACKER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_ANCHOR_ELEMENT_INTERACTION_TRACKER_H_

#include "third_party/blink/public/mojom/loader/anchor_element_interaction_host.mojom-blink.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"

namespace blink {

class AnchorElementListener;
class Document;
class KURL;

// Creates an event listener for mousedown events anywhere on a document.
// If there is one, the listener will retrieve the valid href from the anchor
// element from the event. The tracker will report the href value to the
// browser process via Mojo. The browser process can use this information
// to preload (e.g. preconnect the origin) the URL in order to improve
// performance.
class AnchorElementInteractionTracker
    : public GarbageCollected<AnchorElementInteractionTracker> {
 public:
  explicit AnchorElementInteractionTracker(Document& document);

  static bool IsFeatureEnabled();

  void OnPointerDown(const KURL& url);

  void Trace(Visitor* visitor) const;

 private:
  Member<AnchorElementListener> anchor_element_listener_;
  HeapMojoRemote<mojom::blink::AnchorElementInteractionHost> interaction_host_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_ANCHOR_ELEMENT_INTERACTION_TRACKER_H_
