// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/wake_lock/wake_lock_controller.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/modules/wake_lock/wake_lock_test_utils.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "v8/include/v8.h"

namespace blink {

TEST(WakeLockControllerTest, AcquireScreenWakeLock) {
  MockWakeLockService wake_lock_service;
  WakeLockTestingContext context(&wake_lock_service);
  auto& controller = WakeLockController::From(*context.GetDocument());

  controller.AcquireWakeLock(
      WakeLockType::kScreen,
      MakeGarbageCollected<ScriptPromiseResolver>(context.GetScriptState()));
  MockWakeLock& screen_lock =
      wake_lock_service.get_wake_lock(WakeLockType::kScreen);
  screen_lock.WaitForRequest();

  EXPECT_TRUE(screen_lock.is_acquired());
}

TEST(WakeLockController, AcquireSystemWakeLock) {
  MockWakeLockService wake_lock_service;
  WakeLockTestingContext context(&wake_lock_service);
  auto& controller = WakeLockController::From(*context.GetDocument());

  controller.AcquireWakeLock(
      WakeLockType::kSystem,
      MakeGarbageCollected<ScriptPromiseResolver>(context.GetScriptState()));
  MockWakeLock& system_lock =
      wake_lock_service.get_wake_lock(WakeLockType::kSystem);
  system_lock.WaitForRequest();

  EXPECT_TRUE(system_lock.is_acquired());
}

TEST(WakeLockControllerTest, AcquireMultipleLocks) {
  MockWakeLockService wake_lock_service;
  WakeLockTestingContext context(&wake_lock_service);
  auto& controller = WakeLockController::From(*context.GetDocument());

  controller.AcquireWakeLock(
      WakeLockType::kScreen,
      MakeGarbageCollected<ScriptPromiseResolver>(context.GetScriptState()));
  controller.AcquireWakeLock(
      WakeLockType::kSystem,
      MakeGarbageCollected<ScriptPromiseResolver>(context.GetScriptState()));
  controller.AcquireWakeLock(
      WakeLockType::kSystem,
      MakeGarbageCollected<ScriptPromiseResolver>(context.GetScriptState()));

  MockWakeLock& screen_lock =
      wake_lock_service.get_wake_lock(WakeLockType::kScreen);
  screen_lock.WaitForRequest();
  MockWakeLock& system_lock =
      wake_lock_service.get_wake_lock(WakeLockType::kSystem);
  system_lock.WaitForRequest();

  EXPECT_TRUE(screen_lock.is_acquired());
  EXPECT_TRUE(system_lock.is_acquired());
}

TEST(WakeLockControllerTest, ReleaseWakeLockWithWrongType) {
  MockWakeLockService wake_lock_service;
  WakeLockTestingContext context(&wake_lock_service);
  auto& controller = WakeLockController::From(*context.GetDocument());

  MockWakeLock& screen_lock =
      wake_lock_service.get_wake_lock(WakeLockType::kScreen);
  MockWakeLock& system_lock =
      wake_lock_service.get_wake_lock(WakeLockType::kSystem);

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(context.GetScriptState());
  ScriptPromise promise = resolver->Promise();

  controller.AcquireWakeLock(WakeLockType::kScreen, resolver);
  screen_lock.WaitForRequest();
  controller.ReleaseWakeLock(WakeLockType::kSystem, resolver);

  test::RunPendingTasks();

  EXPECT_EQ(v8::Promise::kPending, GetScriptPromiseState(promise));
  EXPECT_TRUE(screen_lock.is_acquired());
  EXPECT_FALSE(system_lock.is_acquired());
}

TEST(WakeLockControllerTest, ReleaseWakeLock) {
  MockWakeLockService wake_lock_service;
  WakeLockTestingContext context(&wake_lock_service);
  auto& controller = WakeLockController::From(*context.GetDocument());

  MockWakeLock& screen_lock =
      wake_lock_service.get_wake_lock(WakeLockType::kScreen);

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(context.GetScriptState());
  ScriptPromise promise = resolver->Promise();

  controller.AcquireWakeLock(WakeLockType::kScreen, resolver);
  screen_lock.WaitForRequest();
  controller.ReleaseWakeLock(WakeLockType::kScreen, resolver);
  context.WaitForPromiseRejection(promise);
  screen_lock.WaitForCancelation();

  EXPECT_EQ(v8::Promise::kRejected, GetScriptPromiseState(promise));
  EXPECT_FALSE(screen_lock.is_acquired());
}

// This is actually part of WakeLock::request(), but it is easier to just test
// it here.
TEST(WakeLockControllerTest, AbortSignal) {
  MockWakeLockService wake_lock_service;
  WakeLockTestingContext context(&wake_lock_service);
  auto& controller = WakeLockController::From(*context.GetDocument());

  MockWakeLock& screen_lock =
      wake_lock_service.get_wake_lock(WakeLockType::kScreen);
  auto* screen_resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(context.GetScriptState());
  ScriptPromise screen_promise = screen_resolver->Promise();

  auto* abort_signal = MakeGarbageCollected<AbortSignal>(context.GetDocument());
  abort_signal->AddAlgorithm(WTF::Bind(
      &WakeLockController::ReleaseWakeLock, WrapWeakPersistent(&controller),
      WakeLockType::kScreen, WrapPersistent(screen_resolver)));

  controller.AcquireWakeLock(WakeLockType::kScreen, screen_resolver);
  screen_lock.WaitForRequest();

  abort_signal->SignalAbort();
  context.WaitForPromiseRejection(screen_promise);
  screen_lock.WaitForCancelation();

  EXPECT_EQ(v8::Promise::kRejected, GetScriptPromiseState(screen_promise));
  EXPECT_FALSE(screen_lock.is_acquired());
}

// https://w3c.github.io/wake-lock/#handling-document-loss-of-full-activity
TEST(WakeLockControllerTest, LossOfDocumentActivity) {
  MockWakeLockService wake_lock_service;
  WakeLockTestingContext context(&wake_lock_service);
  auto& controller = WakeLockController::From(*context.GetDocument());

  MockWakeLock& screen_lock =
      wake_lock_service.get_wake_lock(WakeLockType::kScreen);
  MockWakeLock& system_lock =
      wake_lock_service.get_wake_lock(WakeLockType::kSystem);

  // First, acquire a handful of locks of different types.
  auto* screen_resolver1 =
      MakeGarbageCollected<ScriptPromiseResolver>(context.GetScriptState());
  ScriptPromise screen_promise1 = screen_resolver1->Promise();
  auto* screen_resolver2 =
      MakeGarbageCollected<ScriptPromiseResolver>(context.GetScriptState());
  ScriptPromise screen_promise2 = screen_resolver2->Promise();
  auto* system_resolver1 =
      MakeGarbageCollected<ScriptPromiseResolver>(context.GetScriptState());
  ScriptPromise system_promise1 = system_resolver1->Promise();
  controller.AcquireWakeLock(WakeLockType::kScreen, screen_resolver1);
  controller.AcquireWakeLock(WakeLockType::kScreen, screen_resolver2);
  screen_lock.WaitForRequest();
  controller.AcquireWakeLock(WakeLockType::kSystem, system_resolver1);
  system_lock.WaitForRequest();

  // Now shut down our Document and make sure all [[ActiveLocks]] slots have
  // been cleared. We cannot check that the promises have been rejected because
  // ScriptPromiseResolver::Reject() will bail out if we no longer have a valid
  // execution context.
  context.GetDocument()->Shutdown();
  screen_lock.WaitForCancelation();
  system_lock.WaitForCancelation();

  EXPECT_FALSE(screen_lock.is_acquired());
  EXPECT_FALSE(system_lock.is_acquired());
}

// https://w3c.github.io/wake-lock/#handling-document-loss-of-visibility
TEST(WakeLockControllerTest, PageVisibilityHidden) {
  MockWakeLockService wake_lock_service;
  WakeLockTestingContext context(&wake_lock_service);
  auto& controller = WakeLockController::From(*context.GetDocument());

  MockWakeLock& screen_lock =
      wake_lock_service.get_wake_lock(WakeLockType::kScreen);
  auto* screen_resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(context.GetScriptState());
  ScriptPromise screen_promise = screen_resolver->Promise();

  MockWakeLock& system_lock =
      wake_lock_service.get_wake_lock(WakeLockType::kSystem);
  auto* system_resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(context.GetScriptState());
  ScriptPromise system_promise = system_resolver->Promise();

  controller.AcquireWakeLock(WakeLockType::kScreen, screen_resolver);
  screen_lock.WaitForRequest();
  controller.AcquireWakeLock(WakeLockType::kSystem, system_resolver);
  system_lock.WaitForRequest();

  context.GetDocument()->GetPage()->SetIsHidden(true, false);
  context.WaitForPromiseRejection(screen_promise);
  screen_lock.WaitForCancelation();

  EXPECT_EQ(v8::Promise::kRejected, GetScriptPromiseState(screen_promise));
  EXPECT_FALSE(screen_lock.is_acquired());
  EXPECT_EQ(v8::Promise::kPending, GetScriptPromiseState(system_promise));
  EXPECT_TRUE(system_lock.is_acquired());
}

// Check that hiding a page and signaling abort does not try to delete a
// screen lock twice.
TEST(WakeLockControllerTest, PageVisibilityAndAbortSignal) {
  MockWakeLockService wake_lock_service;
  WakeLockTestingContext context(&wake_lock_service);
  auto& controller = WakeLockController::From(*context.GetDocument());

  MockWakeLock& screen_lock =
      wake_lock_service.get_wake_lock(WakeLockType::kScreen);
  auto* screen_resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(context.GetScriptState());
  ScriptPromise screen_promise = screen_resolver->Promise();

  auto* abort_signal = MakeGarbageCollected<AbortSignal>(context.GetDocument());
  abort_signal->AddAlgorithm(WTF::Bind(
      &WakeLockController::ReleaseWakeLock, WrapWeakPersistent(&controller),
      WakeLockType::kScreen, WrapPersistent(screen_resolver)));

  controller.AcquireWakeLock(WakeLockType::kScreen, screen_resolver);
  screen_lock.WaitForRequest();

  context.GetDocument()->GetPage()->SetIsHidden(true, false);
  context.WaitForPromiseRejection(screen_promise);
  screen_lock.WaitForCancelation();

  EXPECT_EQ(v8::Promise::kRejected, GetScriptPromiseState(screen_promise));
  EXPECT_FALSE(screen_lock.is_acquired());

  abort_signal->SignalAbort();
  test::RunPendingTasks();

  EXPECT_FALSE(screen_lock.is_acquired());
}

}  // namespace blink
