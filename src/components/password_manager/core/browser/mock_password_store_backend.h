// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MOCK_PASSWORD_STORE_BACKEND_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MOCK_PASSWORD_STORE_BACKEND_H_

#include <memory>
#include <vector>
#include "base/callback_forward.h"
#include "base/time/time.h"
#include "components/password_manager/core/browser/field_info_store.h"
#include "components/password_manager/core/browser/password_store_backend.h"
#include "components/password_manager/core/browser/smart_bubble_stats_store.h"
#include "components/sync/model/proxy_model_type_controller_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

namespace password_manager {

class MockPasswordStoreBackend : public PasswordStoreBackend {
 public:
  MockPasswordStoreBackend();
  ~MockPasswordStoreBackend() override;

  MOCK_METHOD(void,
              InitBackend,
              (RemoteChangesReceived remote_form_changes_received,
               base::RepeatingClosure sync_enabled_or_disabled_cb,
               base::OnceCallback<void(bool)> completion),
              (override));
  MOCK_METHOD(void, Shutdown, (base::OnceClosure), (override));

  MOCK_METHOD(void, GetAllLoginsAsync, (LoginsReply callback), (override));
  MOCK_METHOD(void,
              GetAutofillableLoginsAsync,
              (LoginsReply callback),
              (override));
  MOCK_METHOD(void,
              FillMatchingLoginsAsync,
              (LoginsReply callback,
               bool include_psl,
               const std::vector<PasswordFormDigest>& forms),
              (override));
  MOCK_METHOD(void,
              AddLoginAsync,
              (const PasswordForm& form, PasswordStoreChangeListReply callback),
              (override));
  MOCK_METHOD(void,
              UpdateLoginAsync,
              (const PasswordForm& form, PasswordStoreChangeListReply callback),
              (override));
  MOCK_METHOD(void,
              RemoveLoginAsync,
              (const PasswordForm& form, PasswordStoreChangeListReply callback),
              (override));
  MOCK_METHOD(void,
              RemoveLoginsByURLAndTimeAsync,
              (const base::RepeatingCallback<bool(const GURL&)>& url_filter,
               base::Time delete_begin,
               base::Time delete_end,
               base::OnceCallback<void(bool)> sync_completion,
               PasswordStoreChangeListReply callback),
              (override));
  MOCK_METHOD(void,
              RemoveLoginsCreatedBetweenAsync,
              (base::Time delete_begin,
               base::Time delete_end,
               PasswordStoreChangeListReply callback),
              (override));
  MOCK_METHOD(void,
              DisableAutoSignInForOriginsAsync,
              (const base::RepeatingCallback<bool(const GURL&)>&,
               base::OnceClosure),
              (override));
  MOCK_METHOD(SmartBubbleStatsStore*, GetSmartBubbleStatsStore, (), (override));
  MOCK_METHOD(FieldInfoStore*, GetFieldInfoStore, (), (override));
  MOCK_METHOD(std::unique_ptr<syncer::ProxyModelTypeControllerDelegate>,
              CreateSyncControllerDelegate,
              (),
              (override));
  MOCK_METHOD(void,
              GetSyncStatus,
              (base::OnceCallback<void(bool)>),
              (override));
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MOCK_PASSWORD_STORE_BACKEND_H_
