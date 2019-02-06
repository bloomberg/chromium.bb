// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/animationworklet/animation_worklet_proxy_client.h"

#include <memory>

#include "base/memory/weak_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/graphics/animation_worklet_mutator_dispatcher_impl.h"

namespace blink {

class AnimationWorkletProxyClientTest : public RenderingTest {
 public:
  AnimationWorkletProxyClientTest()
      : RenderingTest(SingleChildLocalFrameClient::Create()) {}
};

TEST_F(AnimationWorkletProxyClientTest,
       AnimationWorkletProxyClientConstruction) {
  AnimationWorkletProxyClient* proxy_client =
      MakeGarbageCollected<AnimationWorkletProxyClient>(1, nullptr, nullptr,
                                                        nullptr, nullptr);
  EXPECT_TRUE(proxy_client->mutator_items_.IsEmpty());

  auto mutator = std::make_unique<AnimationWorkletMutatorDispatcherImpl>(true);
  scoped_refptr<base::SingleThreadTaskRunner> mutator_task_runner =
      mutator->GetTaskRunner();
  base::WeakPtrFactory<AnimationWorkletMutatorDispatcherImpl>
      mutator_dispatcher_factory(mutator.get());

  proxy_client = MakeGarbageCollected<AnimationWorkletProxyClient>(
      1, nullptr, nullptr, mutator_dispatcher_factory.GetWeakPtr(),
      mutator_task_runner);
  EXPECT_EQ(proxy_client->mutator_items_.size(), 1u);

  proxy_client = MakeGarbageCollected<AnimationWorkletProxyClient>(
      1, mutator_dispatcher_factory.GetWeakPtr(), mutator_task_runner,
      mutator_dispatcher_factory.GetWeakPtr(), mutator_task_runner);
  EXPECT_EQ(proxy_client->mutator_items_.size(), 2u);
}

}  // namespace blink
