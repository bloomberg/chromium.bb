// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/DOMWrapperWorld.h"

#include "bindings/core/v8/V8BindingForTesting.h"
#include "bindings/core/v8/V8Initializer.h"
#include "bindings/core/v8/V8PerIsolateData.h"
#include "core/workers/WorkerBackingThread.h"
#include "platform/CrossThreadFunctional.h"
#include "platform/WebTaskRunner.h"
#include "platform/WebThreadSupportingGC.h"
#include "platform/testing/UnitTestHelpers.h"
#include "public/platform/Platform.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace {

Vector<RefPtr<DOMWrapperWorld>> createIsolatedWorlds(v8::Isolate* isolate) {
  Vector<RefPtr<DOMWrapperWorld>> worlds;
  worlds.push_back(DOMWrapperWorld::ensureIsolatedWorld(
      isolate, DOMWrapperWorld::WorldId::MainWorldId + 1));
  worlds.push_back(DOMWrapperWorld::ensureIsolatedWorld(
      isolate, DOMWrapperWorld::WorldId::IsolatedWorldIdLimit - 1));
  EXPECT_TRUE(worlds[0]->isIsolatedWorld());
  EXPECT_TRUE(worlds[1]->isIsolatedWorld());
  return worlds;
}

Vector<RefPtr<DOMWrapperWorld>> createWorlds(v8::Isolate* isolate) {
  Vector<RefPtr<DOMWrapperWorld>> worlds;
  worlds.push_back(
      DOMWrapperWorld::create(isolate, DOMWrapperWorld::WorldType::Worker));
  worlds.push_back(
      DOMWrapperWorld::create(isolate, DOMWrapperWorld::WorldType::Worker));
  worlds.push_back(DOMWrapperWorld::create(
      isolate, DOMWrapperWorld::WorldType::GarbageCollector));
  EXPECT_TRUE(worlds[0]->isWorkerWorld());
  EXPECT_TRUE(worlds[1]->isWorkerWorld());
  EXPECT_FALSE(worlds[2]->isWorkerWorld());

  // World ids should be unique.
  HashSet<int> worldIds;
  EXPECT_TRUE(worldIds.insert(worlds[0]->worldId()).isNewEntry);
  EXPECT_TRUE(worldIds.insert(worlds[1]->worldId()).isNewEntry);
  EXPECT_TRUE(worldIds.insert(worlds[2]->worldId()).isNewEntry);

  return worlds;
}

void workerThreadFunc(WorkerBackingThread* thread,
                      RefPtr<WebTaskRunner> mainThreadTaskRunner) {
  thread->initialize();

  // Worlds on the main thread should not be visible from the worker thread.
  Vector<RefPtr<DOMWrapperWorld>> retrievedWorlds;
  DOMWrapperWorld::allWorldsInCurrentThread(retrievedWorlds);
  EXPECT_TRUE(retrievedWorlds.isEmpty());

  // Create worlds on the worker thread and verify them.
  Vector<RefPtr<DOMWrapperWorld>> worlds = createWorlds(thread->isolate());
  DOMWrapperWorld::allWorldsInCurrentThread(retrievedWorlds);
  EXPECT_EQ(worlds.size(), retrievedWorlds.size());
  retrievedWorlds.clear();

  // Dispose of the last world.
  worlds.pop_back();
  DOMWrapperWorld::allWorldsInCurrentThread(retrievedWorlds);
  EXPECT_EQ(worlds.size(), retrievedWorlds.size());

  // Dispose of remaining worlds.
  for (RefPtr<DOMWrapperWorld>& world : worlds) {
    if (world->isWorkerWorld())
      world->dispose();
  }
  worlds.clear();

  thread->shutdown();
  mainThreadTaskRunner->postTask(BLINK_FROM_HERE,
                                 crossThreadBind(&testing::exitRunLoop));
}

TEST(DOMWrapperWorldTest, Basic) {
  // Initially, there should be no worlds.
  EXPECT_FALSE(DOMWrapperWorld::nonMainWorldsExistInMainThread());
  Vector<RefPtr<DOMWrapperWorld>> retrievedWorlds;
  DOMWrapperWorld::allWorldsInCurrentThread(retrievedWorlds);
  EXPECT_TRUE(retrievedWorlds.isEmpty());

  // Create the main world and verify it.
  DOMWrapperWorld& mainWorld = DOMWrapperWorld::mainWorld();
  EXPECT_TRUE(mainWorld.isMainWorld());
  EXPECT_FALSE(DOMWrapperWorld::nonMainWorldsExistInMainThread());
  DOMWrapperWorld::allWorldsInCurrentThread(retrievedWorlds);
  EXPECT_EQ(1u, retrievedWorlds.size());
  EXPECT_TRUE(retrievedWorlds[0]->isMainWorld());
  retrievedWorlds.clear();

  // Create isolated worlds and verify them.
  V8TestingScope scope;
  Vector<RefPtr<DOMWrapperWorld>> isolatedWorlds =
      createIsolatedWorlds(scope.isolate());
  EXPECT_TRUE(DOMWrapperWorld::nonMainWorldsExistInMainThread());
  DOMWrapperWorld::allWorldsInCurrentThread(retrievedWorlds);
  EXPECT_EQ(isolatedWorlds.size() + 1, retrievedWorlds.size());

  // Create other worlds and verify them.
  Vector<RefPtr<DOMWrapperWorld>> worlds = createWorlds(scope.isolate());
  EXPECT_TRUE(DOMWrapperWorld::nonMainWorldsExistInMainThread());
  retrievedWorlds.clear();
  DOMWrapperWorld::allWorldsInCurrentThread(retrievedWorlds);
  EXPECT_EQ(isolatedWorlds.size() + worlds.size() + 1, retrievedWorlds.size());
  retrievedWorlds.clear();

  // Start a worker thread and create worlds on that.
  std::unique_ptr<WorkerBackingThread> thread =
      WorkerBackingThread::create("DOMWrapperWorld test thread");
  RefPtr<WebTaskRunner> mainThreadTaskRunner =
      Platform::current()->currentThread()->getWebTaskRunner();
  thread->backingThread().postTask(
      BLINK_FROM_HERE,
      crossThreadBind(&workerThreadFunc, crossThreadUnretained(thread.get()),
                      std::move(mainThreadTaskRunner)));
  testing::enterRunLoop();

  // Worlds on the worker thread should not be visible from the main thread.
  EXPECT_TRUE(DOMWrapperWorld::nonMainWorldsExistInMainThread());
  DOMWrapperWorld::allWorldsInCurrentThread(retrievedWorlds);
  EXPECT_EQ(isolatedWorlds.size() + worlds.size() + 1, retrievedWorlds.size());
  retrievedWorlds.clear();

  // Dispose of the isolated worlds.
  isolatedWorlds.clear();
  EXPECT_TRUE(DOMWrapperWorld::nonMainWorldsExistInMainThread());
  DOMWrapperWorld::allWorldsInCurrentThread(retrievedWorlds);
  EXPECT_EQ(worlds.size() + 1, retrievedWorlds.size());
  retrievedWorlds.clear();

  // Dispose of the other worlds.
  for (RefPtr<DOMWrapperWorld>& world : worlds) {
    if (world->isWorkerWorld())
      world->dispose();
  }
  worlds.clear();
  EXPECT_FALSE(DOMWrapperWorld::nonMainWorldsExistInMainThread());
  DOMWrapperWorld::allWorldsInCurrentThread(retrievedWorlds);
  EXPECT_EQ(1u, retrievedWorlds.size());
}

}  // namespace
}  // namespace blink
