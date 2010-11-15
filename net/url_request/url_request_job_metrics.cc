// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_job_metrics.h"

#include "base/basictypes.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"

using base::TimeDelta;

URLRequestJobMetrics::URLRequestJobMetrics()
    : total_bytes_read_(0),
      number_of_read_IO_(0),
      success_(false) {
}

URLRequestJobMetrics::~URLRequestJobMetrics() {}

void URLRequestJobMetrics::AppendText(std::wstring* text) {
  if (!text)
    return;

  text->append(L"job url = ");
  text->append(UTF8ToWide(original_url_->spec()));

  if (url_.get()) {
    text->append(L"; redirected url = ");
    text->append(UTF8ToWide(url_->spec()));
  }

  TimeDelta elapsed = end_time_ - start_time_;
  base::StringAppendF(text,
      L"; total bytes read = %ld; read calls = %d; time = %lld ms;",
      static_cast<long>(total_bytes_read_),
      number_of_read_IO_, elapsed.InMilliseconds());

  if (success_) {
    text->append(L" success.");
  } else {
    text->append(L" fail.");
  }
}
