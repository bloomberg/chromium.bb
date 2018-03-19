// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WorkerFetchTestHelper_h
#define WorkerFetchTestHelper_h

#include "core/loader/modulescript/ModuleScriptCreationParams.h"
#include "core/workers/WorkerOrWorkletModuleFetchCoordinator.h"
#include "platform/loader/fetch/ResourceFetcher.h"
#include "platform/loader/testing/FetchTestingPlatformSupport.h"
#include "platform/testing/TestingPlatformSupport.h"
#include "platform/testing/UnitTestHelpers.h"
#include "platform/wtf/Optional.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class ClientImpl final : public GarbageCollectedFinalized<ClientImpl>,
                         public WorkerOrWorkletModuleFetchCoordinator::Client {
  USING_GARBAGE_COLLECTED_MIXIN(ClientImpl);

 public:
  enum class Result { kInitial, kOK, kFailed };

  void OnFetched(const ModuleScriptCreationParams& params) override {
    ASSERT_EQ(Result::kInitial, result_);
    result_ = Result::kOK;
    params_.emplace(params);
  }

  void OnFailed() override {
    ASSERT_EQ(Result::kInitial, result_);
    result_ = Result::kFailed;
  }

  Result GetResult() const { return result_; }
  WTF::Optional<ModuleScriptCreationParams> GetParams() const {
    return params_;
  }

 private:
  Result result_ = Result::kInitial;
  WTF::Optional<ModuleScriptCreationParams> params_;
};

}  // namespace blink

#endif  // WorkerFetchTestHelper_h
