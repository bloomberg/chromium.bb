// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/canvas/CanvasAsyncBlobCreator.h"

#include "core/html/ImageData.h"
#include "core/testing/DummyPageHolder.h"
#include "platform/testing/UnitTestHelpers.h"
#include "platform/wtf/Functional.h"
#include "public/platform/Platform.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

typedef CanvasAsyncBlobCreator::IdleTaskStatus IdleTaskStatus;

class MockCanvasAsyncBlobCreator : public CanvasAsyncBlobCreator {
 public:
  MockCanvasAsyncBlobCreator(DOMUint8ClampedArray* data,
                             const IntSize& size,
                             MimeType mime_type,
                             Document* document)
      : CanvasAsyncBlobCreator(data,
                               mime_type,
                               size,
                               nullptr,
                               0,
                               document,
                               nullptr) {}

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
                                      WTF::Closure,
                                      double delay_ms) override;
};

void MockCanvasAsyncBlobCreator::SignalAlternativeCodePathFinishedForTesting() {
  testing::ExitRunLoop();
}

void MockCanvasAsyncBlobCreator::PostDelayedTaskToCurrentThread(
    const WebTraceLocation& location,
    WTF::Closure task,
    double delay_ms) {
  DCHECK(IsMainThread());
  Platform::Current()->MainThread()->GetWebTaskRunner()->PostTask(
      location, std::move(task));
}

//==============================================================================
//=================================PNG==========================================
//==============================================================================

class MockCanvasAsyncBlobCreatorWithoutStartPng
    : public MockCanvasAsyncBlobCreator {
 public:
  MockCanvasAsyncBlobCreatorWithoutStartPng(DOMUint8ClampedArray* data,
                                            const IntSize& size,
                                            Document* document)
      : MockCanvasAsyncBlobCreator(data, size, kMimeTypePng, document) {}

 protected:
  void ScheduleInitiateEncoding(double) override {
    // Deliberately make scheduleInitiateEncoding do nothing so that idle
    // task never starts
  }
};

//==============================================================================

class MockCanvasAsyncBlobCreatorWithoutCompletePng
    : public MockCanvasAsyncBlobCreator {
 public:
  MockCanvasAsyncBlobCreatorWithoutCompletePng(DOMUint8ClampedArray* data,
                                               const IntSize& size,
                                               Document* document)
      : MockCanvasAsyncBlobCreator(data, size, kMimeTypePng, document) {}

 protected:
  void ScheduleInitiateEncoding(double quality) override {
    Platform::Current()->MainThread()->GetWebTaskRunner()->PostTask(
        BLINK_FROM_HERE,
        WTF::Bind(
            &MockCanvasAsyncBlobCreatorWithoutCompletePng::InitiateEncoding,
            WrapPersistent(this), quality, std::numeric_limits<double>::max()));
  }

  void IdleEncodeRows(double deadline_seconds) override {
    // Deliberately make idleEncodeRows do nothing so that idle task never
    // completes
  }
};

//==============================================================================
//=================================JPEG=========================================
//==============================================================================

class MockCanvasAsyncBlobCreatorWithoutStartJpeg
    : public MockCanvasAsyncBlobCreator {
 public:
  MockCanvasAsyncBlobCreatorWithoutStartJpeg(DOMUint8ClampedArray* data,
                                             const IntSize& size,
                                             Document* document)
      : MockCanvasAsyncBlobCreator(data, size, kMimeTypeJpeg, document) {}

 protected:
  void ScheduleInitiateEncoding(double) override {
    // Deliberately make scheduleInitiateEncoding do nothing so that idle
    // task never starts
  }
};

//==============================================================================

class MockCanvasAsyncBlobCreatorWithoutCompleteJpeg
    : public MockCanvasAsyncBlobCreator {
 public:
  MockCanvasAsyncBlobCreatorWithoutCompleteJpeg(DOMUint8ClampedArray* data,
                                                const IntSize& size,
                                                Document* document)
      : MockCanvasAsyncBlobCreator(data, size, kMimeTypeJpeg, document) {}

 protected:
  void ScheduleInitiateEncoding(double quality) override {
    Platform::Current()->MainThread()->GetWebTaskRunner()->PostTask(
        BLINK_FROM_HERE,
        WTF::Bind(
            &MockCanvasAsyncBlobCreatorWithoutCompleteJpeg::InitiateEncoding,
            WrapPersistent(this), quality, std::numeric_limits<double>::max()));
  }

  void IdleEncodeRows(double deadline_seconds) override {
    // Deliberately make idleEncodeRows do nothing so that idle task never
    // completes
  }
};

//==============================================================================

class CanvasAsyncBlobCreatorTest : public ::testing::Test {
 public:
  // Png unit tests
  void PrepareMockCanvasAsyncBlobCreatorWithoutStartPng();
  void PrepareMockCanvasAsyncBlobCreatorWithoutCompletePng();
  void PrepareMockCanvasAsyncBlobCreatorFailPng();

  // Jpeg unit tests
  void PrepareMockCanvasAsyncBlobCreatorWithoutStartJpeg();
  void PrepareMockCanvasAsyncBlobCreatorWithoutCompleteJpeg();
  void PrepareMockCanvasAsyncBlobCreatorFailJpeg();

 protected:
  CanvasAsyncBlobCreatorTest();
  MockCanvasAsyncBlobCreator* AsyncBlobCreator() {
    return async_blob_creator_.Get();
  }
  void TearDown() override;

 private:
  Document& GetDocument() { return dummy_page_holder_->GetDocument(); }

  Persistent<MockCanvasAsyncBlobCreator> async_blob_creator_;
  std::unique_ptr<DummyPageHolder> dummy_page_holder_;
};

CanvasAsyncBlobCreatorTest::CanvasAsyncBlobCreatorTest() {
  dummy_page_holder_ = DummyPageHolder::Create();
}

void CanvasAsyncBlobCreatorTest::
    PrepareMockCanvasAsyncBlobCreatorWithoutStartPng() {
  IntSize test_size(20, 20);
  ImageData* image_data = ImageData::Create(test_size);

  async_blob_creator_ = new MockCanvasAsyncBlobCreatorWithoutStartPng(
      image_data->data(), test_size, &GetDocument());
}

void CanvasAsyncBlobCreatorTest::
    PrepareMockCanvasAsyncBlobCreatorWithoutCompletePng() {
  IntSize test_size(20, 20);
  ImageData* image_data = ImageData::Create(test_size);

  async_blob_creator_ = new MockCanvasAsyncBlobCreatorWithoutCompletePng(
      image_data->data(), test_size, &GetDocument());
}

void CanvasAsyncBlobCreatorTest::PrepareMockCanvasAsyncBlobCreatorFailPng() {
  IntSize test_size(0, 0);
  ImageData* image_data = ImageData::CreateForTest(test_size);

  // We reuse the class MockCanvasAsyncBlobCreatorWithoutCompletePng because
  // this test case is expected to fail at initialization step before
  // completion.
  async_blob_creator_ = new MockCanvasAsyncBlobCreatorWithoutCompletePng(
      image_data->data(), test_size, &GetDocument());
}

void CanvasAsyncBlobCreatorTest::
    PrepareMockCanvasAsyncBlobCreatorWithoutStartJpeg() {
  IntSize test_size(20, 20);
  ImageData* image_data = ImageData::Create(test_size);

  async_blob_creator_ = new MockCanvasAsyncBlobCreatorWithoutStartJpeg(
      image_data->data(), test_size, &GetDocument());
}

void CanvasAsyncBlobCreatorTest::
    PrepareMockCanvasAsyncBlobCreatorWithoutCompleteJpeg() {
  IntSize test_size(20, 20);
  ImageData* image_data = ImageData::Create(test_size);

  async_blob_creator_ = new MockCanvasAsyncBlobCreatorWithoutCompleteJpeg(
      image_data->data(), test_size, &GetDocument());
}

void CanvasAsyncBlobCreatorTest::PrepareMockCanvasAsyncBlobCreatorFailJpeg() {
  IntSize test_size(0, 0);
  ImageData* image_data = ImageData::CreateForTest(test_size);

  // We reuse the class MockCanvasAsyncBlobCreatorWithoutCompleteJpeg because
  // this test case is expected to fail at initialization step before
  // completion.
  async_blob_creator_ = new MockCanvasAsyncBlobCreatorWithoutCompleteJpeg(
      image_data->data(), test_size, &GetDocument());
}

void CanvasAsyncBlobCreatorTest::TearDown() {
  async_blob_creator_ = nullptr;
}

//==============================================================================

TEST_F(CanvasAsyncBlobCreatorTest,
       PngIdleTaskNotStartedWhenStartTimeoutEventHappens) {
  // This test mocks the scenario when idle task is not started when the
  // StartTimeoutEvent is inspecting the idle task status.
  // The whole image encoding process (including initialization)  will then
  // become carried out in the alternative code path instead.
  this->PrepareMockCanvasAsyncBlobCreatorWithoutStartPng();
  EXPECT_CALL(*(AsyncBlobCreator()),
              SignalTaskSwitchInStartTimeoutEventForTesting());

  this->AsyncBlobCreator()->ScheduleAsyncBlobCreation(true);
  testing::EnterRunLoop();

  ::testing::Mock::VerifyAndClearExpectations(AsyncBlobCreator());
  EXPECT_EQ(IdleTaskStatus::kIdleTaskSwitchedToImmediateTask,
            this->AsyncBlobCreator()->GetIdleTaskStatus());
}

TEST_F(CanvasAsyncBlobCreatorTest,
       PngIdleTaskNotCompletedWhenCompleteTimeoutEventHappens) {
  // This test mocks the scenario when idle task is not completed when the
  // CompleteTimeoutEvent is inspecting the idle task status.
  // The remaining image encoding process (excluding initialization)  will
  // then become carried out in the alternative code path instead.
  this->PrepareMockCanvasAsyncBlobCreatorWithoutCompletePng();
  EXPECT_CALL(*(AsyncBlobCreator()),
              SignalTaskSwitchInCompleteTimeoutEventForTesting());

  this->AsyncBlobCreator()->ScheduleAsyncBlobCreation(true);
  testing::EnterRunLoop();

  ::testing::Mock::VerifyAndClearExpectations(AsyncBlobCreator());
  EXPECT_EQ(IdleTaskStatus::kIdleTaskSwitchedToImmediateTask,
            this->AsyncBlobCreator()->GetIdleTaskStatus());
}

TEST_F(CanvasAsyncBlobCreatorTest,
       PngIdleTaskFailedWhenStartTimeoutEventHappens) {
  // This test mocks the scenario when idle task is not failed during when
  // either the StartTimeoutEvent or the CompleteTimeoutEvent is inspecting
  // the idle task status.
  this->PrepareMockCanvasAsyncBlobCreatorFailPng();

  this->AsyncBlobCreator()->ScheduleAsyncBlobCreation(true);
  testing::EnterRunLoop();

  EXPECT_EQ(IdleTaskStatus::kIdleTaskFailed,
            this->AsyncBlobCreator()->GetIdleTaskStatus());
}

// The below 3 unit tests have exactly same workflow as the above 3 unit tests
// except that they are encoding on JPEG image formats instead of PNG.
TEST_F(CanvasAsyncBlobCreatorTest,
       JpegIdleTaskNotStartedWhenStartTimeoutEventHappens) {
  this->PrepareMockCanvasAsyncBlobCreatorWithoutStartJpeg();
  EXPECT_CALL(*(AsyncBlobCreator()),
              SignalTaskSwitchInStartTimeoutEventForTesting());

  this->AsyncBlobCreator()->ScheduleAsyncBlobCreation(1.0);
  testing::EnterRunLoop();

  ::testing::Mock::VerifyAndClearExpectations(AsyncBlobCreator());
  EXPECT_EQ(IdleTaskStatus::kIdleTaskSwitchedToImmediateTask,
            this->AsyncBlobCreator()->GetIdleTaskStatus());
}

TEST_F(CanvasAsyncBlobCreatorTest,
       JpegIdleTaskNotCompletedWhenCompleteTimeoutEventHappens) {
  this->PrepareMockCanvasAsyncBlobCreatorWithoutCompleteJpeg();
  EXPECT_CALL(*(AsyncBlobCreator()),
              SignalTaskSwitchInCompleteTimeoutEventForTesting());

  this->AsyncBlobCreator()->ScheduleAsyncBlobCreation(1.0);
  testing::EnterRunLoop();

  ::testing::Mock::VerifyAndClearExpectations(AsyncBlobCreator());
  EXPECT_EQ(IdleTaskStatus::kIdleTaskSwitchedToImmediateTask,
            this->AsyncBlobCreator()->GetIdleTaskStatus());
}

TEST_F(CanvasAsyncBlobCreatorTest,
       JpegIdleTaskFailedWhenStartTimeoutEventHappens) {
  this->PrepareMockCanvasAsyncBlobCreatorFailJpeg();

  this->AsyncBlobCreator()->ScheduleAsyncBlobCreation(1.0);
  testing::EnterRunLoop();

  EXPECT_EQ(IdleTaskStatus::kIdleTaskFailed,
            this->AsyncBlobCreator()->GetIdleTaskStatus());
}
}
