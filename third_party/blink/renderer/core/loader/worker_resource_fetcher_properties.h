// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_WORKER_RESOURCE_FETCHER_PROPERTIES_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_WORKER_RESOURCE_FETCHER_PROPERTIES_H_

#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher_properties.h"

namespace blink {

// WorkerResourceFetcherProperties is a ResourceFetcherProperties implementation
// for workers and worklets.
class WorkerResourceFetcherProperties final : public ResourceFetcherProperties {
 public:
  WorkerResourceFetcherProperties() = default;
  ~WorkerResourceFetcherProperties() override = default;

  // ResourceFetcherProperties implementation
  bool IsMainFrame() const override { return false; }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_WORKER_RESOURCE_FETCHER_PROPERTIES_H_
