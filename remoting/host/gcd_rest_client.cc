// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/gcd_rest_client.h"

#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/message_loop/message_loop.h"
#include "base/thread_task_runner_handle.h"
#include "base/time/default_clock.h"
#include "base/values.h"
#include "net/url_request/url_fetcher.h"
#include "remoting/base/logging.h"

namespace remoting {

// Only 'patchState' requests are supported at the moment, but other
// types of requests may be added in the future.
struct GcdRestClient::PatchStateRequest {
  GcdRestClient::PatchStateCallback callback;
  scoped_ptr<net::URLFetcher> url_fetcher;
};

GcdRestClient::GcdRestClient(const std::string& gcd_base_url,
                             const std::string& gcd_device_id,
                             const scoped_refptr<net::URLRequestContextGetter>&
                                 url_request_context_getter,
                             OAuthTokenGetter* token_getter)
    : gcd_base_url_(gcd_base_url),
      gcd_device_id_(gcd_device_id),
      url_request_context_getter_(url_request_context_getter),
      token_getter_(token_getter),
      clock_(new base::DefaultClock),
      weak_factory_(this) {
}

GcdRestClient::~GcdRestClient() {
  while (!pending_requests_.empty()) {
    delete pending_requests_.front();
    pending_requests_.pop();
  }
}

void GcdRestClient::PatchState(
    scoped_ptr<base::DictionaryValue> patch_details,
    const GcdRestClient::PatchStateCallback& callback) {
  // Construct a status update message in the format GCD expects.  The
  // message looks like this, where "..." is filled in from
  // |patch_details|:
  //
  // {
  //   requestTimeMs: T,
  //   patches: [{
  //     timeMs: T,
  //     patch: {...}
  //   }]
  // }
  //
  // Note that |now| is deliberately using a double to hold an integer
  // value because |DictionaryValue| doesn't support int64 values, and
  // GCD doesn't accept fractional values.
  double now = clock_->Now().ToJavaTime();
  scoped_ptr<base::DictionaryValue> patch_dict(new base::DictionaryValue);
  patch_dict->SetDouble("requestTimeMs", now);
  scoped_ptr<base::ListValue> patch_list(new base::ListValue);
  base::DictionaryValue* patch_item = new base::DictionaryValue;
  patch_list->Append(patch_item);
  patch_item->Set("patch", patch_details.Pass());
  patch_item->SetDouble("timeMs", now);
  patch_dict->Set("patches", patch_list.Pass());

  // Stringify the message.
  std::string patch_string;
  if (!base::JSONWriter::Write(*patch_dict, &patch_string)) {
    LOG(ERROR) << "Error building GCD device state patch.";
    callback.Run(OTHER_ERROR);
    return;
  }
  DLOG(INFO) << "sending state patch: " << patch_string;

  std::string url =
      gcd_base_url_ + "/devices/" + gcd_device_id_ + "/patchState";

  // Enqueue an HTTP request to issue once an auth token is available.
  scoped_ptr<PatchStateRequest> request(new PatchStateRequest);
  request->callback = callback;
  request->url_fetcher =
      net::URLFetcher::Create(GURL(url), net::URLFetcher::POST, this);
  request->url_fetcher->SetUploadData("application/json", patch_string);
  if (url_request_context_getter_) {
    request->url_fetcher->SetRequestContext(url_request_context_getter_.get());
  }

  if (current_request_) {
    // New request will start when the current request finishes.
    DCHECK(pending_requests_.empty());
    pending_requests_.push(request.release());
  } else {
    current_request_ = request.Pass();
    StartNextRequest();
  }
}

void GcdRestClient::StartNextRequest() {
  DCHECK(current_request_);
  token_getter_->CallWithToken(
      base::Bind(&GcdRestClient::OnTokenReceived, base::Unretained(this)));
}

void GcdRestClient::OnTokenReceived(OAuthTokenGetter::Status status,
                                    const std::string& user_email,
                                    const std::string& access_token) {
  DCHECK(current_request_);
  if (status != OAuthTokenGetter::SUCCESS) {
    LOG(ERROR) << "Error getting OAuth token for GCD request: "
               << current_request_->url_fetcher->GetOriginalURL();
    if (status == OAuthTokenGetter::NETWORK_ERROR) {
      FinishCurrentRequest(NETWORK_ERROR);
    } else {
      FinishCurrentRequest(OTHER_ERROR);
    }
    return;
  }

  current_request_->url_fetcher->SetExtraRequestHeaders(
      "Authorization: Bearer " + access_token);
  current_request_->url_fetcher->Start();
}

void GcdRestClient::FinishCurrentRequest(Status result) {
  DCHECK(current_request_);
  scoped_ptr<PatchStateRequest> request = current_request_.Pass();
  if (!pending_requests_.empty()) {
    current_request_.reset(pending_requests_.front());
    pending_requests_.pop();

    // This object may be destroyed by the call to request->callback
    // below, so instead of calling StartNextRequest() at the end of
    // this method, schedule a call using a weak pointer.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&GcdRestClient::StartNextRequest,
                              weak_factory_.GetWeakPtr()));
  }

  request->callback.Run(result);
}

void GcdRestClient::OnURLFetchComplete(const net::URLFetcher* source) {
  DCHECK(current_request_);

  const GURL& request_url = current_request_->url_fetcher->GetOriginalURL();
  Status status = OTHER_ERROR;
  int response = source->GetResponseCode();
  if (response >= 200 && response < 300) {
    DLOG(INFO) << "GCD request succeeded:" << request_url;
    status = SUCCESS;
  } else if (response == 404) {
    LOG(WARNING) << "Host not found (" << response
                 << ") fetching URL: " << request_url;
    status = NO_SUCH_HOST;
  } else if (response == 0) {
    LOG(ERROR) << "Network error (" << response
               << ") fetching URL: " << request_url;
    status = NETWORK_ERROR;
  } else {
    LOG(ERROR) << "Error (" << response << ") fetching URL: " << request_url;
  }

  FinishCurrentRequest(status);
}

}  // namespace remoting
