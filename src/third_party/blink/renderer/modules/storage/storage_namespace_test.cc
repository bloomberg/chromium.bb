// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/storage/storage_namespace.h"
#include <third_party/blink/renderer/modules/storage/storage_controller.h>

#include "base/task/post_task.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/modules/storage/testing/fake_area_source.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/uuid.h"

namespace blink {
namespace {

constexpr size_t kTestCacheLimit = 100;

TEST(StorageNamespaceTest, BasicStorageAreas) {
  const auto kOrigin = SecurityOrigin::CreateFromString("http://dom_storage1/");
  const auto kOrigin2 =
      SecurityOrigin::CreateFromString("http://dom_storage2/");
  const auto kOrigin3 =
      SecurityOrigin::CreateFromString("http://dom_storage3/");
  const String kKey("key");
  const String kValue("value");
  const String kSessionStorageNamespace("abcd");
  const KURL kPageUrl("http://dom_storage/page");
  Persistent<FakeAreaSource> source_area =
      MakeGarbageCollected<FakeAreaSource>(kPageUrl);

  StorageController::DomStorageConnection connection;
  ignore_result(connection.dom_storage_remote.BindNewPipeAndPassReceiver());
  StorageController controller(std::move(connection),
                               scheduler::GetSingleThreadTaskRunnerForTesting(),
                               kTestCacheLimit);

  StorageNamespace* localStorage =
      MakeGarbageCollected<StorageNamespace>(&controller);
  StorageNamespace* sessionStorage = MakeGarbageCollected<StorageNamespace>(
      &controller, kSessionStorageNamespace);

  EXPECT_FALSE(localStorage->IsSessionStorage());
  EXPECT_TRUE(sessionStorage->IsSessionStorage());

  auto cached_area1 = localStorage->GetCachedArea(kOrigin.get());
  cached_area1->RegisterSource(source_area);
  cached_area1->SetItem(kKey, kValue, source_area);
  auto cached_area2 = localStorage->GetCachedArea(kOrigin2.get());
  cached_area2->RegisterSource(source_area);
  cached_area2->SetItem(kKey, kValue, source_area);
  auto cached_area3 = sessionStorage->GetCachedArea(kOrigin3.get());
  cached_area3->RegisterSource(source_area);
  cached_area3->SetItem(kKey, kValue, source_area);

  EXPECT_EQ(cached_area1->GetItem(kKey), kValue);
  EXPECT_EQ(cached_area2->GetItem(kKey), kValue);
  EXPECT_EQ(cached_area3->GetItem(kKey), kValue);
}

}  // namespace
}  // namespace blink
