// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/setup/service_client.h"

#include <memory>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

namespace remoting {

class ServiceClient::Core
    : public base::RefCountedThreadSafe<ServiceClient::Core> {
 public:
  Core(const std::string& chromoting_hosts_url,
       scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
      : url_loader_factory_(url_loader_factory),
        delegate_(nullptr),
        pending_request_type_(PENDING_REQUEST_NONE),
        chromoting_hosts_url_(chromoting_hosts_url) {}

  void RegisterHost(const std::string& host_id,
                    const std::string& host_name,
                    const std::string& public_key,
                    const std::string& host_client_id,
                    const std::string& oauth_access_token,
                    ServiceClient::Delegate* delegate);

  void UnregisterHost(const std::string& host_id,
                      const std::string& oauth_access_token,
                      ServiceClient::Delegate* delegate);

  void OnURLLoadComplete(std::unique_ptr<std::string> response_body);

 private:
  friend class base::RefCountedThreadSafe<Core>;
  ~Core() = default;

  enum PendingRequestType {
    PENDING_REQUEST_NONE,
    PENDING_REQUEST_REGISTER_HOST,
    PENDING_REQUEST_UNREGISTER_HOST
  };

  void MakeChromotingRequest(const std::string& request_type,
                             const std::string& post_body,
                             const std::string& url_suffix,
                             const std::string& oauth_access_token,
                             ServiceClient::Delegate* delegate);

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  ServiceClient::Delegate* delegate_;
  std::unique_ptr<network::SimpleURLLoader> url_loader_;
  PendingRequestType pending_request_type_;
  std::string chromoting_hosts_url_;
};

void ServiceClient::Core::RegisterHost(
    const std::string& host_id,
    const std::string& host_name,
    const std::string& public_key,
    const std::string& host_client_id,
    const std::string& oauth_access_token,
    Delegate* delegate) {
  DCHECK(pending_request_type_ == PENDING_REQUEST_NONE);
  pending_request_type_ = PENDING_REQUEST_REGISTER_HOST;
  base::DictionaryValue post_body;
  post_body.SetString("data.hostId", host_id);
  post_body.SetString("data.hostName", host_name);
  post_body.SetString("data.publicKey", public_key);
  std::string url_suffix;
  if (!host_client_id.empty())
    url_suffix = "?hostClientId=" + host_client_id;
  std::string post_body_str;
  base::JSONWriter::Write(post_body, &post_body_str);
  MakeChromotingRequest("POST", url_suffix, post_body_str, oauth_access_token,
                        delegate);
}

void ServiceClient::Core::UnregisterHost(
    const std::string& host_id,
    const std::string& oauth_access_token,
    Delegate* delegate) {
  DCHECK(pending_request_type_ == PENDING_REQUEST_NONE);
  pending_request_type_ = PENDING_REQUEST_UNREGISTER_HOST;
  MakeChromotingRequest("DELETE", host_id, std::string(), oauth_access_token,
                        delegate);
}

void ServiceClient::Core::MakeChromotingRequest(
    const std::string& request_type,
    const std::string& url_suffix,
    const std::string& request_body,
    const std::string& oauth_access_token,
    ServiceClient::Delegate* delegate) {
  delegate_ = delegate;
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GURL(chromoting_hosts_url_ + url_suffix);
  resource_request->method = request_type;
  resource_request->headers.SetHeader(
      "Authorization", std::string("OAuth ") + oauth_access_token);

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("CRD_service_client",
                                          R"(
        semantics {
          sender: "CRD Service Client"
          description: "Client implementation for the chromoting service."
          trigger:
            "Manually triggered running <out>/remoting_start_host."
          data: "No user data."
          destination: OTHER
          destination_other:
            "The Chrome Remote Desktop client/host the user is connecting to."
        }
        policy {
          cookies_allowed: NO
          setting:
            "This request cannot be stopped in settings, but will not be sent "
            "if user does not use Chrome Remote Desktop."
          policy_exception_justification:
            "Not implemented."
        })");

  url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 traffic_annotation);
  url_loader_->AttachStringForUpload(request_body,
                                     "application/json; charset=UTF-8");
  url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&ServiceClient::Core::OnURLLoadComplete,
                     base::Unretained(this)));
}

void ServiceClient::Core::OnURLLoadComplete(
    std::unique_ptr<std::string> response_body) {
  DCHECK(pending_request_type_ != PENDING_REQUEST_NONE);
  PendingRequestType old_type = pending_request_type_;
  pending_request_type_ = PENDING_REQUEST_NONE;

  int response_code = -1;
  if (url_loader_->ResponseInfo() && url_loader_->ResponseInfo()->headers) {
    response_code = url_loader_->ResponseInfo()->headers->response_code();
  }
  url_loader_.reset();

  if (response_code == net::HTTP_BAD_REQUEST) {
    delegate_->OnOAuthError();
    return;
  }

  if (response_body) {
    // Treat codes 2xx as successful; for example, HTTP_NO_CONTENT (204) can be
    // returned from a DELETE_REQUEST.
    DCHECK(response_code == -1 || (response_code / 100 == 2));
    switch (old_type) {
      case PENDING_REQUEST_NONE:
        break;
      case PENDING_REQUEST_REGISTER_HOST:
        {
        std::string data = *response_body;
        std::unique_ptr<base::Value> message_value =
            base::JSONReader::Read(data);
        base::DictionaryValue* dict;
        std::string code;
        if (message_value.get() && message_value->is_dict() &&
            message_value->GetAsDictionary(&dict) &&
            dict->GetString("data.authorizationCode", &code)) {
          delegate_->OnHostRegistered(code);
          } else {
            delegate_->OnHostRegistered(std::string());
          }
        }
        break;
      case PENDING_REQUEST_UNREGISTER_HOST:
        delegate_->OnHostUnregistered();
        break;
    }
    return;
  }

  delegate_->OnNetworkError(response_code);
}

ServiceClient::ServiceClient(
    const std::string& chromoting_hosts_url,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  core_ = new Core(chromoting_hosts_url, url_loader_factory);
}

ServiceClient::~ServiceClient() = default;

void ServiceClient::RegisterHost(
    const std::string& host_id,
    const std::string& host_name,
    const std::string& public_key,
    const std::string& host_client_id,
    const std::string& oauth_access_token,
    Delegate* delegate) {
  return core_->RegisterHost(host_id, host_name, public_key, host_client_id,
                             oauth_access_token, delegate);
}

void ServiceClient::UnregisterHost(
    const std::string& host_id,
    const std::string& oauth_access_token,
    Delegate* delegate) {
  return core_->UnregisterHost(host_id, oauth_access_token, delegate);
}

}  // namespace gaia
