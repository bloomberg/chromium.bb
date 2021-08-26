// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MESSAGING_LAYER_PUBLIC_REPORT_CLIENT_H_
#define CHROME_BROWSER_POLICY_MESSAGING_LAYER_PUBLIC_REPORT_CLIENT_H_

#include <memory>
#include <queue>
#include <utility>

#include "base/memory/singleton.h"
#include "chrome/browser/policy/messaging_layer/upload/upload_client.h"
#include "chrome/browser/policy/messaging_layer/util/get_cloud_policy_client.h"
#include "components/reporting/client/report_queue_configuration.h"
#include "components/reporting/client/report_queue_provider.h"
#include "components/reporting/proto/record.pb.h"
#include "components/reporting/storage/storage_module_interface.h"
#include "components/reporting/storage/storage_uploader_interface.h"
#include "components/reporting/storage_selector/storage_selector.h"
#include "components/reporting/util/shared_queue.h"
#include "components/reporting/util/statusor.h"

namespace reporting {

// ReportingClient is an implementation of ReportQueueProvider for Chrome
// (currently only primary Chrome is supported, and so the client expects
// cloud policy client to be available and creates an uploader to use it.

class ReportingClient : public ReportQueueProvider {
 public:
  using CreateReportQueueResponse = StatusOr<std::unique_ptr<ReportQueue>>;

  using CreateReportQueueCallback =
      base::OnceCallback<void(CreateReportQueueResponse)>;

  class ClientInitializingContext
      : public ReportQueueProvider::InitializingContext {
   public:
    ClientInitializingContext(
        GetCloudPolicyClientCallback get_client_cb,
        UploaderInterface::AsyncStartUploaderCb async_start_upload_cb,
        InitCompleteCallback init_complete_cb,
        ReportingClient* client,
        scoped_refptr<InitializationStateTracker> init_state_tracker);

   private:
    // Destructor only called from Complete().
    // The class runs a series of callbacks each of which may invoke
    // either the next callback or Complete(). Thus eventually Complete()
    // is always called and InitializingContext instance is self-destruct.
    ~ClientInitializingContext() override;

    // Begins the process of configuring the ReportingClient.
    void OnStart() override;

    // Finally updates client with the elements of the configuration into the
    // ReportingClient, if the configuration process succeeded.
    void OnCompleted() override;

    // Called back after CloudPolicyClient configuration succeeded.
    void OnCloudPolicyClientConfigured(
        StatusOr<policy::CloudPolicyClient*> client_result);

    // Instantiates a StorageModuleInterface for ReportingClient to refer to.
    void ConfigureStorageModule();
    void OnStorageModuleConfigured(
        StatusOr<scoped_refptr<StorageModuleInterface>> storage_result);

    // Calls back upon creation of upload client.
    void OnUploadClientCreated(
        StatusOr<std::unique_ptr<UploadClient>> upload_client_result);

    GetCloudPolicyClientCallback get_client_cb_;
    UploaderInterface::AsyncStartUploaderCb async_start_upload_cb_;

    policy::CloudPolicyClient* cloud_policy_client_ = nullptr;
    std::unique_ptr<UploadClient> upload_client_;
    scoped_refptr<StorageModuleInterface> storage_;
    ReportingClient* const client_;
  };

  ReportQueueProvider::InitializingContext* InstantiateInitializingContext(
      InitCompleteCallback init_complete_cb,
      scoped_refptr<InitializationStateTracker> init_state_tracker) override;

  StatusOr<std::unique_ptr<ReportQueue>> CreateNewQueue(
      std::unique_ptr<ReportQueueConfiguration> config) override;

  StatusOr<std::unique_ptr<ReportQueue, base::OnTaskRunnerDeleter>>
  CreateNewSpeculativeQueue() override;

  // RAII class for testing ReportingClient - substitutes reporting files
  // location, signature verification public key and a cloud policy client
  // builder to return given client. Resets client when destructed.
  class TestEnvironment {
   public:
    TestEnvironment(const base::FilePath& reporting_path,
                    base::StringPiece verification_key,
                    policy::CloudPolicyClient* client);
    TestEnvironment(const TestEnvironment& other) = delete;
    TestEnvironment& operator=(const TestEnvironment& other) = delete;
    ~TestEnvironment();

   private:
    GetCloudPolicyClientCallback saved_build_cloud_policy_client_cb_;
  };

  ~ReportingClient() override;
  ReportingClient(const ReportingClient& other) = delete;
  ReportingClient& operator=(const ReportingClient& other) = delete;

 private:
  class Uploader;

  // Request for async start uploader (to be held in queue until upload_client
  // is set).
  class AsyncStartUploaderRequest {
   public:
    AsyncStartUploaderRequest(
        bool need_encryption_key,
        UploaderInterface::UploaderInterfaceResultCb start_uploader_cb);
    AsyncStartUploaderRequest(const AsyncStartUploaderRequest& other) = delete;
    AsyncStartUploaderRequest& operator=(
        const AsyncStartUploaderRequest& other) = delete;
    ~AsyncStartUploaderRequest();

    bool need_encryption_key() const;
    UploaderInterface::UploaderInterfaceResultCb& start_uploader_cb();

   private:
    const bool need_encryption_key_;
    UploaderInterface::UploaderInterfaceResultCb start_uploader_cb_;
  };

  friend class TestEnvironment;
  friend class ReportQueueProvider;
  friend struct base::DefaultSingletonTraits<ReportingClient>;

  // Constructor to be used by singleton only.
  ReportingClient();

  // Accesses singleton ReportingClient instance.
  // Separate from ReportQueueProvider::GetInstance, because
  // Singleton<ReportingClient>::get() can only be used inside ReportingClient
  // class.
  static ReportingClient* GetInstance();

  void OnInitState(bool reporting_client_configured);
  void OnInitializationComplete(Status init_status);

  static void AsyncStartUploader(
      bool need_encryption_key,
      UploaderInterface::UploaderInterfaceResultCb start_uploader_cb);

  void DeliverAsyncStartUploader(
      bool need_encryption_key,
      UploaderInterface::UploaderInterfaceResultCb start_uploader_cb);

  void FlushAsyncStartUploaderQueue();

  void SetUploadClient(std::unique_ptr<UploadClient> upload_client);

  base::FilePath reporting_path_;
  std::string verification_key_;
  GetCloudPolicyClientCallback build_cloud_policy_client_cb_;

  // The three member variables below are protected by
  // uploaders_queue_task_runner_.
  // TODO(chromium:1078512) Passing around a raw pointer is unsafe. Wrap
  // CloudPolicyClient and guard access.
  policy::CloudPolicyClient* cloud_policy_client_ = nullptr;
  std::unique_ptr<UploadClient> upload_client_;
  scoped_refptr<StorageModuleInterface> storage_;

  // Queue of async start uploader requests protected by sequenced task runner.
  // When new request is posted, upload_client_ might be not set yet; in that
  // case it is added to the queue and executed only once upload_client_ is set.
  std::queue<AsyncStartUploaderRequest> async_start_uploaders_queue_;
  scoped_refptr<base::SequencedTaskRunner> uploaders_queue_task_runner_;
  SEQUENCE_CHECKER(uploaders_queue_sequence_checker_);
};
}  // namespace reporting

#endif  // CHROME_BROWSER_POLICY_MESSAGING_LAYER_PUBLIC_REPORT_CLIENT_H_
