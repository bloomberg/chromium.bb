// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>

#include "chrome/browser/media/router/media_router.h"
#include "chrome/test/media_router/test_media_sinks_observer.h"

namespace media_router {

TestMediaSinksObserver::TestMediaSinksObserver(MediaRouter* router,
                                               const MediaSource& source,
                                               const url::Origin& origin)
    : MediaSinksObserver(router, source, origin) {}

TestMediaSinksObserver::~TestMediaSinksObserver() {
}

void TestMediaSinksObserver::OnSinksReceived(
    const std::vector<MediaSink>& result) {
  sink_map.clear();
  for (const MediaSink& sink : result) {
    sink_map.insert(std::make_pair(sink.name(), sink));
  }
}

}  // namespace media_router
