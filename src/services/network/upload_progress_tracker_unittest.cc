// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/upload_progress_tracker.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "net/base/upload_progress.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {
namespace {

class TestingUploadProgressTracker : public UploadProgressTracker {
 public:
  TestingUploadProgressTracker(
      const base::Location& location,
      UploadProgressReportCallback report_callback,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner)
      : UploadProgressTracker(location,
                              std::move(report_callback),
                              nullptr,
                              std::move(task_runner)),
        current_time_(base::TimeTicks::Now()) {}

  void set_upload_progress(const net::UploadProgress& upload_progress) {
    upload_progress_ = upload_progress;
  }

  void set_current_time(const base::TimeTicks& current_time) {
    current_time_ = current_time;
  }

 private:
  // UploadProgressTracker overrides.
  base::TimeTicks GetCurrentTime() const override { return current_time_; }
  net::UploadProgress GetUploadProgress() const override {
    return upload_progress_;
  }

  base::TimeTicks current_time_;
  net::UploadProgress upload_progress_;

  DISALLOW_COPY_AND_ASSIGN(TestingUploadProgressTracker);
};

}  // namespace

class UploadProgressTrackerTest : public ::testing::Test {
 public:
  UploadProgressTrackerTest()
      : task_runner_handle_(mock_task_runner_),
        upload_progress_tracker_(
            FROM_HERE,
            base::BindRepeating(
                &UploadProgressTrackerTest::OnUploadProgressReported,
                base::Unretained(this)),
            mock_task_runner_) {}

 private:
  void OnUploadProgressReported(const net::UploadProgress& progress) {
    ++report_count_;
    reported_position_ = progress.position();
    reported_total_size_ = progress.size();
  }

 protected:
  int report_count_ = 0;
  int64_t reported_position_ = 0;
  int64_t reported_total_size_ = 0;

  // Mocks the current thread's task runner which will also be used as the
  // UploadProgressTracker's task runner.
  scoped_refptr<base::TestMockTimeTaskRunner> mock_task_runner_ =
      new base::TestMockTimeTaskRunner;
  base::ThreadTaskRunnerHandle task_runner_handle_;

  TestingUploadProgressTracker upload_progress_tracker_;

  DISALLOW_COPY_AND_ASSIGN(UploadProgressTrackerTest);
};

TEST_F(UploadProgressTrackerTest, NoACK) {
  upload_progress_tracker_.set_upload_progress(net::UploadProgress(500, 1000));

  // The first timer task calls ReportUploadProgress.
  EXPECT_EQ(0, report_count_);
  mock_task_runner_->FastForwardBy(
      UploadProgressTracker::GetUploadProgressIntervalForTesting());
  EXPECT_EQ(1, report_count_);
  EXPECT_EQ(500, reported_position_);
  EXPECT_EQ(1000, reported_total_size_);

  upload_progress_tracker_.set_upload_progress(net::UploadProgress(750, 1000));

  // The second timer task does nothing, since the first report didn't send the
  // ACK.
  mock_task_runner_->FastForwardBy(
      UploadProgressTracker::GetUploadProgressIntervalForTesting());
  EXPECT_EQ(1, report_count_);
}

TEST_F(UploadProgressTrackerTest, NoUpload) {
  upload_progress_tracker_.set_upload_progress(net::UploadProgress(0, 0));

  // UploadProgressTracker does nothing on the empty upload content.
  EXPECT_EQ(0, report_count_);
  mock_task_runner_->FastForwardBy(
      UploadProgressTracker::GetUploadProgressIntervalForTesting());
  EXPECT_EQ(0, report_count_);
}

TEST_F(UploadProgressTrackerTest, NoProgress) {
  upload_progress_tracker_.set_upload_progress(net::UploadProgress(500, 1000));

  // The first timer task calls ReportUploadProgress.
  EXPECT_EQ(0, report_count_);
  mock_task_runner_->FastForwardBy(
      UploadProgressTracker::GetUploadProgressIntervalForTesting());
  EXPECT_EQ(1, report_count_);
  EXPECT_EQ(500, reported_position_);
  EXPECT_EQ(1000, reported_total_size_);

  upload_progress_tracker_.OnAckReceived();

  // The second time doesn't call ReportUploadProgress since there's no
  // progress.
  EXPECT_EQ(1, report_count_);
  mock_task_runner_->FastForwardBy(
      UploadProgressTracker::GetUploadProgressIntervalForTesting());
  EXPECT_EQ(1, report_count_);
}

TEST_F(UploadProgressTrackerTest, Finished) {
  upload_progress_tracker_.set_upload_progress(net::UploadProgress(999, 1000));

  // The first timer task calls ReportUploadProgress.
  EXPECT_EQ(0, report_count_);
  mock_task_runner_->FastForwardBy(
      UploadProgressTracker::GetUploadProgressIntervalForTesting());
  EXPECT_EQ(1, report_count_);
  EXPECT_EQ(999, reported_position_);
  EXPECT_EQ(1000, reported_total_size_);

  upload_progress_tracker_.OnAckReceived();
  upload_progress_tracker_.set_upload_progress(net::UploadProgress(1000, 1000));

  // The second timer task calls ReportUploadProgress for reporting the
  // completion.
  EXPECT_EQ(1, report_count_);
  mock_task_runner_->FastForwardBy(
      UploadProgressTracker::GetUploadProgressIntervalForTesting());
  EXPECT_EQ(2, report_count_);
  EXPECT_EQ(1000, reported_position_);
  EXPECT_EQ(1000, reported_total_size_);
}

TEST_F(UploadProgressTrackerTest, Progress) {
  upload_progress_tracker_.set_upload_progress(net::UploadProgress(500, 1000));

  // The first timer task calls ReportUploadProgress.
  EXPECT_EQ(0, report_count_);
  mock_task_runner_->FastForwardBy(
      UploadProgressTracker::GetUploadProgressIntervalForTesting());
  EXPECT_EQ(1, report_count_);
  EXPECT_EQ(500, reported_position_);
  EXPECT_EQ(1000, reported_total_size_);

  upload_progress_tracker_.OnAckReceived();
  upload_progress_tracker_.set_upload_progress(net::UploadProgress(750, 1000));

  // The second timer task calls ReportUploadProgress since the progress is
  // big enough to report.
  EXPECT_EQ(1, report_count_);
  mock_task_runner_->FastForwardBy(
      UploadProgressTracker::GetUploadProgressIntervalForTesting());
  EXPECT_EQ(2, report_count_);
  EXPECT_EQ(750, reported_position_);
  EXPECT_EQ(1000, reported_total_size_);
}

TEST_F(UploadProgressTrackerTest, TimePassed) {
  upload_progress_tracker_.set_upload_progress(net::UploadProgress(500, 1000));

  // The first timer task calls ReportUploadProgress.
  EXPECT_EQ(0, report_count_);
  mock_task_runner_->FastForwardBy(
      UploadProgressTracker::GetUploadProgressIntervalForTesting());
  EXPECT_EQ(1, report_count_);
  EXPECT_EQ(500, reported_position_);
  EXPECT_EQ(1000, reported_total_size_);

  upload_progress_tracker_.OnAckReceived();
  upload_progress_tracker_.set_upload_progress(net::UploadProgress(501, 1000));

  // The second timer task doesn't call ReportUploadProgress since the progress
  // is too small to report it.
  EXPECT_EQ(1, report_count_);
  mock_task_runner_->FastForwardBy(
      UploadProgressTracker::GetUploadProgressIntervalForTesting());
  EXPECT_EQ(1, report_count_);

  upload_progress_tracker_.set_current_time(base::TimeTicks::Now() +
                                            base::TimeDelta::FromSeconds(5));

  // The third timer task calls ReportUploadProgress since it's been long time
  // from the last report.
  EXPECT_EQ(1, report_count_);
  mock_task_runner_->FastForwardBy(
      UploadProgressTracker::GetUploadProgressIntervalForTesting());
  EXPECT_EQ(2, report_count_);
  EXPECT_EQ(501, reported_position_);
  EXPECT_EQ(1000, reported_total_size_);
}

TEST_F(UploadProgressTrackerTest, Rewound) {
  upload_progress_tracker_.set_upload_progress(net::UploadProgress(500, 1000));

  // The first timer task calls ReportUploadProgress.
  EXPECT_EQ(0, report_count_);
  mock_task_runner_->FastForwardBy(
      UploadProgressTracker::GetUploadProgressIntervalForTesting());
  EXPECT_EQ(1, report_count_);
  EXPECT_EQ(500, reported_position_);
  EXPECT_EQ(1000, reported_total_size_);

  upload_progress_tracker_.OnAckReceived();
  upload_progress_tracker_.set_upload_progress(net::UploadProgress(250, 1000));

  // The second timer task doesn't call ReportUploadProgress since the progress
  // was rewound.
  EXPECT_EQ(1, report_count_);
  mock_task_runner_->FastForwardBy(
      UploadProgressTracker::GetUploadProgressIntervalForTesting());
  EXPECT_EQ(1, report_count_);

  upload_progress_tracker_.set_current_time(base::TimeTicks::Now() +
                                            base::TimeDelta::FromSeconds(5));

  // Even after a good amount of time passed, the rewound progress should not be
  // reported.
  EXPECT_EQ(1, report_count_);
  mock_task_runner_->FastForwardBy(
      UploadProgressTracker::GetUploadProgressIntervalForTesting());
  EXPECT_EQ(1, report_count_);
}

TEST_F(UploadProgressTrackerTest, Completed) {
  upload_progress_tracker_.set_upload_progress(net::UploadProgress(500, 1000));

  // The first timer task calls ReportUploadProgress.
  EXPECT_EQ(0, report_count_);
  mock_task_runner_->FastForwardBy(
      UploadProgressTracker::GetUploadProgressIntervalForTesting());
  EXPECT_EQ(1, report_count_);
  EXPECT_EQ(500, reported_position_);
  EXPECT_EQ(1000, reported_total_size_);

  upload_progress_tracker_.set_upload_progress(net::UploadProgress(1000, 1000));

  // OnUploadCompleted runs ReportUploadProgress even without Ack nor timer.
  upload_progress_tracker_.OnUploadCompleted();
  EXPECT_EQ(2, report_count_);
  EXPECT_EQ(1000, reported_position_);
  EXPECT_EQ(1000, reported_total_size_);

  mock_task_runner_->FastForwardBy(
      UploadProgressTracker::GetUploadProgressIntervalForTesting());
  EXPECT_EQ(2, report_count_);
  EXPECT_FALSE(mock_task_runner_->HasPendingTask());
}

}  // namespace network
