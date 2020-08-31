// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/trial_group/trial_group_checker.h"

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "net/base/load_flags.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace {

constexpr char kNetworkTrafficAnnotationProto[] = R"(
    semantics {
      sender: "Trial Group Lookup"
      description:
        "Obtains whether user is in the Google group for a dogfood trial."
      trigger: "On boot."
      data: "Dogfood enum identifier and credentials."
      destination: GOOGLE_OWNED_SERVICE
    }
    policy {
      cookies_allowed: NO
    }
)";
constexpr int kIsMember = 1;
constexpr char kServerUrl[] =
    "https://crosdogpack-pa.googleapis.com/v1/isMember";

}  // namespace

namespace chromeos {
namespace trial_group {

TrialGroupChecker::TrialGroupChecker(GroupId group_id)
    : server_url_(GURL(kServerUrl)), group_id_(group_id) {}

TrialGroupChecker::~TrialGroupChecker() = default;

void TrialGroupChecker::SetServerUrl(GURL server_url) {
  server_url_ = server_url;
}

void TrialGroupChecker::OnRequestComplete(
    std::unique_ptr<std::string> response_body) {
  const int net_error = loader_->NetError();

  int response_code = 0;
  if (loader_->ResponseInfo()) {
    response_code = loader_->ResponseInfo()->headers->response_code();
  }
  loader_.reset();

  const bool server_error =
      net_error != net::OK || (response_code >= 500 && response_code < 600);
  if (server_error || response_body->empty()) {
    std::move(callback_).Run(false);
    return;
  }

  base::JSONReader::ValueWithError membership_info =
      base::JSONReader::ReadAndReturnValueWithError(*response_body,
                                                    base::JSON_PARSE_RFC);
  if (!membership_info.value || !membership_info.value->is_dict()) {
    std::move(callback_).Run(false);
    return;
  }

  base::Value* member_status =
      membership_info.value->FindKey("membership_info");
  if (member_status == nullptr || !member_status->is_int()) {
    std::move(callback_).Run(false);
    return;
  }

  bool is_member = (member_status->GetInt() == kIsMember);
  std::move(callback_).Run(is_member);
}

TrialGroupChecker::Status TrialGroupChecker::LookUpMembership(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    base::OnceCallback<void(bool is_member)> callback) {
  // OnRequestComplete has not completed from a previous call, so exit.
  if (loader_)
    return TrialGroupChecker::PREVIOUS_CALL_RUNNING;

  callback_ = std::move(callback);

  std::string upload_data;
  {
    base::DictionaryValue request;
    request.SetInteger("group", static_cast<int>(group_id_));
    base::JSONWriter::Write(request, &upload_data);
  }

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("trial_group_lookup",
                                          kNetworkTrafficAnnotationProto);

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = server_url_;

  DCHECK(resource_request->url.is_valid());
  resource_request->method = "POST";
  resource_request->load_flags =
      net::LOAD_BYPASS_CACHE | net::LOAD_DISABLE_CACHE;
  resource_request->credentials_mode =
      network::mojom::CredentialsMode::kInclude;

  loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                             traffic_annotation);

  loader_->AttachStringForUpload(upload_data, "application/json");
  loader_->DownloadToString(
      url_loader_factory.get(),
      base::BindOnce(&TrialGroupChecker::OnRequestComplete,
                     weak_factory_.GetWeakPtr()),
      1024 /* 1 kiB */);
  return TrialGroupChecker::OK;
}

}  // namespace trial_group
}  // namespace chromeos
