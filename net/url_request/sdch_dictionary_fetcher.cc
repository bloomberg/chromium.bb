// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/sdch_dictionary_fetcher.h"

#include <stdint.h>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/profiler/scoped_tracker.h"
#include "base/thread_task_runner_handle.h"
#include "net/base/io_buffer.h"
#include "net/base/load_flags.h"
#include "net/base/net_log.h"
#include "net/base/sdch_net_log_params.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_status.h"
#include "net/url_request/url_request_throttler_manager.h"

namespace net {

namespace {

const int kBufferSize = 4096;

// Map the bytes_read result from a read attempt and a URLRequest's
// status into a single net return value.
int GetReadResult(int bytes_read, const URLRequest* request) {
  int rv = request->status().error();
  if (request->status().is_success() && bytes_read < 0) {
    rv = ERR_FAILED;
    request->net_log().AddEventWithNetErrorCode(
        NetLog::TYPE_SDCH_DICTIONARY_FETCH_IMPLIED_ERROR, rv);
  }

  if (rv == OK)
    rv = bytes_read;

  return rv;
}

}  // namespace

SdchDictionaryFetcher::SdchDictionaryFetcher(
    URLRequestContext* context,
    const OnDictionaryFetchedCallback& callback)
    : next_state_(STATE_NONE),
      in_loop_(false),
      context_(context),
      dictionary_fetched_callback_(callback),
      weak_factory_(this) {
  DCHECK(CalledOnValidThread());
  DCHECK(context);
}

SdchDictionaryFetcher::~SdchDictionaryFetcher() {
  DCHECK(CalledOnValidThread());
}

void SdchDictionaryFetcher::Schedule(const GURL& dictionary_url) {
  DCHECK(CalledOnValidThread());

  // Avoid pushing duplicate copy onto queue. We may fetch this url again later
  // and get a different dictionary, but there is no reason to have it in the
  // queue twice at one time.
  if ((!fetch_queue_.empty() && fetch_queue_.back() == dictionary_url) ||
      attempted_load_.find(dictionary_url) != attempted_load_.end()) {
    // TODO(rdsmith): log this error to the net log of the URLRequest
    // initiating this fetch, once URLRequest will be passed here.
    SdchManager::SdchErrorRecovery(
        SDCH_DICTIONARY_PREVIOUSLY_SCHEDULED_TO_DOWNLOAD);
    return;
  }

  attempted_load_.insert(dictionary_url);
  fetch_queue_.push(dictionary_url);

  // If the loop is already processing, it'll pick up the above in the
  // normal course of events.
  if (next_state_ != STATE_NONE)
    return;

  next_state_ = STATE_SEND_REQUEST;

  // There are no callbacks to user code from the dictionary fetcher,
  // and Schedule() is only called from user code, so this call to DoLoop()
  // does not require an |if (in_loop_) return;| guard.
  DoLoop(OK);
}

void SdchDictionaryFetcher::Cancel() {
  DCHECK(CalledOnValidThread());

  next_state_ = STATE_NONE;

  while (!fetch_queue_.empty())
    fetch_queue_.pop();
  attempted_load_.clear();
  weak_factory_.InvalidateWeakPtrs();
  current_request_.reset(NULL);
  buffer_ = NULL;
  dictionary_.clear();
}

void SdchDictionaryFetcher::OnResponseStarted(URLRequest* request) {
  // TODO(vadimt): Remove ScopedTracker below once crbug.com/423948 is fixed.
  tracked_objects::ScopedTracker tracking_profile(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "423948 SdchDictionaryFetcher::OnResponseStarted"));

  DCHECK(CalledOnValidThread());
  DCHECK_EQ(request, current_request_.get());
  DCHECK_EQ(next_state_, STATE_SEND_REQUEST_COMPLETE);
  DCHECK(!in_loop_);

  DoLoop(request->status().error());
}

void SdchDictionaryFetcher::OnReadCompleted(URLRequest* request,
                                            int bytes_read) {
  // TODO(vadimt): Remove ScopedTracker below once crbug.com/423948 is fixed.
  tracked_objects::ScopedTracker tracking_profile(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "423948 SdchDictionaryFetcher::OnReadCompleted"));

  DCHECK(CalledOnValidThread());
  DCHECK_EQ(request, current_request_.get());
  DCHECK_EQ(next_state_, STATE_READ_BODY_COMPLETE);
  DCHECK(!in_loop_);

  DoLoop(GetReadResult(bytes_read, current_request_.get()));
}

int SdchDictionaryFetcher::DoLoop(int rv) {
  DCHECK(!in_loop_);
  base::AutoReset<bool> auto_reset_in_loop(&in_loop_, true);

  do {
    State state = next_state_;
    next_state_ = STATE_NONE;
    switch (state) {
      case STATE_SEND_REQUEST:
        rv = DoSendRequest(rv);
        break;
      case STATE_SEND_REQUEST_COMPLETE:
        rv = DoSendRequestComplete(rv);
        break;
      case STATE_READ_BODY:
        rv = DoReadBody(rv);
        break;
      case STATE_READ_BODY_COMPLETE:
        rv = DoReadBodyComplete(rv);
        break;
      case STATE_REQUEST_COMPLETE:
        rv = DoCompleteRequest(rv);
        break;
      case STATE_NONE:
        NOTREACHED();
    }
  } while (rv != ERR_IO_PENDING && next_state_ != STATE_NONE);

  return rv;
}

int SdchDictionaryFetcher::DoSendRequest(int rv) {
  DCHECK(CalledOnValidThread());

  // |rv| is ignored, as the result from the previous request doesn't
  // affect the next request.

  if (fetch_queue_.empty() || current_request_.get()) {
    next_state_ = STATE_NONE;
    return OK;
  }

  next_state_ = STATE_SEND_REQUEST_COMPLETE;

  current_request_ =
      context_->CreateRequest(fetch_queue_.front(), IDLE, this, NULL);
  current_request_->SetLoadFlags(LOAD_DO_NOT_SEND_COOKIES |
                                 LOAD_DO_NOT_SAVE_COOKIES);
  buffer_ = new IOBuffer(kBufferSize);
  fetch_queue_.pop();

  current_request_->Start();
  current_request_->net_log().AddEvent(NetLog::TYPE_SDCH_DICTIONARY_FETCH);

  return ERR_IO_PENDING;
}

int SdchDictionaryFetcher::DoSendRequestComplete(int rv) {
  DCHECK(CalledOnValidThread());

  // If there's been an error, abort the current request.
  if (rv != OK) {
    current_request_.reset();
    buffer_ = NULL;
    next_state_ = STATE_SEND_REQUEST;

    return OK;
  }

  next_state_ = STATE_READ_BODY;
  return OK;
}

int SdchDictionaryFetcher::DoReadBody(int rv) {
  DCHECK(CalledOnValidThread());

  // If there's been an error, abort the current request.
  if (rv != OK) {
    current_request_.reset();
    buffer_ = NULL;
    next_state_ = STATE_SEND_REQUEST;

    return OK;
  }

  next_state_ = STATE_READ_BODY_COMPLETE;
  int bytes_read = 0;
  current_request_->Read(buffer_.get(), kBufferSize, &bytes_read);
  if (current_request_->status().is_io_pending())
    return ERR_IO_PENDING;

  return GetReadResult(bytes_read, current_request_.get());
}

int SdchDictionaryFetcher::DoReadBodyComplete(int rv) {
  DCHECK(CalledOnValidThread());

  // An error; abort the current request.
  if (rv < 0) {
    current_request_.reset();
    buffer_ = NULL;
    next_state_ = STATE_SEND_REQUEST;
    return OK;
  }

  DCHECK(current_request_->status().is_success());

  // Data; append to the dictionary and look for more data.
  if (rv > 0) {
    dictionary_.append(buffer_->data(), rv);
    next_state_ = STATE_READ_BODY;
    return OK;
  }

  // End of file; complete the request.
  next_state_ = STATE_REQUEST_COMPLETE;
  return OK;
}

int SdchDictionaryFetcher::DoCompleteRequest(int rv) {
  DCHECK(CalledOnValidThread());

  // DoReadBodyComplete() only transitions to this state
  // on success.
  DCHECK_EQ(OK, rv);

  dictionary_fetched_callback_.Run(dictionary_, current_request_->url(),
                                   current_request_->net_log());
  current_request_.reset();
  buffer_ = NULL;
  dictionary_.clear();

  next_state_ = STATE_SEND_REQUEST;

  return OK;
}

}  // namespace net
