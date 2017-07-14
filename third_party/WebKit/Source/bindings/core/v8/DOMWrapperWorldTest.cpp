// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/bindings/DOMWrapperWorld.h"

#include "bindings/core/v8/V8BindingForTesting.h"
#include "bindings/core/v8/V8Initializer.h"
#include "bindings/core/v8/WorkerV8Settings.h"
#include "core/workers/WorkerBackingThread.h"
#include "platform/CrossThreadFunctional.h"
#include "platform/WebTaskRunner.h"
#include "platform/WebThreadSupportingGC.h"
#include "platform/bindings/V8PerIsolateData.h"
#include "platform/testing/UnitTestHelpers.h"
#include "public/platform/Platform.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace {

Vector<RefPtr<DOMWrapperWorld>> CreateIsolatedWorlds(v8::Isolate* isolate) {
  Vector<RefPtr<DOMWrapperWorld>> worlds;
  worlds.push_back(DOMWrapperWorld::EnsureIsolatedWorld(
      isolate, DOMWrapperWorld::WorldId::kMainWorldId + 1));
  worlds.push_back(DOMWrapperWorld::EnsureIsolatedWorld(
      isolate, DOMWrapperWorld::WorldId::kIsolatedWorldIdLimit - 1));
  EXPECT_TRUE(worlds[0]->IsIsolatedWorld());
  EXPECT_TRUE(worlds[1]->IsIsolatedWorld());
  return worlds;
}

Vector<RefPtr<DOMWrapperWorld>> CreateWorlds(v8::Isolate* isolate) {
  Vector<RefPtr<DOMWrapperWorld>> worlds;
  worlds.push_back(
      DOMWrapperWorld::Create(isolate, DOMWrapperWorld::WorldType::kWorker));
  worlds.push_back(
      DOMWrapperWorld::Create(isolate, DOMWrapperWorld::WorldType::kWorker));
  worlds.push_back(DOMWrapperWorld::Create(
      isolate, DOMWrapperWorld::WorldType::kGarbageCollector));
  EXPECT_TRUE(worlds[0]->IsWorkerWorld());
  EXPECT_TRUE(worlds[1]->IsWorkerWorld());
  EXPECT_FALSE(worlds[2]->IsWorkerWorld());

  // World ids should be unique.
  HashSet<int> world_ids;
  EXPECT_TRUE(world_ids.insert(worlds[0]->GetWorldId()).is_new_entry);
  EXPECT_TRUE(world_ids.insert(worlds[1]->GetWorldId()).is_new_entry);
  EXPECT_TRUE(world_ids.insert(worlds[2]->GetWorldId()).is_new_entry);

  return worlds;
}

void WorkerThreadFunc(WorkerBackingThread* thread,
                      RefPtr<WebTaskRunner> main_thread_task_runner) {
  thread->InitializeOnBackingThread(WorkerV8Settings::Default());

  // Worlds on the main thread should not be visible from the worker thread.
  Vector<RefPtr<DOMWrapperWorld>> retrieved_worlds;
  DOMWrapperWorld::AllWorldsInCurrentThread(retrieved_worlds);
  EXPECT_TRUE(retrieved_worlds.IsEmpty());

  // Create worlds on the worker thread and verify them.
  Vector<RefPtr<DOMWrapperWorld>> worlds = CreateWorlds(thread->GetIsolate());
  DOMWrapperWorld::AllWorldsInCurrentThread(retrieved_worlds);
  EXPECT_EQ(worlds.size(), retrieved_worlds.size());
  retrieved_worlds.clear();

  // Dispose of the last world.
  worlds.pop_back();
  DOMWrapperWorld::AllWorldsInCurrentThread(retrieved_worlds);
  EXPECT_EQ(worlds.size(), retrieved_worlds.size());

  // Dispose of remaining worlds.
  for (RefPtr<DOMWrapperWorld>& world : worlds) {
    if (world->IsWorkerWorld())
      world->Dispose();
  }
  worlds.clear();

  thread->ShutdownOnBackingThread();
  main_thread_task_runner->PostTask(BLINK_FROM_HERE,
                                    CrossThreadBind(&testing::ExitRunLoop));
}

TEST(DOMWrapperWorldTest, Basic) {
  // Create the main world and verify it.
  DOMWrapperWorld& main_world = DOMWrapperWorld::MainWorld();
  EXPECT_TRUE(main_world.IsMainWorld());
  EXPECT_FALSE(DOMWrapperWorld::NonMainWorldsExistInMainThread());
  Vector<RefPtr<DOMWrapperWorld>> retrieved_worlds;
  DOMWrapperWorld::AllWorldsInCurrentThread(retrieved_worlds);
  EXPECT_EQ(1u, retrieved_worlds.size());
  EXPECT_TRUE(retrieved_worlds[0]->IsMainWorld());
  retrieved_worlds.clear();

  // Create isolated worlds and verify them.
  V8TestingScope scope;
  Vector<RefPtr<DOMWrapperWorld>> isolated_worlds =
      CreateIsolatedWorlds(scope.GetIsolate());
  EXPECT_TRUE(DOMWrapperWorld::NonMainWorldsExistInMainThread());
  DOMWrapperWorld::AllWorldsInCurrentThread(retrieved_worlds);
  EXPECT_EQ(isolated_worlds.size() + 1, retrieved_worlds.size());

  // Create other worlds and verify them.
  Vector<RefPtr<DOMWrapperWorld>> worlds = CreateWorlds(scope.GetIsolate());
  EXPECT_TRUE(DOMWrapperWorld::NonMainWorldsExistInMainThread());
  retrieved_worlds.clear();
  DOMWrapperWorld::AllWorldsInCurrentThread(retrieved_worlds);
  EXPECT_EQ(isolated_worlds.size() + worlds.size() + 1,
            retrieved_worlds.size());
  retrieved_worlds.clear();

  // Start a worker thread and create worlds on that.
  std::unique_ptr<WorkerBackingThread> thread =
      WorkerBackingThread::Create("DOMWrapperWorld test thread");
  RefPtr<WebTaskRunner> main_thread_task_runner =
      Platform::Current()->CurrentThread()->GetWebTaskRunner();
  thread->BackingThread().PostTask(
      BLINK_FROM_HERE,
      CrossThreadBind(&WorkerThreadFunc, CrossThreadUnretained(thread.get()),
                      std::move(main_thread_task_runner)));
  testing::EnterRunLoop();

  // Worlds on the worker thread should not be visible from the main thread.
  EXPECT_TRUE(DOMWrapperWorld::NonMainWorldsExistInMainThread());
  DOMWrapperWorld::AllWorldsInCurrentThread(retrieved_worlds);
  EXPECT_EQ(isolated_worlds.size() + worlds.size() + 1,
            retrieved_worlds.size());
  retrieved_worlds.clear();

  // Dispose of the isolated worlds.
  isolated_worlds.clear();
  EXPECT_TRUE(DOMWrapperWorld::NonMainWorldsExistInMainThread());
  DOMWrapperWorld::AllWorldsInCurrentThread(retrieved_worlds);
  EXPECT_EQ(worlds.size() + 1, retrieved_worlds.size());
  retrieved_worlds.clear();

  // Dispose of the other worlds.
  for (RefPtr<DOMWrapperWorld>& world : worlds) {
    if (world->IsWorkerWorld())
      world->Dispose();
  }
  worlds.clear();
  EXPECT_FALSE(DOMWrapperWorld::NonMainWorldsExistInMainThread());
  DOMWrapperWorld::AllWorldsInCurrentThread(retrieved_worlds);
  EXPECT_EQ(1u, retrieved_worlds.size());
}

}  // namespace
}  // namespace blink
