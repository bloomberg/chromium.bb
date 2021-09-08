// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_CONTACTS_NEARBY_SHARE_CONTACT_DOWNLOADER_IMPL_H_
#define CHROME_BROWSER_NEARBY_SHARING_CONTACTS_NEARBY_SHARE_CONTACT_DOWNLOADER_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_http_result.h"
#include "chrome/browser/nearby_sharing/contacts/nearby_share_contact_downloader.h"
#include "chrome/browser/nearby_sharing/proto/contact_rpc.pb.h"
#include "chrome/browser/nearby_sharing/proto/rpc_resources.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class NearbyShareClient;
class NearbyShareClientFactory;

// An implementation of the NearbyShareContactDownloader that uses
// NearbyShareClients to make HTTP calls. This class enforces a timeout for all
// HTTP calls but does not retry failures.
// TODO(https://crbug.com/1114516): Use SimpleURLLoader timeout functionality
// after refactoring our Nearby Share HTTP client.
class NearbyShareContactDownloaderImpl : public NearbyShareContactDownloader {
 public:
  class Factory {
   public:
    static std::unique_ptr<NearbyShareContactDownloader> Create(
        const std::string& device_id,
        base::TimeDelta timeout,
        NearbyShareClientFactory* client_factory,
        SuccessCallback success_callback,
        FailureCallback failure_callback);
    static void SetFactoryForTesting(Factory* test_factory);

   protected:
    virtual ~Factory();
    virtual std::unique_ptr<NearbyShareContactDownloader> CreateInstance(
        const std::string& device_id,
        base::TimeDelta timeout,
        NearbyShareClientFactory* client_factory,
        SuccessCallback success_callback,
        FailureCallback failure_callback) = 0;

   private:
    static Factory* test_factory_;
  };

  ~NearbyShareContactDownloaderImpl() override;

 private:
  // |timeout|: The maximum amount of time to wait between the request and
  //            response of each HTTP call before failing.
  NearbyShareContactDownloaderImpl(const std::string& device_id,
                                   base::TimeDelta timeout,
                                   NearbyShareClientFactory* client_factory,
                                   SuccessCallback success_callback,
                                   FailureCallback failure_callback);

  // NearbyShareContactDownloader:
  void OnRun() override;

  void CallListContactPeople(
      const absl::optional<std::string>& next_page_token);
  void OnListContactPeopleSuccess(
      const nearbyshare::proto::ListContactPeopleResponse& response);
  void OnListContactPeopleFailure(NearbyShareHttpError error);
  void OnListContactPeopleTimeout();

  size_t current_page_number_ = 0;
  std::vector<nearbyshare::proto::ContactRecord> contacts_;
  base::TimeDelta timeout_;
  NearbyShareClientFactory* client_factory_ = nullptr;
  base::TimeTicks start_timestamp_;
  std::unique_ptr<NearbyShareClient> client_;
  base::OneShotTimer timer_;
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_CONTACTS_NEARBY_SHARE_CONTACT_DOWNLOADER_IMPL_H_
