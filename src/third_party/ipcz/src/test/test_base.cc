// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/test_base.h"

#include "api.h"
#include "ipcz/ipcz.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/synchronization/notification.h"

namespace ipcz::test::internal {

namespace {

void CreateNodeChecked(const IpczAPI& ipcz,
                       const IpczDriver& driver,
                       IpczCreateNodeFlags flags,
                       IpczHandle& handle) {
  const IpczResult result = ipcz.CreateNode(&driver, IPCZ_INVALID_DRIVER_HANDLE,
                                            flags, nullptr, &handle);
  ASSERT_EQ(IPCZ_RESULT_OK, result);
}

void OpenPortalsChecked(const IpczAPI& ipcz,
                        IpczHandle node,
                        IpczHandle& first,
                        IpczHandle& second) {
  const IpczResult result =
      ipcz.OpenPortals(node, IPCZ_NO_FLAGS, nullptr, &first, &second);
  ASSERT_EQ(IPCZ_RESULT_OK, result);
}

}  // namespace

TestBase::TestBase() {
  IpczGetAPI(&ipcz_);
}

TestBase::~TestBase() = default;

void TestBase::Close(IpczHandle handle) {
  ASSERT_EQ(IPCZ_RESULT_OK, ipcz().Close(handle, IPCZ_NO_FLAGS, nullptr));
}

void TestBase::CloseAll(absl::Span<const IpczHandle> handles) {
  for (IpczHandle handle : handles) {
    Close(handle);
  }
}

IpczHandle TestBase::CreateNode(const IpczDriver& driver,
                                IpczCreateNodeFlags flags) {
  IpczHandle node;
  CreateNodeChecked(ipcz(), driver, flags, node);
  return node;
}

std::pair<IpczHandle, IpczHandle> TestBase::OpenPortals(IpczHandle node) {
  IpczHandle a, b;
  OpenPortalsChecked(ipcz(), node, a, b);
  return {a, b};
}

IpczResult TestBase::Put(IpczHandle portal,
                         std::string_view message,
                         absl::Span<IpczHandle> handles) {
  return ipcz().Put(portal, message.data(), message.size(), handles.data(),
                    handles.size(), IPCZ_NO_FLAGS, nullptr);
}

IpczResult TestBase::Get(IpczHandle portal,
                         std::string* message,
                         absl::Span<IpczHandle> handles) {
  if (message) {
    message->clear();
  }

  size_t num_bytes = 0;
  IpczHandle* handle_storage = handles.empty() ? nullptr : handles.data();
  size_t num_handles = handles.size();
  IpczResult result = ipcz().Get(portal, IPCZ_NO_FLAGS, nullptr, nullptr,
                                 &num_bytes, handle_storage, &num_handles);
  if (result != IPCZ_RESULT_RESOURCE_EXHAUSTED) {
    return result;
  }

  void* data_storage = nullptr;
  if (message) {
    message->resize(num_bytes);
    data_storage = message->data();
  }

  return ipcz().Get(portal, IPCZ_NO_FLAGS, nullptr, data_storage, &num_bytes,
                    handle_storage, &num_handles);
}

IpczResult TestBase::Trap(IpczHandle portal,
                          const IpczTrapConditions& conditions,
                          TrapEventHandler fn,
                          IpczTrapConditionFlags* flags,
                          IpczPortalStatus* status) {
  auto handler = std::make_unique<TrapEventHandler>(std::move(fn));
  auto context = reinterpret_cast<uintptr_t>(handler.get());
  const IpczResult result =
      ipcz().Trap(portal, &conditions, &HandleEvent, context, IPCZ_NO_FLAGS,
                  nullptr, flags, status);
  if (result == IPCZ_RESULT_OK) {
    std::ignore = handler.release();
  }
  return result;
}

IpczResult TestBase::WaitForConditions(IpczHandle portal,
                                       const IpczTrapConditions& conditions) {
  absl::Notification notification;
  const IpczResult result = Trap(
      portal, conditions, [&](const IpczTrapEvent&) { notification.Notify(); });

  switch (result) {
    case IPCZ_RESULT_OK:
      notification.WaitForNotification();
      return IPCZ_RESULT_OK;

    case IPCZ_RESULT_FAILED_PRECONDITION:
      return IPCZ_RESULT_OK;

    default:
      return result;
  }
}

IpczResult TestBase::WaitForConditionFlags(IpczHandle portal,
                                           IpczTrapConditionFlags flags) {
  const IpczTrapConditions conditions = {
      .size = sizeof(conditions),
      .flags = flags,
  };
  return WaitForConditions(portal, conditions);
}

IpczResult TestBase::WaitToGet(IpczHandle portal,
                               std::string* message,
                               absl::Span<IpczHandle> handles) {
  const IpczTrapConditions conditions = {
      .size = sizeof(conditions),
      .flags = IPCZ_TRAP_ABOVE_MIN_LOCAL_PARCELS,
      .min_local_parcels = 0,
  };
  IpczResult result = WaitForConditions(portal, conditions);
  if (result != IPCZ_RESULT_OK) {
    return result;
  }

  return Get(portal, message, handles);
}

void TestBase::VerifyEndToEnd(IpczHandle portal) {
  static const char kTestMessage[] = "Ping!!!";
  std::string message;
  EXPECT_EQ(IPCZ_RESULT_OK, Put(portal, kTestMessage));
  EXPECT_EQ(IPCZ_RESULT_OK, WaitToGet(portal, &message));
  EXPECT_EQ(kTestMessage, message);
}

void TestBase::VerifyEndToEndLocal(IpczHandle a, IpczHandle b) {
  const std::string kMessage1 = "psssst";
  const std::string kMessage2 = "ssshhh";

  Put(a, kMessage1);
  Put(b, kMessage2);

  std::string message;
  EXPECT_EQ(IPCZ_RESULT_OK, WaitToGet(a, &message));
  EXPECT_EQ(kMessage2, message);
  EXPECT_EQ(IPCZ_RESULT_OK, WaitToGet(b, &message));
  EXPECT_EQ(kMessage1, message);
}

void TestBase::HandleEvent(const IpczTrapEvent* event) {
  auto handler =
      absl::WrapUnique(reinterpret_cast<TrapEventHandler*>(event->context));
  (*handler)(*event);
}

}  // namespace ipcz::test::internal
