// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_STORE_ANDROID_BACKEND_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_STORE_ANDROID_BACKEND_H_

#include <memory>
#include <unordered_map>

#include "base/containers/small_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/types/pass_key.h"
#include "base/types/strong_alias.h"
#include "chrome/browser/password_manager/android/password_manager_lifecycle_helper.h"
#include "chrome/browser/password_manager/android/password_store_android_backend_bridge.h"
#include "components/password_manager/core/browser/password_store_backend.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace syncer {
class ModelTypeControllerDelegate;
}  // namespace syncer

namespace password_manager {

class PasswordSyncControllerDelegateAndroid;

// Android-specific password store backend that delegates every request to
// Google Mobile Service.
// It uses a `PasswordStoreAndroidBackendBridge` to send API requests for each
// method it implements from `PasswordStoreBackend`. The response will invoke a
// consumer method with an originally provided `JobId`. Based on that `JobId`,
// this class maps ongoing jobs to the callbacks of the methods that originally
// required the job since JNI itself can't preserve the callbacks.
class PasswordStoreAndroidBackend
    : public PasswordStoreBackend,
      public PasswordStoreAndroidBackendBridge::Consumer {
 public:
  PasswordStoreAndroidBackend();
  PasswordStoreAndroidBackend(
      base::PassKey<class PasswordStoreAndroidBackendTest>,
      std::unique_ptr<PasswordStoreAndroidBackendBridge> bridge,
      std::unique_ptr<PasswordManagerLifecycleHelper> lifecycle_helper);
  ~PasswordStoreAndroidBackend() override;

 private:
  SEQUENCE_CHECKER(main_sequence_checker_);

  using MetricInfix = base::StrongAlias<struct MetricNameTag, std::string>;

  // Records metrics for an asynchronous job or a series of jobs. The job is
  // expected to have started when the MetricsRecorder instance is created.
  // Latency is reported in RecordMetrics() under that assumption.
  class MetricsRecorder {
   public:
    MetricsRecorder();
    explicit MetricsRecorder(MetricInfix metric_name);
    MetricsRecorder(MetricsRecorder&&);
    MetricsRecorder& operator=(MetricsRecorder&&);
    ~MetricsRecorder();

    // Records the following metrics:
    // - "PasswordManager.PasswordStoreAndroidBackend.<metric_infix_>.Latency"
    // - "PasswordManager.PasswordStoreAndroidBackend.<metric_infix_>.Success"
    // When |error| is specified, the following metrcis are recorded in
    // addition:
    // - "PasswordManager.PasswordStoreAndroidBackend.APIError"
    // - "PasswordManager.PasswordStoreAndroidBackend.ErrorCode"
    // - "PasswordManager.PasswordStoreAndroidBackend.<metric_infix_>.APIError"
    // - "PasswordManager.PasswordStoreAndroidBackend.<metric_infix_>.ErrorCode"
    void RecordMetrics(bool success,
                       absl::optional<AndroidBackendError> error) const;

   private:
    MetricInfix metric_infix_;
    base::Time start_ = base::Time::Now();
  };

  class ClearAllLocalPasswordsMetricRecorder;

  // Wraps the handler for an asynchronous job (if successful) and invokes the
  // supplied metrics recorded upon completion. An object of this type shall be
  // created and stored in |request_for_job_| once an asynchronous begins, and
  // destroyed once the job is finished.
  class JobReturnHandler {
   public:
    using ErrorReply = base::OnceClosure;

    JobReturnHandler();
    JobReturnHandler(LoginsOrErrorReply callback,
                     MetricsRecorder metrics_recorder);
    JobReturnHandler(PasswordStoreChangeListReply callback,
                     MetricsRecorder metrics_recorder);
    JobReturnHandler(JobReturnHandler&&);
    JobReturnHandler& operator=(JobReturnHandler&&);
    ~JobReturnHandler();

    template <typename T>
    bool Holds() const {
      return absl::holds_alternative<T>(success_callback_);
    }

    template <typename T>
    T&& Get() && {
      return std::move(absl::get<T>(success_callback_));
    }

    void RecordMetrics(absl::optional<AndroidBackendError> error) const;

   private:
    absl::variant<LoginsOrErrorReply, PasswordStoreChangeListReply>
        success_callback_;
    MetricsRecorder metrics_recorder_;
  };

  using JobId = PasswordStoreAndroidBackendBridge::JobId;
  // Using a small_map should ensure that we handle rare cases with many jobs
  // like a bulk deletion just as well as the normal, rather small job load.
  using JobMap = base::small_map<
      std::unordered_map<JobId, JobReturnHandler, JobId::Hasher>>;
  using AccumulatedLoginsReply =
      base::OnceCallback<void(std::unique_ptr<LoginsResult>)>;
  using AccumulatedPasswordStoreChangeListReply =
      base::OnceCallback<void(std::unique_ptr<PasswordStoreChangeList>)>;

  // Implements PasswordStoreBackend interface.
  void InitBackend(RemoteChangesReceived stored_passwords_changed,
                   base::RepeatingClosure sync_enabled_or_disabled_cb,
                   base::OnceCallback<void(bool)> completion) override;
  void Shutdown(base::OnceClosure shutdown_completed) override;
  void GetAllLoginsAsync(LoginsOrErrorReply callback) override;
  void GetAutofillableLoginsAsync(LoginsOrErrorReply callback) override;
  void FillMatchingLoginsAsync(
      LoginsReply callback,
      bool include_psl,
      const std::vector<PasswordFormDigest>& forms) override;
  void AddLoginAsync(const PasswordForm& form,
                     PasswordStoreChangeListReply callback) override;
  void UpdateLoginAsync(const PasswordForm& form,
                        PasswordStoreChangeListReply callback) override;
  void RemoveLoginAsync(const PasswordForm& form,
                        PasswordStoreChangeListReply callback) override;
  void RemoveLoginsByURLAndTimeAsync(
      const base::RepeatingCallback<bool(const GURL&)>& url_filter,
      base::Time delete_begin,
      base::Time delete_end,
      base::OnceCallback<void(bool)> sync_completion,
      PasswordStoreChangeListReply callback) override;
  void RemoveLoginsCreatedBetweenAsync(
      base::Time delete_begin,
      base::Time delete_end,
      PasswordStoreChangeListReply callback) override;
  void DisableAutoSignInForOriginsAsync(
      const base::RepeatingCallback<bool(const GURL&)>& origin_filter,
      base::OnceClosure completion) override;
  SmartBubbleStatsStore* GetSmartBubbleStatsStore() override;
  FieldInfoStore* GetFieldInfoStore() override;
  std::unique_ptr<syncer::ProxyModelTypeControllerDelegate>
  CreateSyncControllerDelegate() override;
  void ClearAllLocalPasswords() override;

  // Implements PasswordStoreAndroidBackendBridge::Consumer interface.
  void OnCompleteWithLogins(PasswordStoreAndroidBackendBridge::JobId job_id,
                            std::vector<PasswordForm> passwords) override;
  void OnLoginsChanged(
      PasswordStoreAndroidBackendBridge::JobId task_id,
      absl::optional<PasswordStoreChangeList> changes) override;
  void OnError(PasswordStoreAndroidBackendBridge::JobId job_id,
               AndroidBackendError error) override;

  base::WeakPtr<syncer::ModelTypeControllerDelegate>
  GetSyncControllerDelegate();

  void QueueNewJob(JobId job_id, JobReturnHandler return_handler);
  JobReturnHandler GetAndEraseJob(JobId job_id);

  // Gets logins matching |form|.
  void GetLoginsAsync(const PasswordFormDigest& form,
                      bool include_psl,
                      LoginsOrErrorReply callback);

  // Filters |logins| created between |delete_begin| and |delete_end| time
  // that match |url_filer| and asynchronously removes them.
  void FilterAndRemoveLogins(
      const base::RepeatingCallback<bool(const GURL&)>& url_filter,
      base::Time delete_begin,
      base::Time delete_end,
      PasswordStoreChangeListReply reply,
      LoginsResultOrError result);

  // Filters logins that match |origin_filer| and asynchronously disables
  // autosignin by updating stored logins.
  void FilterAndDisableAutoSignIn(
      const base::RepeatingCallback<bool(const GURL&)>& origin_filter,
      PasswordStoreChangeListReply completion,
      LoginsResultOrError result);

  // Creates a metrics recorder that records latency and success metrics for
  // logins retrieval operation with |metric_infix| name prior to calling
  // |callback|.
  static LoginsOrErrorReply ReportMetricsAndInvokeCallbackForLoginsRetrieval(
      const MetricInfix& metric_infix,
      LoginsReply callback);

  // Creates a metrics recorder that records latency and success metrics for
  // store modification operation with |metric_infix| name prior to
  // calling |callback|.
  static PasswordStoreChangeListReply
  ReportMetricsAndInvokeCallbackForStoreModifications(
      const MetricInfix& metric_infix,
      PasswordStoreChangeListReply callback);

  // Returns the complete list of PasswordForms (regardless of their blocklist
  // status) from specified storage.
  void GetAllLoginsForTarget(PasswordStoreOperationTarget target,
                             LoginsOrErrorReply callback);

  // Removes |form| from specified storage.
  void RemoveLoginForTarget(const PasswordForm& form,
                            PasswordStoreOperationTarget target,
                            PasswordStoreChangeListReply callback);

  // Invoked synchronously by `lifecycle_helper_` when Chrome is foregrounded.
  // This should not cover the initial startup since the registration for the
  // event happens afterwads and is not repeated. A "foreground session" starts
  // when a Chrome activity resumes for the first time.
  void OnForegroundSessionStart();

  // Observer to propagate potential password changes to.
  RemoteChangesReceived stored_passwords_changed_;

  // Helper that receives lifecycle events via JNI and synchronously invokes a
  // passed callback, e.g. `OnForegroundSessionStart`.
  std::unique_ptr<PasswordManagerLifecycleHelper> lifecycle_helper_;

  // TaskRunner to run responses on the correct thread.
  scoped_refptr<base::SequencedTaskRunner> main_task_runner_;

  // Used to store callbacks for each invoked jobs since callbacks can't be
  // called via JNI directly.
  JobMap request_for_job_ GUARDED_BY_CONTEXT(main_sequence_checker_);

  // This object is the proxy to the JNI bridge that performs the API requests.
  std::unique_ptr<PasswordStoreAndroidBackendBridge> bridge_;

  // Delegate to handle sync events.
  std::unique_ptr<PasswordSyncControllerDelegateAndroid>
      sync_controller_delegate_;

  base::WeakPtrFactory<PasswordStoreAndroidBackend> weak_ptr_factory_{this};
};

}  // namespace password_manager

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_STORE_ANDROID_BACKEND_H_
