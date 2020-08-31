// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/timezone/timezone_provider.h"

#include <algorithm>
#include <iterator>
#include <utility>

#include "base/bind.h"
#include "base/check.h"
#include "base/memory/ptr_util.h"
#include "chromeos/geolocation/geoposition.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace chromeos {

TimeZoneProvider::TimeZoneProvider(
    scoped_refptr<network::SharedURLLoaderFactory> factory,
    const GURL& url)
    : shared_url_loader_factory_(std::move(factory)), url_(url) {}

TimeZoneProvider::~TimeZoneProvider() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void TimeZoneProvider::RequestTimezone(
    const Geoposition& position,
    base::TimeDelta timeout,
    TimeZoneRequest::TimeZoneResponseCallback callback) {
  TimeZoneRequest* request(
      new TimeZoneRequest(shared_url_loader_factory_, url_, position, timeout));
  requests_.push_back(base::WrapUnique(request));

  // TimeZoneProvider owns all requests. It is safe to pass unretained "this"
  // because destruction of TimeZoneProvider cancels all requests.
  TimeZoneRequest::TimeZoneResponseCallback callback_tmp =
      base::BindOnce(&TimeZoneProvider::OnTimezoneResponse,
                     base::Unretained(this), request, std::move(callback));
  request->MakeRequest(std::move(callback_tmp));
}

void TimeZoneProvider::OnTimezoneResponse(
    TimeZoneRequest* request,
    TimeZoneRequest::TimeZoneResponseCallback callback,
    std::unique_ptr<TimeZoneResponseData> timezone,
    bool server_error) {
  std::vector<std::unique_ptr<TimeZoneRequest>>::iterator position =
      std::find_if(requests_.begin(), requests_.end(),
                   [request](const std::unique_ptr<TimeZoneRequest>& req) {
                     return req.get() == request;
                   });
  DCHECK(position != requests_.end());
  if (position != requests_.end()) {
    std::swap(*position, *requests_.rbegin());
    requests_.resize(requests_.size() - 1);
  }

  std::move(callback).Run(std::move(timezone), server_error);
}

}  // namespace chromeos
