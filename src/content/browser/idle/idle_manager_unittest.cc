// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/idle/idle_manager.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/time/time.h"
#include "content/browser/permissions/permission_controller_impl.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/permission_type.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/test/mock_permission_manager.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_service_manager_context.h"
#include "content/test/test_render_frame_host.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "services/service_manager/public/cpp/bind_source_info.h"
#include "services/service_manager/public/cpp/connector.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/idle/idle_manager.mojom.h"

using blink::mojom::IdleManagerPtr;
using blink::mojom::IdleMonitorPtr;
using ::testing::_;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::StrictMock;

namespace content {

namespace {

constexpr base::TimeDelta kThreshold = base::TimeDelta::FromSeconds(60);

class MockIdleMonitor : public blink::mojom::IdleMonitor {
 public:
  MOCK_METHOD1(Update, void(blink::mojom::IdleStatePtr));
};

class MockIdleTimeProvider : public IdleManager::IdleTimeProvider {
 public:
  MockIdleTimeProvider() = default;
  ~MockIdleTimeProvider() override = default;

  MOCK_METHOD1(CalculateIdleState, ui::IdleState(base::TimeDelta));
  MOCK_METHOD0(CalculateIdleTime, base::TimeDelta());
  MOCK_METHOD0(CheckIdleStateIsLocked, bool());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockIdleTimeProvider);
};

class IdleManagerTest : public RenderViewHostImplTestHarness {
 protected:
  IdleManagerTest() {}

  ~IdleManagerTest() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(IdleManagerTest);
};

}  // namespace

TEST_F(IdleManagerTest, AddMonitor) {
  auto impl = std::make_unique<IdleManager>();
  auto* mock = new NiceMock<MockIdleTimeProvider>();
  impl->SetIdleTimeProviderForTest(base::WrapUnique(mock));
  blink::mojom::IdleManagerPtr service_ptr;
  GURL url("http://google.com");
  impl->CreateService(mojo::MakeRequest(&service_ptr),
                      url::Origin::Create(url));

  blink::mojom::IdleMonitorPtr monitor_ptr;
  blink::mojom::IdleMonitorRequest monitor_request =
      mojo::MakeRequest(&monitor_ptr);

  base::RunLoop loop;

  service_ptr.set_connection_error_handler(base::BindLambdaForTesting([&]() {
    ADD_FAILURE() << "Unexpected connection error";

    loop.Quit();
  }));

  // Initial state of the system.
  EXPECT_CALL(*mock, CalculateIdleTime())
      .WillRepeatedly(testing::Return(base::TimeDelta::FromSeconds(0)));
  EXPECT_CALL(*mock, CheckIdleStateIsLocked())
      .WillRepeatedly(testing::Return(false));

  service_ptr->AddMonitor(
      kThreshold, std::move(monitor_ptr),
      base::BindOnce(
          [](base::OnceClosure callback, blink::mojom::IdleStatePtr state) {
            // The initial state of the status of the user is to be active.
            EXPECT_EQ(blink::mojom::UserIdleState::kActive, state->user);
            EXPECT_EQ(blink::mojom::ScreenIdleState::kUnlocked, state->screen);
            std::move(callback).Run();
          },
          loop.QuitClosure()));

  loop.Run();
}

TEST_F(IdleManagerTest, Idle) {
  blink::mojom::IdleManagerPtr service_ptr;

  auto impl = std::make_unique<IdleManager>();
  auto* mock = new NiceMock<MockIdleTimeProvider>();
  impl->SetIdleTimeProviderForTest(base::WrapUnique(mock));

  GURL url("http://google.com");
  impl->CreateService(mojo::MakeRequest(&service_ptr),
                      url::Origin::Create(url));

  blink::mojom::IdleMonitorPtr monitor_ptr;
  auto monitor_request = mojo::MakeRequest(&monitor_ptr);
  MockIdleMonitor monitor;
  mojo::Binding<blink::mojom::IdleMonitor> monitor_binding(
      &monitor, std::move(monitor_request));

  {
    base::RunLoop loop;
    // Initial state of the system.
    EXPECT_CALL(*mock, CalculateIdleTime())
        .WillRepeatedly(testing::Return(base::TimeDelta::FromSeconds(0)));

    service_ptr->AddMonitor(
        kThreshold, std::move(monitor_ptr),
        base::BindLambdaForTesting([&](blink::mojom::IdleStatePtr state) {
          EXPECT_EQ(blink::mojom::UserIdleState::kActive, state->user);
          loop.Quit();
        }));

    loop.Run();
  }

  {
    base::RunLoop loop;
    // Simulates a user going idle.
    EXPECT_CALL(*mock, CalculateIdleTime())
        .WillRepeatedly(testing::Return(base::TimeDelta::FromSeconds(60)));

    // Expects Update to be notified about the change to idle.
    EXPECT_CALL(monitor, Update(_))
        .WillOnce(Invoke([&](blink::mojom::IdleStatePtr state) {
          EXPECT_EQ(blink::mojom::UserIdleState::kIdle, state->user);
          loop.Quit();
        }));
    loop.Run();
  }

  {
    base::RunLoop loop;
    // Simulates a user going active, calling a callback under the threshold.
    EXPECT_CALL(*mock, CalculateIdleTime())
        .WillRepeatedly(testing::Return(base::TimeDelta::FromSeconds(0)));

    // Expects Update to be notified about the change to active.
    // auto quit = loop.QuitClosure();
    EXPECT_CALL(monitor, Update(_))
        .WillOnce(Invoke([&](blink::mojom::IdleStatePtr state) {
          EXPECT_EQ(blink::mojom::UserIdleState::kActive, state->user);
          // Ends the test.
          loop.Quit();
        }));
    loop.Run();
  }
}

TEST_F(IdleManagerTest, UnlockingScreen) {
  blink::mojom::IdleManagerPtr service_ptr;

  auto impl = std::make_unique<IdleManager>();
  auto* mock = new NiceMock<MockIdleTimeProvider>();
  impl->SetIdleTimeProviderForTest(base::WrapUnique(mock));

  GURL url("http://google.com");
  impl->CreateService(mojo::MakeRequest(&service_ptr),
                      url::Origin::Create(url));

  blink::mojom::IdleMonitorPtr monitor_ptr;
  auto monitor_request = mojo::MakeRequest(&monitor_ptr);
  MockIdleMonitor monitor;
  mojo::Binding<blink::mojom::IdleMonitor> monitor_binding(
      &monitor, std::move(monitor_request));

  {
    base::RunLoop loop;

    // Initial state of the system.
    EXPECT_CALL(*mock, CheckIdleStateIsLocked())
        .WillRepeatedly(testing::Return(true));

    service_ptr->AddMonitor(
        kThreshold, std::move(monitor_ptr),
        base::BindLambdaForTesting([&](blink::mojom::IdleStatePtr state) {
          EXPECT_EQ(blink::mojom::ScreenIdleState::kLocked, state->screen);
          loop.Quit();
        }));

    loop.Run();
  }

  {
    base::RunLoop loop;

    // Simulates a user unlocking the screen.
    EXPECT_CALL(*mock, CheckIdleStateIsLocked())
        .WillRepeatedly(testing::Return(false));

    // Expects Update to be notified about the change to unlocked.
    EXPECT_CALL(monitor, Update(_))
        .WillOnce(Invoke([&](blink::mojom::IdleStatePtr state) {
          EXPECT_EQ(blink::mojom::ScreenIdleState::kUnlocked, state->screen);
          loop.Quit();
        }));

    loop.Run();
  }
}

TEST_F(IdleManagerTest, LockingScreen) {
  blink::mojom::IdleManagerPtr service_ptr;

  auto impl = std::make_unique<IdleManager>();
  auto* mock = new NiceMock<MockIdleTimeProvider>();
  impl->SetIdleTimeProviderForTest(base::WrapUnique(mock));

  GURL url("http://google.com");
  impl->CreateService(mojo::MakeRequest(&service_ptr),
                      url::Origin::Create(url));

  blink::mojom::IdleMonitorPtr monitor_ptr;
  auto monitor_request = mojo::MakeRequest(&monitor_ptr);
  MockIdleMonitor monitor;
  mojo::Binding<blink::mojom::IdleMonitor> monitor_binding(
      &monitor, std::move(monitor_request));

  {
    base::RunLoop loop;

    // Initial state of the system.
    EXPECT_CALL(*mock, CheckIdleStateIsLocked())
        .WillRepeatedly(testing::Return(false));

    service_ptr->AddMonitor(
        kThreshold, std::move(monitor_ptr),
        base::BindLambdaForTesting([&](blink::mojom::IdleStatePtr state) {
          EXPECT_EQ(blink::mojom::ScreenIdleState::kUnlocked, state->screen);
          loop.Quit();
        }));

    loop.Run();
  }

  {
    base::RunLoop loop;

    // Simulates a user locking the screen.
    EXPECT_CALL(*mock, CheckIdleStateIsLocked())
        .WillRepeatedly(testing::Return(true));

    // Expects Update to be notified about the change to unlocked.
    EXPECT_CALL(monitor, Update(_))
        .WillOnce(Invoke([&](blink::mojom::IdleStatePtr state) {
          EXPECT_EQ(blink::mojom::ScreenIdleState::kLocked, state->screen);
          loop.Quit();
        }));

    loop.Run();
  }
}

TEST_F(IdleManagerTest, LockingScreenThenIdle) {
  blink::mojom::IdleManagerPtr service_ptr;

  auto impl = std::make_unique<IdleManager>();
  auto* mock = new NiceMock<MockIdleTimeProvider>();
  impl->SetIdleTimeProviderForTest(base::WrapUnique(mock));

  GURL url("http://google.com");
  impl->CreateService(mojo::MakeRequest(&service_ptr),
                      url::Origin::Create(url));

  blink::mojom::IdleMonitorPtr monitor_ptr;
  auto monitor_request = mojo::MakeRequest(&monitor_ptr);
  MockIdleMonitor monitor;
  mojo::Binding<blink::mojom::IdleMonitor> monitor_binding(
      &monitor, std::move(monitor_request));

  {
    base::RunLoop loop;

    // Initial state of the system.
    EXPECT_CALL(*mock, CheckIdleStateIsLocked())
        .WillRepeatedly(testing::Return(false));

    service_ptr->AddMonitor(
        kThreshold, std::move(monitor_ptr),
        base::BindLambdaForTesting([&](blink::mojom::IdleStatePtr state) {
          EXPECT_EQ(blink::mojom::UserIdleState::kActive, state->user);
          EXPECT_EQ(blink::mojom::ScreenIdleState::kUnlocked, state->screen);
          loop.Quit();
        }));

    loop.Run();
  }

  {
    base::RunLoop loop;

    // Simulates a user locking screen.
    EXPECT_CALL(*mock, CheckIdleStateIsLocked())
        .WillRepeatedly(testing::Return(true));

    // Expects Update to be notified about the change to locked.
    EXPECT_CALL(monitor, Update(_))
        .WillOnce(Invoke([&](blink::mojom::IdleStatePtr state) {
          EXPECT_EQ(blink::mojom::ScreenIdleState::kLocked, state->screen);
          EXPECT_EQ(blink::mojom::UserIdleState::kActive, state->user);
          loop.Quit();
        }));

    loop.Run();
  }

  {
    base::RunLoop loop;

    // Simulates a user going idle, whilte the screen is still locked.
    EXPECT_CALL(*mock, CalculateIdleTime())
        .WillRepeatedly(testing::Return(base::TimeDelta::FromSeconds(60)));
    EXPECT_CALL(*mock, CheckIdleStateIsLocked())
        .WillRepeatedly(testing::Return(true));

    // Expects Update to be notified about the change to active.
    EXPECT_CALL(monitor, Update(_))
        .WillOnce(Invoke([&](blink::mojom::IdleStatePtr state) {
          EXPECT_EQ(blink::mojom::UserIdleState::kIdle, state->user);
          EXPECT_EQ(blink::mojom::ScreenIdleState::kLocked, state->screen);
          // Ends the test.
          loop.Quit();
        }));

    loop.Run();
  }
}

TEST_F(IdleManagerTest, LockingScreenAfterIdle) {
  blink::mojom::IdleManagerPtr service_ptr;

  auto impl = std::make_unique<IdleManager>();
  auto* mock = new NiceMock<MockIdleTimeProvider>();
  impl->SetIdleTimeProviderForTest(base::WrapUnique(mock));

  GURL url("http://google.com");
  impl->CreateService(mojo::MakeRequest(&service_ptr),
                      url::Origin::Create(url));

  blink::mojom::IdleMonitorPtr monitor_ptr;
  auto monitor_request = mojo::MakeRequest(&monitor_ptr);
  MockIdleMonitor monitor;
  mojo::Binding<blink::mojom::IdleMonitor> monitor_binding(
      &monitor, std::move(monitor_request));

  {
    base::RunLoop loop;

    // Simulates a user going idle, but with the screen still unlocked.
    EXPECT_CALL(*mock, CalculateIdleTime())
        .WillRepeatedly(testing::Return(base::TimeDelta::FromSeconds(0)));
    EXPECT_CALL(*mock, CheckIdleStateIsLocked())
        .WillRepeatedly(testing::Return(false));

    service_ptr->AddMonitor(
        kThreshold, std::move(monitor_ptr),
        base::BindLambdaForTesting([&](blink::mojom::IdleStatePtr state) {
          EXPECT_EQ(blink::mojom::UserIdleState::kActive, state->user);
          EXPECT_EQ(blink::mojom::ScreenIdleState::kUnlocked, state->screen);
          loop.Quit();
        }));

    loop.Run();
  }

  {
    base::RunLoop loop;
    // Simulates a user going idle, but with the screen still unlocked.
    EXPECT_CALL(*mock, CalculateIdleTime())
        .WillRepeatedly(testing::Return(base::TimeDelta::FromSeconds(60)));
    EXPECT_CALL(*mock, CheckIdleStateIsLocked())
        .WillRepeatedly(testing::Return(false));

    // Expects Update to be notified about the change to idle.
    EXPECT_CALL(monitor, Update(_))
        .WillOnce(Invoke([&](blink::mojom::IdleStatePtr state) {
          EXPECT_EQ(blink::mojom::UserIdleState::kIdle, state->user);
          EXPECT_EQ(blink::mojom::ScreenIdleState::kUnlocked, state->screen);
          loop.Quit();
        }));

    loop.Run();
  }

  {
    base::RunLoop loop;
    // Simulates the screeng getting locked by the system after the user goes
    // idle (e.g. screensaver kicks in first, throwing idleness, then getting
    // locked).
    EXPECT_CALL(*mock, CalculateIdleTime())
        .WillRepeatedly(testing::Return(base::TimeDelta::FromSeconds(60)));
    EXPECT_CALL(*mock, CheckIdleStateIsLocked())
        .WillRepeatedly(testing::Return(true));

    // Expects Update to be notified about the change to locked.
    EXPECT_CALL(monitor, Update(_))
        .WillOnce(Invoke([&](blink::mojom::IdleStatePtr state) {
          EXPECT_EQ(blink::mojom::ScreenIdleState::kLocked, state->screen);
          EXPECT_EQ(blink::mojom::UserIdleState::kIdle, state->user);
          // Ends the test.
          loop.Quit();
        }));
    loop.Run();
  }
}

TEST_F(IdleManagerTest, RemoveMonitorStopsPolling) {
  // Simulates the renderer disconnecting (e.g. on page reload) and verifies
  // that the polling stops for the idle detection.

  auto impl = std::make_unique<IdleManager>();
  auto* mock = new NiceMock<MockIdleTimeProvider>();
  impl->SetIdleTimeProviderForTest(base::WrapUnique(mock));

  blink::mojom::IdleManagerPtr service_ptr;
  GURL url("http://google.com");
  impl->CreateService(mojo::MakeRequest(&service_ptr),
                      url::Origin::Create(url));

  blink::mojom::IdleMonitorPtr monitor_ptr;
  blink::mojom::IdleMonitorRequest monitor_request =
      mojo::MakeRequest(&monitor_ptr);
  MockIdleMonitor monitor_impl;
  mojo::Binding<blink::mojom::IdleMonitor> monitor_binding(
      &monitor_impl, std::move(monitor_request));

  {
    base::RunLoop loop;

    service_ptr->AddMonitor(
        kThreshold, std::move(monitor_ptr),
        base::BindLambdaForTesting(
            [&](blink::mojom::IdleStatePtr state) { loop.Quit(); }));

    loop.Run();
  }

  EXPECT_TRUE(impl->IsPollingForTest());

  {
    base::RunLoop loop;

    // Simulates the renderer disconnecting.
    monitor_binding.Close();

    // Wait for the IdleManager to observe the pipe close.
    loop.RunUntilIdle();
  }

  EXPECT_FALSE(impl->IsPollingForTest());
}

TEST_F(IdleManagerTest, Threshold) {
  auto impl = std::make_unique<IdleManager>();
  auto* mock = new NiceMock<MockIdleTimeProvider>();
  impl->SetIdleTimeProviderForTest(base::WrapUnique(mock));
  blink::mojom::IdleManagerPtr service_ptr;
  GURL url("http://google.com");
  impl->CreateService(mojo::MakeRequest(&service_ptr),
                      url::Origin::Create(url));

  blink::mojom::IdleMonitorPtr monitor_ptr;
  blink::mojom::IdleMonitorRequest monitor_request =
      mojo::MakeRequest(&monitor_ptr);

  base::RunLoop loop;

  // Initial state of the system.
  EXPECT_CALL(*mock, CalculateIdleTime())
      .WillRepeatedly(testing::Return(base::TimeDelta::FromSeconds(91)));
  EXPECT_CALL(*mock, CheckIdleStateIsLocked())
      .WillRepeatedly(testing::Return(false));

  service_ptr->AddMonitor(
      base::TimeDelta::FromSeconds(90), std::move(monitor_ptr),
      base::BindLambdaForTesting([&](blink::mojom::IdleStatePtr state) {
        EXPECT_EQ(blink::mojom::UserIdleState::kIdle, state->user);
        loop.Quit();
      }));

  loop.Run();
}

TEST_F(IdleManagerTest, BadThreshold) {
  mojo::test::BadMessageObserver bad_message_observer;
  auto impl = std::make_unique<IdleManager>();
  auto* mock = new NiceMock<MockIdleTimeProvider>();
  impl->SetIdleTimeProviderForTest(base::WrapUnique(mock));
  blink::mojom::IdleManagerPtr service_ptr;
  GURL url("http://google.com");
  impl->CreateService(mojo::MakeRequest(&service_ptr),
                      url::Origin::Create(url));

  blink::mojom::IdleMonitorPtr monitor_ptr;
  blink::mojom::IdleMonitorRequest monitor_request =
      mojo::MakeRequest(&monitor_ptr);

  // Should not start initial state of the system.
  EXPECT_CALL(*mock, CalculateIdleTime()).Times(0);
  EXPECT_CALL(*mock, CheckIdleStateIsLocked()).Times(0);

  service_ptr->AddMonitor(base::TimeDelta::FromSeconds(50),
                          std::move(monitor_ptr), base::NullCallback());
  EXPECT_EQ("Minimum threshold is 60 seconds.",
            bad_message_observer.WaitForBadMessage());
}

}  // namespace content
