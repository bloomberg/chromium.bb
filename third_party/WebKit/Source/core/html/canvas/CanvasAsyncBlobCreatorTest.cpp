// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/canvas/CanvasAsyncBlobCreator.h"

#include "core/html/ImageData.h"
#include "core/testing/PageTestBase.h"
#include "platform/testing/UnitTestHelpers.h"
#include "platform/wtf/Functional.h"
#include "public/platform/Platform.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkSurface.h"

namespace blink {

typedef CanvasAsyncBlobCreator::IdleTaskStatus IdleTaskStatus;

class MockCanvasAsyncBlobCreator : public CanvasAsyncBlobCreator {
 public:
  MockCanvasAsyncBlobCreator(scoped_refptr<StaticBitmapImage> image,
                             MimeType mime_type,
                             Document* document,
                             bool fail_encoder_initialization = false)
      : CanvasAsyncBlobCreator(image,
                               mime_type,
                               nullptr,
                               0,
                               document,
                               nullptr) {
    if (fail_encoder_initialization)
      fail_encoder_initialization_for_test_ = true;
  }

  CanvasAsyncBlobCreator::IdleTaskStatus GetIdleTaskStatus() {
    return idle_task_status_;
  }

  MOCK_METHOD0(SignalTaskSwitchInStartTimeoutEventForTesting, void());
  MOCK_METHOD0(SignalTaskSwitchInCompleteTimeoutEventForTesting, void());

 protected:
  void CreateBlobAndReturnResult() override {}
  void CreateNullAndReturnResult() override {}
  void SignalAlternativeCodePathFinishedForTesting() override;
  void PostDelayedTaskToCurrentThread(const WebTraceLocation&,
                                      base::OnceClosure,
                                      double delay_ms) override;
};

void MockCanvasAsyncBlobCreator::SignalAlternativeCodePathFinishedForTesting() {
  testing::ExitRunLoop();
}

void MockCanvasAsyncBlobCreator::PostDelayedTaskToCurrentThread(
    const WebTraceLocation& location,
    base::OnceClosure task,
    double delay_ms) {
  DCHECK(IsMainThread());
  Platform::Current()->MainThread()->GetWebTaskRunner()->PostTask(
      location, std::move(task));
}

//==============================================================================

class MockCanvasAsyncBlobCreatorWithoutStart
    : public MockCanvasAsyncBlobCreator {
 public:
  MockCanvasAsyncBlobCreatorWithoutStart(scoped_refptr<StaticBitmapImage> image,
                                         Document* document)
      : MockCanvasAsyncBlobCreator(image, kMimeTypePng, document) {}

 protected:
  void ScheduleInitiateEncoding(double) override {
    // Deliberately make scheduleInitiateEncoding do nothing so that idle
    // task never starts
  }
};

//==============================================================================

class MockCanvasAsyncBlobCreatorWithoutComplete
    : public MockCanvasAsyncBlobCreator {
 public:
  MockCanvasAsyncBlobCreatorWithoutComplete(
      scoped_refptr<StaticBitmapImage> image,
      Document* document,
      bool fail_encoder_initialization = false)
      : MockCanvasAsyncBlobCreator(image,
                                   kMimeTypePng,
                                   document,
                                   fail_encoder_initialization) {}

 protected:
  void ScheduleInitiateEncoding(double quality) override {
    Platform::Current()->MainThread()->GetWebTaskRunner()->PostTask(
        BLINK_FROM_HERE,
        WTF::Bind(&MockCanvasAsyncBlobCreatorWithoutComplete::InitiateEncoding,
                  WrapPersistent(this), quality,
                  std::numeric_limits<double>::max()));
  }

  void IdleEncodeRows(double deadline_seconds) override {
    // Deliberately make idleEncodeRows do nothing so that idle task never
    // completes
  }
};

//==============================================================================

class CanvasAsyncBlobCreatorTest : public PageTestBase {
 public:
  void PrepareMockCanvasAsyncBlobCreatorWithoutStart();
  void PrepareMockCanvasAsyncBlobCreatorWithoutComplete();
  void PrepareMockCanvasAsyncBlobCreatorFail();

 protected:
  CanvasAsyncBlobCreatorTest();
  MockCanvasAsyncBlobCreator* AsyncBlobCreator() {
    return async_blob_creator_.Get();
  }
  void TearDown() override;

 private:

  Persistent<MockCanvasAsyncBlobCreator> async_blob_creator_;
};

CanvasAsyncBlobCreatorTest::CanvasAsyncBlobCreatorTest() {
}

scoped_refptr<StaticBitmapImage> CreateTransparentImage(int width, int height) {
  sk_sp<SkSurface> surface = SkSurface::MakeRasterN32Premul(width, height);
  if (!surface)
    return nullptr;
  return StaticBitmapImage::Create(surface->makeImageSnapshot());
}

void CanvasAsyncBlobCreatorTest::
    PrepareMockCanvasAsyncBlobCreatorWithoutStart() {
  async_blob_creator_ = new MockCanvasAsyncBlobCreatorWithoutStart(
      CreateTransparentImage(20, 20), &GetDocument());
}

void CanvasAsyncBlobCreatorTest::
    PrepareMockCanvasAsyncBlobCreatorWithoutComplete() {
  async_blob_creator_ = new MockCanvasAsyncBlobCreatorWithoutComplete(
      CreateTransparentImage(20, 20), &GetDocument());
}

void CanvasAsyncBlobCreatorTest::PrepareMockCanvasAsyncBlobCreatorFail() {
  // We reuse the class MockCanvasAsyncBlobCreatorWithoutComplete because
  // this test case is expected to fail at initialization step before
  // completion.
  async_blob_creator_ = new MockCanvasAsyncBlobCreatorWithoutComplete(
      CreateTransparentImage(20, 20), &GetDocument(), true);
}

void CanvasAsyncBlobCreatorTest::TearDown() {
  async_blob_creator_ = nullptr;
}

//==============================================================================

TEST_F(CanvasAsyncBlobCreatorTest,
       IdleTaskNotStartedWhenStartTimeoutEventHappens) {
  // This test mocks the scenario when idle task is not started when the
  // StartTimeoutEvent is inspecting the idle task status.
  // The whole image encoding process (including initialization)  will then
  // become carried out in the alternative code path instead.
  PrepareMockCanvasAsyncBlobCreatorWithoutStart();
  EXPECT_CALL(*(AsyncBlobCreator()),
              SignalTaskSwitchInStartTimeoutEventForTesting());

  AsyncBlobCreator()->ScheduleAsyncBlobCreation(true);
  testing::EnterRunLoop();

  ::testing::Mock::VerifyAndClearExpectations(AsyncBlobCreator());
  EXPECT_EQ(IdleTaskStatus::kIdleTaskSwitchedToImmediateTask,
            AsyncBlobCreator()->GetIdleTaskStatus());
}

TEST_F(CanvasAsyncBlobCreatorTest,
       IdleTaskNotCompletedWhenCompleteTimeoutEventHappens) {
  // This test mocks the scenario when idle task is not completed when the
  // CompleteTimeoutEvent is inspecting the idle task status.
  // The remaining image encoding process (excluding initialization)  will
  // then become carried out in the alternative code path instead.
  PrepareMockCanvasAsyncBlobCreatorWithoutComplete();
  EXPECT_CALL(*(AsyncBlobCreator()),
              SignalTaskSwitchInCompleteTimeoutEventForTesting());

  AsyncBlobCreator()->ScheduleAsyncBlobCreation(true);
  testing::EnterRunLoop();

  ::testing::Mock::VerifyAndClearExpectations(AsyncBlobCreator());
  EXPECT_EQ(IdleTaskStatus::kIdleTaskSwitchedToImmediateTask,
            AsyncBlobCreator()->GetIdleTaskStatus());
}

TEST_F(CanvasAsyncBlobCreatorTest, IdleTaskFailedWhenStartTimeoutEventHappens) {
  // This test mocks the scenario when idle task is not failed during when
  // either the StartTimeoutEvent or the CompleteTimeoutEvent is inspecting
  // the idle task status.
  PrepareMockCanvasAsyncBlobCreatorFail();

  AsyncBlobCreator()->ScheduleAsyncBlobCreation(true);
  testing::EnterRunLoop();

  EXPECT_EQ(IdleTaskStatus::kIdleTaskFailed,
            AsyncBlobCreator()->GetIdleTaskStatus());
}

}
