// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "device/wake_lock/public/interfaces/wake_lock.mojom.h"
#include "device/wake_lock/public/interfaces/wake_lock_context.mojom.h"
#include "device/wake_lock/public/interfaces/wake_lock_provider.mojom.h"
#include "device/wake_lock/wake_lock_provider.h"
#include "mojo/public/cpp/bindings/interface_ptr.h"
#include "services/device/device_service_test_base.h"
#include "services/device/public/interfaces/constants.mojom.h"

namespace device {

namespace {

class WakeLockTest : public DeviceServiceTestBase {
 public:
  WakeLockTest() = default;
  ~WakeLockTest() override = default;

 protected:
  void SetUp() override {
    DeviceServiceTestBase::SetUp();
    connector()->BindInterface(mojom::kServiceName, &wake_lock_provider_);

    WakeLockProvider::is_in_unittest_ = true;
    wake_lock_provider_->GetWakeLockWithoutContext(
        device::mojom::WakeLockType::PreventAppSuspension,
        device::mojom::WakeLockReason::ReasonOther, "WakeLockTest",
        mojo::MakeRequest(&wake_lock_));
  }

  void OnHasWakeLock(base::Closure quit_closure, bool has_wakelock) {
    has_wakelock_ = has_wakelock;
    quit_closure.Run();
  }

  bool HasWakeLock() {
    has_wakelock_ = false;

    base::RunLoop run_loop;
    wake_lock_->HasWakeLockForTests(base::Bind(&WakeLockTest::OnHasWakeLock,
                                               base::Unretained(this),
                                               run_loop.QuitClosure()));
    run_loop.Run();

    return has_wakelock_;
  }

  bool has_wakelock_;

  mojom::WakeLockProviderPtr wake_lock_provider_;
  mojom::WakeLockPtr wake_lock_;

  DISALLOW_COPY_AND_ASSIGN(WakeLockTest);
};

// Request a wake lock, then cancel.
TEST_F(WakeLockTest, RequestThenCancel) {
  EXPECT_FALSE(HasWakeLock());

  wake_lock_->RequestWakeLock();
  EXPECT_TRUE(HasWakeLock());

  wake_lock_->CancelWakeLock();
  EXPECT_FALSE(HasWakeLock());
}

// Cancel a wake lock first, which should have no effect.
TEST_F(WakeLockTest, CancelThenRequest) {
  EXPECT_FALSE(HasWakeLock());

  wake_lock_->CancelWakeLock();
  EXPECT_FALSE(HasWakeLock());

  wake_lock_->RequestWakeLock();
  EXPECT_TRUE(HasWakeLock());

  wake_lock_->CancelWakeLock();
  EXPECT_FALSE(HasWakeLock());
}

// Send multiple requests, which should be coalesced as one request.
TEST_F(WakeLockTest, MultipleRequests) {
  EXPECT_FALSE(HasWakeLock());

  wake_lock_->RequestWakeLock();
  wake_lock_->RequestWakeLock();
  wake_lock_->RequestWakeLock();
  EXPECT_TRUE(HasWakeLock());

  wake_lock_->CancelWakeLock();
  EXPECT_FALSE(HasWakeLock());
}

// WakeLockProvider connection broken doesn't affect WakeLock.
TEST_F(WakeLockTest, OnWakeLockProviderConnectionError) {
  EXPECT_FALSE(HasWakeLock());

  wake_lock_->RequestWakeLock();
  EXPECT_TRUE(HasWakeLock());

  wake_lock_provider_.reset();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(HasWakeLock());
  wake_lock_->CancelWakeLock();
  EXPECT_FALSE(HasWakeLock());
}

// One WakeLock instance can serve multiple clients at same time.
TEST_F(WakeLockTest, MultipleClients) {
  EXPECT_FALSE(HasWakeLock());

  mojom::WakeLockPtr wake_lock_1;
  mojom::WakeLockPtr wake_lock_2;
  mojom::WakeLockPtr wake_lock_3;
  wake_lock_->AddClient(mojo::MakeRequest(&wake_lock_1));
  wake_lock_->AddClient(mojo::MakeRequest(&wake_lock_2));
  wake_lock_->AddClient(mojo::MakeRequest(&wake_lock_3));

  EXPECT_FALSE(HasWakeLock());

  wake_lock_1->RequestWakeLock();
  wake_lock_2->RequestWakeLock();
  wake_lock_3->RequestWakeLock();
  EXPECT_TRUE(HasWakeLock());

  wake_lock_1->CancelWakeLock();
  wake_lock_2->CancelWakeLock();
  EXPECT_TRUE(HasWakeLock());

  wake_lock_3->CancelWakeLock();
  EXPECT_FALSE(HasWakeLock());
}

// WakeLock should update the wake lock status correctly when
// connection error happens.
TEST_F(WakeLockTest, OnWakeLockConnectionError) {
  EXPECT_FALSE(HasWakeLock());

  mojom::WakeLockPtr wake_lock_1;
  mojom::WakeLockPtr wake_lock_2;
  mojom::WakeLockPtr wake_lock_3;
  wake_lock_->AddClient(mojo::MakeRequest(&wake_lock_1));
  wake_lock_->AddClient(mojo::MakeRequest(&wake_lock_2));
  wake_lock_->AddClient(mojo::MakeRequest(&wake_lock_3));

  EXPECT_FALSE(HasWakeLock());

  wake_lock_1->RequestWakeLock();
  wake_lock_2->RequestWakeLock();
  wake_lock_3->RequestWakeLock();
  EXPECT_TRUE(HasWakeLock());

  wake_lock_1.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(HasWakeLock());

  wake_lock_2.reset();
  wake_lock_3.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(HasWakeLock());
}

// Test mixed operations.
TEST_F(WakeLockTest, MixedTest) {
  EXPECT_FALSE(HasWakeLock());

  mojom::WakeLockPtr wake_lock_1;
  mojom::WakeLockPtr wake_lock_2;
  mojom::WakeLockPtr wake_lock_3;
  wake_lock_->AddClient(mojo::MakeRequest(&wake_lock_1));
  wake_lock_->AddClient(mojo::MakeRequest(&wake_lock_2));
  wake_lock_->AddClient(mojo::MakeRequest(&wake_lock_3));

  EXPECT_FALSE(HasWakeLock());

  // Execute a series of calls that should result in |wake_lock_1| and
  // |wake_lock_3| having outstanding wake lock requests.
  wake_lock_1->RequestWakeLock();
  wake_lock_1->CancelWakeLock();
  wake_lock_2->RequestWakeLock();
  wake_lock_1->RequestWakeLock();
  wake_lock_1->RequestWakeLock();
  wake_lock_3->CancelWakeLock();
  wake_lock_3->CancelWakeLock();
  wake_lock_2->CancelWakeLock();
  wake_lock_3->RequestWakeLock();
  wake_lock_2.reset();
  EXPECT_TRUE(HasWakeLock());

  wake_lock_1.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(HasWakeLock());

  wake_lock_3->CancelWakeLock();
  EXPECT_FALSE(HasWakeLock());
}

}  // namespace

}  // namespace device
