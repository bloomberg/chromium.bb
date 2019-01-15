// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_TESTING_TEST_RESOURCE_FETCHER_PROPERTIES_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_TESTING_TEST_RESOURCE_FETCHER_PROPERTIES_H_

#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher_properties.h"

namespace blink {

// TestResourceFetcherProperties is a ResourceFetcherProperties implementation
// for tests.
class TestResourceFetcherProperties final : public ResourceFetcherProperties {
 public:
  TestResourceFetcherProperties() = default;
  ~TestResourceFetcherProperties() override = default;

  // ResourceFetcherProperties implementation
  bool IsMainFrame() const override { return false; }
  ControllerServiceWorkerMode GetControllerServiceWorkerMode() const override {
    return ControllerServiceWorkerMode::kNoController;
  }
  int64_t ServiceWorkerId() const override {
    NOTREACHED();
    return 0;
  }
  bool IsPaused() const override { return false; }
  bool IsLoadComplete() const override { return load_complete_; }
  bool ShouldBlockLoadingMainResource() const override {
    return should_block_loading_main_resource_;
  }
  bool ShouldBlockLoadingSubResource() const override {
    return should_block_loading_sub_resource_;
  }

  void SetIsLoadComplete(bool value) { load_complete_ = value; }
  void SetShouldBlockLoadingMainResource(bool value) {
    should_block_loading_main_resource_ = value;
  }
  void SetShouldBlockLoadingSubResource(bool value) {
    should_block_loading_sub_resource_ = value;
  }

 private:
  bool load_complete_ = false;
  bool should_block_loading_main_resource_ = false;
  bool should_block_loading_sub_resource_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_TESTING_TEST_RESOURCE_FETCHER_PROPERTIES_H_
