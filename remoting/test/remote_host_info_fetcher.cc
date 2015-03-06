// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/test/remote_host_info_fetcher.h"

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/stringprintf.h"
#include "base/thread_task_runner_handle.h"
#include "base/values.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_fetcher.h"
#include "remoting/base/url_request_context_getter.h"

namespace {
const char kRequestTestOrigin[] =
    "Origin: chrome-extension://ljacajndfccfgnfohlgkdphmbnpkjflk";
}

namespace remoting {
namespace test {

RemoteHostInfoFetcher::RemoteHostInfoFetcher() {}

RemoteHostInfoFetcher::~RemoteHostInfoFetcher() {}

bool RemoteHostInfoFetcher::RetrieveRemoteHostInfo(
    const std::string& application_id,
    const std::string& access_token,
    ServiceEnvironment service_environment,
    const RemoteHostInfoCallback& callback) {
  DCHECK(!application_id.empty());
  DCHECK(!access_token.empty());
  DCHECK(!callback.is_null());
  DCHECK(remote_host_info_callback_.is_null());

  DVLOG(2) << "RemoteHostInfoFetcher::RetrieveRemoteHostInfo() called";

  std::string service_url;
  switch (service_environment) {
    case kDeveloperEnvironment:
      DVLOG(1) << "Configuring service request for dev environment";
      service_url = base::StringPrintf(kDevServiceEnvironmentUrlFormat,
                                       application_id.c_str());
      break;

    case kTestingEnvironment:
      DVLOG(1) << "Configuring service request for test environment";
      service_url = base::StringPrintf(kTestServiceEnvironmentUrlFormat,
                                       application_id.c_str());
      break;

    default:
      LOG(ERROR) << "Unrecognized service type: " << service_environment;
      return false;
  }

  remote_host_info_callback_ = callback;

  scoped_refptr<remoting::URLRequestContextGetter> request_context_getter;
  request_context_getter = new remoting::URLRequestContextGetter(
      base::ThreadTaskRunnerHandle::Get(),   // network_runner
      base::ThreadTaskRunnerHandle::Get());  // file_runner

  request_.reset(
      net::URLFetcher::Create(GURL(service_url), net::URLFetcher::POST, this));
  request_->SetRequestContext(request_context_getter.get());
  request_->AddExtraRequestHeader("Authorization: OAuth " + access_token);
  request_->AddExtraRequestHeader(kRequestTestOrigin);
  request_->SetUploadData("application/json; charset=UTF-8", "{}");
  request_->Start();

  return true;
}

void RemoteHostInfoFetcher::OnURLFetchComplete(const net::URLFetcher* source) {
  DCHECK(source);
  DVLOG(2) << "URL Fetch Completed for: " << source->GetOriginalURL();

  RemoteHostInfo remote_host_info;
  int response_code = request_->GetResponseCode();
  if (response_code != net::HTTP_OK) {
    LOG(ERROR) << "RemoteHostInfo request failed with error code: "
               << response_code;
    remote_host_info_callback_.Run(remote_host_info);
    remote_host_info_callback_.Reset();
    return;
  }

  std::string response_string;
  if (!request_->GetResponseAsString(&response_string)) {
    LOG(ERROR) << "Failed to retrieve RemoteHostInfo response data";
    remote_host_info_callback_.Run(remote_host_info);
    remote_host_info_callback_.Reset();
    return;
  }

  scoped_ptr<base::Value> response_value(
      base::JSONReader::Read(response_string));
  if (!response_value ||
      !response_value->IsType(base::Value::TYPE_DICTIONARY)) {
    LOG(ERROR) << "Failed to parse response string to JSON";
    remote_host_info_callback_.Run(remote_host_info);
    remote_host_info_callback_.Reset();
    return;
  }

  std::string remote_host_status;
  const base::DictionaryValue* response;
  if (response_value->GetAsDictionary(&response)) {
    response->GetString("status", &remote_host_status);
  } else {
    LOG(ERROR) << "Failed to convert parsed JSON to a dictionary object";
    remote_host_info_callback_.Run(remote_host_info);
    remote_host_info_callback_.Reset();
    return;
  }

  remote_host_info.SetRemoteHostStatusFromString(remote_host_status);

  if (remote_host_info.IsReadyForConnection()) {
    response->GetString("host.applicationId", &remote_host_info.application_id);
    response->GetString("host.hostId", &remote_host_info.host_id);
    response->GetString("hostJid", &remote_host_info.host_jid);
    response->GetString("authorizationCode",
                        &remote_host_info.authorization_code);
    response->GetString("sharedSecret", &remote_host_info.shared_secret);
  }

  remote_host_info_callback_.Run(remote_host_info);
  remote_host_info_callback_.Reset();
}

}  // namespace test
}  // namespace remoting
