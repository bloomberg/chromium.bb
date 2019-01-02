// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/web_database_host_impl.h"

#include "base/strings/utf_string_conversions.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "mojo/core/embedder/embedder.h"
#include "storage/common/database/database_identifier.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {
base::string16 ConstructVfsFileName(const url::Origin& origin,
                                    const base::string16& name,
                                    const base::string16& suffix) {
  std::string identifier = storage::GetIdentifierFromOrigin(origin);
  return base::UTF8ToUTF16(identifier) + base::ASCIIToUTF16("/") + name +
         base::ASCIIToUTF16("#") + suffix;
}

class BadMessageObserver {
 public:
  BadMessageObserver()
      : dummy_message_(0, 0, 0, 0, nullptr), context_(&dummy_message_) {
    mojo::core::SetDefaultProcessErrorCallback(base::BindRepeating(
        &BadMessageObserver::ReportBadMessage, base::Unretained(this)));
  }

  ~BadMessageObserver() {
    mojo::core::SetDefaultProcessErrorCallback(
        mojo::core::ProcessErrorCallback());
  }

  const std::string& last_error() const { return last_error_; }

 private:
  void ReportBadMessage(const std::string& error) { last_error_ = error; }

  mojo::Message dummy_message_;
  mojo::internal::MessageDispatchContext context_;
  std::string last_error_;

  DISALLOW_COPY_AND_ASSIGN(BadMessageObserver);
};
}  // namespace

class WebDatabaseHostImplTest : public ::testing::Test {
 protected:
  WebDatabaseHostImplTest() = default;
  ~WebDatabaseHostImplTest() override = default;

  void SetUp() override {
    scoped_refptr<storage::DatabaseTracker> db_tracker =
        base::MakeRefCounted<storage::DatabaseTracker>(
            base::FilePath(), /*is_incognito=*/false,
            /*special_storage_policy=*/nullptr,
            /*quota_manager_proxy=*/nullptr);
    db_tracker->set_task_runner_for_testing(
        base::ThreadTaskRunnerHandle::Get());

    host_ = std::make_unique<WebDatabaseHostImpl>(process_id(),
                                                  std::move(db_tracker));
  }

  template <typename Callable>
  void CheckUnauthorizedOrigin(const Callable& func) {
    BadMessageObserver bad_message_observer;
    func();
    EXPECT_EQ("Unauthorized origin.", bad_message_observer.last_error());
  }

  template <typename Callable>
  void CheckInvalidOrigin(const Callable& func) {
    BadMessageObserver bad_message_observer;
    func();
    EXPECT_EQ("Invalid origin.", bad_message_observer.last_error());
  }

  WebDatabaseHostImpl* host() { return host_.get(); }
  int process_id() { return kProcessId; }

 private:
  static constexpr int kProcessId = 1234;
  TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<WebDatabaseHostImpl> host_;

  DISALLOW_COPY_AND_ASSIGN(WebDatabaseHostImplTest);
};

TEST_F(WebDatabaseHostImplTest, BadMessagesUnauthorized) {
  const url::Origin correct_origin =
      url::Origin::Create(GURL("http://correct.com"));
  const url::Origin incorrect_origin =
      url::Origin::Create(GURL("http://incorrect.net"));
  const base::string16 db_name(base::ASCIIToUTF16("db_name"));
  const base::string16 suffix(base::ASCIIToUTF16("suffix"));
  const base::string16 bad_vfs_file_name =
      ConstructVfsFileName(incorrect_origin, db_name, suffix);

  auto* security_policy = ChildProcessSecurityPolicyImpl::GetInstance();
  security_policy->Add(process_id());
  security_policy->AddIsolatedOrigins({correct_origin, incorrect_origin});
  security_policy->LockToOrigin(process_id(), correct_origin.GetURL());
  ASSERT_TRUE(security_policy->CanAccessDataForOrigin(process_id(),
                                                      correct_origin.GetURL()));
  ASSERT_FALSE(security_policy->CanAccessDataForOrigin(
      process_id(), incorrect_origin.GetURL()));

  CheckUnauthorizedOrigin([&]() {
    host()->OpenFile(bad_vfs_file_name,
                     /*desired_flags=*/0, base::BindOnce([](base::File) {}));
  });

  CheckUnauthorizedOrigin([&]() {
    host()->DeleteFile(bad_vfs_file_name,
                       /*sync_dir=*/false, base::BindOnce([](int32_t) {}));
  });

  CheckUnauthorizedOrigin([&]() {
    host()->GetFileAttributes(bad_vfs_file_name,
                              base::BindOnce([](int32_t) {}));
  });

  CheckUnauthorizedOrigin([&]() {
    host()->GetFileSize(bad_vfs_file_name, base::BindOnce([](int64_t) {}));
  });

  CheckUnauthorizedOrigin([&]() {
    host()->SetFileSize(bad_vfs_file_name, /*expected_size=*/0,
                        base::BindOnce([](bool) {}));
  });

  CheckUnauthorizedOrigin([&]() {
    host()->GetSpaceAvailable(incorrect_origin, base::BindOnce([](int64_t) {}));
  });

  CheckUnauthorizedOrigin([&]() {
    host()->Opened(incorrect_origin, db_name, base::ASCIIToUTF16("description"),
                   /*estimated_size=*/0);
  });

  CheckUnauthorizedOrigin(
      [&]() { host()->Modified(incorrect_origin, db_name); });

  CheckUnauthorizedOrigin([&]() { host()->Closed(incorrect_origin, db_name); });

  CheckUnauthorizedOrigin([&]() {
    host()->HandleSqliteError(incorrect_origin, db_name, /*error=*/0);
  });
}

TEST_F(WebDatabaseHostImplTest, BadMessagesInvalid) {
  const url::Origin opaque_origin;
  const base::string16 db_name(base::ASCIIToUTF16("db_name"));

  CheckInvalidOrigin([&]() {
    host()->GetSpaceAvailable(opaque_origin, base::BindOnce([](int64_t) {}));
  });

  CheckInvalidOrigin([&]() {
    host()->Opened(opaque_origin, db_name, base::ASCIIToUTF16("description"),
                   /*estimated_size=*/0);
  });

  CheckInvalidOrigin([&]() { host()->Modified(opaque_origin, db_name); });

  CheckInvalidOrigin([&]() { host()->Closed(opaque_origin, db_name); });

  CheckInvalidOrigin([&]() {
    host()->HandleSqliteError(opaque_origin, db_name, /*error=*/0);
  });
}

}  // namespace content
