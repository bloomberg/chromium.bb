// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/setup/service_client.h"

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_context_getter.h"
#include "url/gurl.h"

namespace remoting {

class ServiceClient::Core
    : public base::RefCountedThreadSafe<ServiceClient::Core>,
      public net::URLFetcherDelegate {
 public:
  Core(const std::string& chromoting_hosts_url,
       net::URLRequestContextGetter* request_context_getter)
           : request_context_getter_(request_context_getter),
             delegate_(NULL),
             pending_request_type_(PENDING_REQUEST_NONE),
             chromoting_hosts_url_(chromoting_hosts_url) {
  }

  void RegisterHost(const std::string& host_id,
                    const std::string& host_name,
                    const std::string& public_key,
                    const std::string& host_client_id,
                    const std::string& oauth_access_token,
                    ServiceClient::Delegate* delegate);

  void UnregisterHost(const std::string& host_id,
                      const std::string& oauth_access_token,
                      ServiceClient::Delegate* delegate);

  // net::URLFetcherDelegate implementation.
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;

 private:
  friend class base::RefCountedThreadSafe<Core>;
  virtual ~Core() {}

  enum PendingRequestType {
    PENDING_REQUEST_NONE,
    PENDING_REQUEST_REGISTER_HOST,
    PENDING_REQUEST_UNREGISTER_HOST
  };

  void MakeChromotingRequest(net::URLFetcher::RequestType request_type,
                       const std::string& post_body,
                       const std::string& url_suffix,
                       const std::string& oauth_access_token,
                       ServiceClient::Delegate* delegate);
  void HandleResponse(const net::URLFetcher* source);

  scoped_refptr<net::URLRequestContextGetter> request_context_getter_;
  ServiceClient::Delegate* delegate_;
  scoped_ptr<net::URLFetcher> request_;
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
  base::JSONWriter::Write(&post_body, &post_body_str);
  MakeChromotingRequest(net::URLFetcher::POST,
                        url_suffix,
                        post_body_str,
                        oauth_access_token,
                        delegate);
}

void ServiceClient::Core::UnregisterHost(
    const std::string& host_id,
    const std::string& oauth_access_token,
    Delegate* delegate) {
  DCHECK(pending_request_type_ == PENDING_REQUEST_NONE);
  pending_request_type_ = PENDING_REQUEST_UNREGISTER_HOST;
  MakeChromotingRequest(net::URLFetcher::DELETE_REQUEST,
                        host_id,
                        std::string(),
                        oauth_access_token,
                        delegate);
}

void ServiceClient::Core::MakeChromotingRequest(
    net::URLFetcher::RequestType request_type,
    const std::string& url_suffix,
    const std::string& request_body,
    const std::string& oauth_access_token,
    ServiceClient::Delegate* delegate) {
  delegate_ = delegate;
  request_.reset(net::URLFetcher::Create(
      0, GURL(chromoting_hosts_url_ + url_suffix), request_type, this));
  request_->SetRequestContext(request_context_getter_.get());
  request_->SetUploadData("application/json; charset=UTF-8", request_body);
  request_->AddExtraRequestHeader("Authorization: OAuth " + oauth_access_token);
  request_->Start();
}

// URLFetcher::Delegate implementation.
void ServiceClient::Core::OnURLFetchComplete(
    const net::URLFetcher* source) {
  HandleResponse(source);
  request_.reset();
}

void ServiceClient::Core::HandleResponse(const net::URLFetcher* source) {
  DCHECK(pending_request_type_ != PENDING_REQUEST_NONE);
  PendingRequestType old_type = pending_request_type_;
  pending_request_type_ = PENDING_REQUEST_NONE;
  if (source->GetResponseCode() == net::HTTP_BAD_REQUEST) {
    delegate_->OnOAuthError();
    return;
  }

  // Treat codes 2xx as successful; for example, HTTP_NO_CONTENT (204) can be
  // returned from a DELETE_REQUEST.
  if (source->GetResponseCode() / 100 == 2) {
    switch (old_type) {
      case PENDING_REQUEST_NONE:
        break;
      case PENDING_REQUEST_REGISTER_HOST:
        {
          std::string data;
          source->GetResponseAsString(&data);
          scoped_ptr<base::Value> message_value(base::JSONReader::Read(data));
          base::DictionaryValue *dict;
          std::string code;
          if (message_value.get() &&
              message_value->IsType(base::Value::TYPE_DICTIONARY) &&
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
  delegate_->OnNetworkError(source->GetResponseCode());
}

ServiceClient::ServiceClient(const std::string& chromoting_hosts_url,
                             net::URLRequestContextGetter* context_getter) {
  core_ = new Core(chromoting_hosts_url, context_getter);
}

ServiceClient::~ServiceClient() {
}

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
