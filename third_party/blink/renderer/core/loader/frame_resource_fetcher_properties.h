// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_FRAME_RESOURCE_FETCHER_PROPERTIES_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_FRAME_RESOURCE_FETCHER_PROPERTIES_H_

#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher_properties.h"

namespace blink {

class LocalFrame;

// FrameResourceFetcherProperties is a ResourceFetcherProperties implementation
// for Frame.
class FrameResourceFetcherProperties final : public ResourceFetcherProperties {
 public:
  // |frame| must not be null.
  explicit FrameResourceFetcherProperties(LocalFrame* frame);
  ~FrameResourceFetcherProperties() override = default;

  void Trace(Visitor*) override;

  // ResourceFetcherProperties implementation
  bool IsMainFrame() const override;

 private:
  const Member<LocalFrame> frame_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_FRAME_RESOURCE_FETCHER_PROPERTIES_H_
