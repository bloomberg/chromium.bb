// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/browser/fileapi/quota/quota_reservation_manager.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/browser/fileapi/quota/open_file_handle.h"
#include "webkit/browser/fileapi/quota/quota_reservation.h"

namespace fileapi {

namespace {

const char kOrigin[] = "http://example.com";
const FileSystemType kType = kFileSystemTypeTemporary;
const int64 kInitialFileSize = 30;

typedef QuotaReservationManager::ReserveQuotaCallback ReserveQuotaCallback;

class FakeBackend : public QuotaReservationManager::QuotaBackend {
 public:
  FakeBackend()
      : on_memory_usage_(0),
        on_disk_usage_(0) {}
  virtual ~FakeBackend() {}

  virtual void ReserveQuota(const GURL& origin,
                            FileSystemType type,
                            int64 delta,
                            const ReserveQuotaCallback& callback) OVERRIDE {
    EXPECT_EQ(GURL(kOrigin), origin);
    EXPECT_EQ(kType, type);
    on_memory_usage_ += delta;
    base::MessageLoopProxy::current()->PostTask(
        FROM_HERE,
        base::Bind(base::IgnoreResult(callback), base::PLATFORM_FILE_OK));
  }

  virtual void ReleaseReservedQuota(const GURL& origin,
                                    FileSystemType type,
                                    int64 size) OVERRIDE {
    EXPECT_LE(0, size);
    EXPECT_EQ(GURL(kOrigin), origin);
    EXPECT_EQ(kType, type);
    on_memory_usage_ -= size;
  }

  virtual void CommitQuotaUsage(const GURL& origin,
                                FileSystemType type,
                                int64 delta) OVERRIDE {
    EXPECT_EQ(GURL(kOrigin), origin);
    EXPECT_EQ(kType, type);
    on_disk_usage_ += delta;
  }

  virtual void IncrementDirtyCount(const GURL& origin,
                                   FileSystemType type) OVERRIDE {}
  virtual void DecrementDirtyCount(const GURL& origin,
                                   FileSystemType type) OVERRIDE {}

  int64 on_memory_usage() { return on_memory_usage_; }
  int64 on_disk_usage() { return on_disk_usage_; }

 private:
  int64 on_memory_usage_;
  int64 on_disk_usage_;

  DISALLOW_COPY_AND_ASSIGN(FakeBackend);
};

void ExpectSuccess(bool* done, base::PlatformFileError error) {
  EXPECT_FALSE(*done);
  *done = true;
  EXPECT_EQ(base::PLATFORM_FILE_OK, error);
}

void RefreshQuota(QuotaReservation* reservation, int64 size) {
  DCHECK(reservation);

  bool done = false;
  reservation->RefreshReservation(size, base::Bind(&ExpectSuccess, &done));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(done);
}

}  // namespace

class QuotaReservationManagerTest : public testing::Test {
 public:
  QuotaReservationManagerTest() {}
  virtual ~QuotaReservationManagerTest() {}

  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(work_dir_.CreateUniqueTempDir());
    file_path_ = work_dir_.path().Append(FILE_PATH_LITERAL("hoge"));
    SetFileSize(kInitialFileSize);

    scoped_ptr<QuotaReservationManager::QuotaBackend> backend(new FakeBackend);
    reservation_manager_.reset(new QuotaReservationManager(backend.Pass()));
  }

  virtual void TearDown() OVERRIDE {
    reservation_manager_.reset();
  }

  int64 GetFileSize() {
    int64 size = 0;
    base::GetFileSize(file_path_, &size);
    return size;
  }

  void SetFileSize(int64 size) {
    bool created = false;
    base::PlatformFileError error = base::PLATFORM_FILE_ERROR_FAILED;
    base::PlatformFile file = CreatePlatformFile(
        file_path_,
        base::PLATFORM_FILE_OPEN_ALWAYS | base::PLATFORM_FILE_WRITE,
        &created, &error);
    ASSERT_EQ(base::PLATFORM_FILE_OK, error);
    ASSERT_TRUE(base::TruncatePlatformFile(file, size));
    ASSERT_TRUE(base::ClosePlatformFile(file));
  }

  void ExtendFileTo(int64 size) {
    if (GetFileSize() < size)
      SetFileSize(size);
  }

  FakeBackend* fake_backend() {
    return static_cast<FakeBackend*>(reservation_manager_->backend_.get());
  }

  QuotaReservationManager* reservation_manager() {
    return reservation_manager_.get();
  }

  const base::FilePath& file_path() const {
    return file_path_;
  }

 private:
  base::MessageLoop message_loop_;
  base::ScopedTempDir work_dir_;
  base::FilePath file_path_;
  scoped_ptr<QuotaReservationManager> reservation_manager_;

  DISALLOW_COPY_AND_ASSIGN(QuotaReservationManagerTest);
};

TEST_F(QuotaReservationManagerTest, BasicTest) {
  GURL origin(kOrigin);
  FileSystemType type = kType;

  // Create Reservation channel for the origin and type.
  // Reservation holds remaining quota reservation and provides a method to
  // refresh it.
  scoped_refptr<QuotaReservation> reservation =
      reservation_manager()->CreateReservation(origin, type);
  EXPECT_EQ(0, reservation->remaining_quota());

  RefreshQuota(reservation, 100);
  EXPECT_EQ(100, reservation->remaining_quota());

  {
    // For each open file for write, the client should create OpenFileHandle
    // object.
    // It's OK to create multiple OpenFileHandle for single file.
    scoped_ptr<OpenFileHandle> open_file =
        reservation->GetOpenFileHandle(file_path());

    // Before reserved quota ran out, the client can perform any number of
    // operation for the file.
    // The client should calculate how much quota is consumed by itself.
    int64 remaining_quota = reservation->remaining_quota();
    int64 base_file_size = open_file->base_file_size();
    int64 max_written_offset = base_file_size;
    ExtendFileTo(90);
    max_written_offset = 90;
    remaining_quota -= max_written_offset - base_file_size;

    // When the reserved quota ran out, the client can request quota refresh
    // through Reservation.  Before requesting another portion of quota, the
    // client should report maximum written offset for each modified files.
    open_file->UpdateMaxWrittenOffset(max_written_offset);
    EXPECT_EQ(remaining_quota, reservation->remaining_quota());

    RefreshQuota(reservation, 100);
    EXPECT_EQ(100, reservation->remaining_quota());
  }

  EXPECT_EQ(90, GetFileSize());
  EXPECT_EQ(100, fake_backend()->on_memory_usage());
  EXPECT_EQ(90 - kInitialFileSize, fake_backend()->on_disk_usage());

  reservation = NULL;

  EXPECT_EQ(90, GetFileSize());
  EXPECT_EQ(0, fake_backend()->on_memory_usage());
  EXPECT_EQ(90 - kInitialFileSize, fake_backend()->on_disk_usage());
}

TEST_F(QuotaReservationManagerTest, MultipleWriter) {
  GURL origin(kOrigin);
  FileSystemType type = kType;

  scoped_refptr<QuotaReservation> reservation =
      reservation_manager()->CreateReservation(origin, type);
  EXPECT_EQ(0, reservation->remaining_quota());

  RefreshQuota(reservation, 100);
  EXPECT_EQ(100, reservation->remaining_quota());

  {
    scoped_ptr<OpenFileHandle> open_file1 =
        reservation->GetOpenFileHandle(file_path());
    scoped_ptr<OpenFileHandle> open_file2 =
        reservation->GetOpenFileHandle(file_path());

    int64 remaining_quota = reservation->remaining_quota();

    int64 base_file_size_for_file1 = open_file1->base_file_size();
    int64 max_written_offset_for_file1 = base_file_size_for_file1;

    int64 base_file_size_for_file2 = open_file2->base_file_size();
    int64 max_written_offset_for_file2 = base_file_size_for_file2;

    // Each writer should maintain max_written_offset and base_file_size
    // independently even if there are multiple writers for the same file.
    max_written_offset_for_file1 = 50;
    ExtendFileTo(max_written_offset_for_file1);
    remaining_quota -= max_written_offset_for_file1 - base_file_size_for_file1;
    base_file_size_for_file1 = max_written_offset_for_file1;

    max_written_offset_for_file2 = 90;
    ExtendFileTo(max_written_offset_for_file2);
    remaining_quota -= max_written_offset_for_file2 - base_file_size_for_file2;
    base_file_size_for_file2 = max_written_offset_for_file2;

    // Before requesting quota refresh, each writer should report their
    // maximum_written_offset.  UpdateMaxWrittenOffset returns updated
    // base_file_size that the writer should calculate quota consumption based
    // on that.
    base_file_size_for_file1 =
        open_file1->UpdateMaxWrittenOffset(max_written_offset_for_file1);
    max_written_offset_for_file1 = base_file_size_for_file1;
    EXPECT_EQ(100 - (50 - kInitialFileSize), reservation->remaining_quota());

    base_file_size_for_file2 =
        open_file2->UpdateMaxWrittenOffset(max_written_offset_for_file2);
    max_written_offset_for_file2 = base_file_size_for_file2;
    EXPECT_EQ(100 - (50 - kInitialFileSize) - (90 - 50),
              reservation->remaining_quota());

    RefreshQuota(reservation, 100);
    EXPECT_EQ(100, reservation->remaining_quota());
  }

  EXPECT_EQ(90, GetFileSize());
  EXPECT_EQ(100, fake_backend()->on_memory_usage());
  EXPECT_EQ(90 - kInitialFileSize, fake_backend()->on_disk_usage());

  reservation = NULL;

  EXPECT_EQ(90, GetFileSize());
  EXPECT_EQ(0, fake_backend()->on_memory_usage());
  EXPECT_EQ(90 - kInitialFileSize, fake_backend()->on_disk_usage());
}

TEST_F(QuotaReservationManagerTest, MultipleClient) {
  GURL origin(kOrigin);
  FileSystemType type = kType;

  scoped_refptr<QuotaReservation> reservation1 =
      reservation_manager()->CreateReservation(origin, type);
  EXPECT_EQ(0, reservation1->remaining_quota());
  RefreshQuota(reservation1, 100);
  EXPECT_EQ(100, reservation1->remaining_quota());

  scoped_refptr<QuotaReservation> reservation2 =
      reservation_manager()->CreateReservation(origin, type);
  EXPECT_EQ(0, reservation2->remaining_quota());
  RefreshQuota(reservation2, 500);
  EXPECT_EQ(500, reservation2->remaining_quota());

  // Attach a file to both of two reservations.
  scoped_ptr<OpenFileHandle> open_file1 =
      reservation1->GetOpenFileHandle(file_path());
  scoped_ptr<OpenFileHandle> open_file2 =
      reservation2->GetOpenFileHandle(file_path());

  // Each client should manage reserved quota and its consumption separately.
  int64 remaining_quota1 = reservation1->remaining_quota();
  int64 base_file_size1 = open_file1->base_file_size();
  int64 max_written_offset1 = base_file_size1;

  int64 remaining_quota2 = reservation2->remaining_quota();
  int64 base_file_size2 = open_file2->base_file_size();
  int64 max_written_offset2 = base_file_size2;

  max_written_offset1 = 50;
  remaining_quota1 -= max_written_offset1 - base_file_size1;
  base_file_size1 = max_written_offset1;
  ExtendFileTo(max_written_offset1);

  max_written_offset2 = 400;
  remaining_quota2 -= max_written_offset2 - base_file_size2;
  base_file_size2 = max_written_offset2;
  ExtendFileTo(max_written_offset2);

  // For multiple Reservation case, RefreshQuota needs usage report only from
  // associated OpenFile's.
  base_file_size1 =
      open_file1->UpdateMaxWrittenOffset(max_written_offset1);
  max_written_offset1 = base_file_size1;
  EXPECT_EQ(100 - (50 - kInitialFileSize), reservation1->remaining_quota());

  RefreshQuota(reservation1, 200);
  EXPECT_EQ(200, reservation1->remaining_quota());

  base_file_size2 =
      open_file2->UpdateMaxWrittenOffset(max_written_offset2);
  max_written_offset2 = base_file_size2;
  EXPECT_EQ(500 - (400 - 50), reservation2->remaining_quota());

  RefreshQuota(reservation2, 150);
  EXPECT_EQ(150, reservation2->remaining_quota());

  open_file1.reset();
  open_file2.reset();

  EXPECT_EQ(400, GetFileSize());
  EXPECT_EQ(200 + 150, fake_backend()->on_memory_usage());
  EXPECT_EQ(400 - kInitialFileSize, fake_backend()->on_disk_usage());

  reservation1 = NULL;

  EXPECT_EQ(400, GetFileSize());
  EXPECT_EQ(150, fake_backend()->on_memory_usage());
  EXPECT_EQ(400 - kInitialFileSize, fake_backend()->on_disk_usage());

  reservation2 = NULL;

  EXPECT_EQ(400, GetFileSize());
  EXPECT_EQ(0, fake_backend()->on_memory_usage());
  EXPECT_EQ(400 - kInitialFileSize, fake_backend()->on_disk_usage());
}

// TODO(tzik): Add Truncate test.
// TODO(tzik): Add PluginCrash test and DropReservationManager test.

}  // namespace fileapi
