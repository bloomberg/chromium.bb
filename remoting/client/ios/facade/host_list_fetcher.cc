// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/ios/facade/host_list_fetcher.h"

#include <thread>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_fetcher.h"
#include "remoting/base/url_request_context_getter.h"

namespace remoting {

HostListFetcher::HostListFetcher(
    const scoped_refptr<net::URLRequestContextGetter>&
        url_request_context_getter)
    : url_request_context_getter_(url_request_context_getter) {}

HostListFetcher::~HostListFetcher() {}

// TODO(nicholss): This was written assuming only one request at a time. Fix
// that. For the moment it will work to make progress in the app.
void HostListFetcher::RetrieveHostlist(const std::string& access_token,
                                       const HostlistCallback& callback) {
  // TODO(nicholss): There is a bug here if two host list fetches are happening
  // at the same time there will be a dcheck thrown. Fix this for release.
  DCHECK(!access_token.empty());
  DCHECK(callback);
  DCHECK(!hostlist_callback_);

  hostlist_callback_ = callback;

  request_ = net::URLFetcher::Create(GURL(kHostListProdRequestUrl),
                                     net::URLFetcher::GET, this);
  request_->SetRequestContext(url_request_context_getter_.get());
  request_->AddExtraRequestHeader("Authorization: OAuth " + access_token);
  request_->SetMaxRetriesOn5xx(0);
  request_->SetAutomaticallyRetryOnNetworkChanges(3);
  request_->Start();
}

bool HostListFetcher::ProcessResponse(
    std::vector<remoting::HostInfo>* hostlist) {
  int response_code = request_->GetResponseCode();
  if (response_code != net::HTTP_OK) {
    LOG(ERROR) << "Hostlist request failed with error code: " << response_code;
    return false;
  }

  std::string response_string;
  if (!request_->GetResponseAsString(&response_string)) {
    LOG(ERROR) << "Failed to retrieve Hostlist response data";
    return false;
  }

  std::unique_ptr<base::Value> response_value(
      base::JSONReader::Read(response_string));
  if (!response_value ||
      !response_value->IsType(base::Value::Type::DICTIONARY)) {
    LOG(ERROR) << "Failed to parse response string to JSON";
    return false;
  }

  const base::DictionaryValue* response;
  if (!response_value->GetAsDictionary(&response)) {
    LOG(ERROR) << "Failed to convert parsed JSON to a dictionary object";
    return false;
  }

  const base::DictionaryValue* data = nullptr;
  if (!response->GetDictionary("data", &data)) {
    LOG(ERROR) << "Hostlist response data is empty";
    return false;
  }

  const base::ListValue* hosts = nullptr;
  if (!data->GetList("items", &hosts)) {
    LOG(ERROR) << "Failed to find hosts in Hostlist response data";
    return false;
  }

  // Any host_info with malformed data will not be added to the hostlist.
  const base::DictionaryValue* host_dict;
  for (const auto& host_info : *hosts) {
    remoting::HostInfo host;
    if (host_info.GetAsDictionary(&host_dict) &&
        host.ParseHostInfo(*host_dict)) {
      hostlist->push_back(host);
    }
  }
  return true;
}

void HostListFetcher::OnURLFetchComplete(const net::URLFetcher* source) {
  DCHECK(source);

  std::vector<HostInfo> hostlist;
  if (!ProcessResponse(&hostlist)) {
    hostlist.clear();
  }
  base::ResetAndReturn(&hostlist_callback_).Run(hostlist);
}

}  // namespace remoting
