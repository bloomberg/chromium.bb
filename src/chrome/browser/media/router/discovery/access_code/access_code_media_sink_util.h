// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_ACCESS_CODE_ACCESS_CODE_MEDIA_SINK_UTIL_H_
#define CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_ACCESS_CODE_ACCESS_CODE_MEDIA_SINK_UTIL_H_

#include "chrome/browser/media/router/discovery/access_code/discovery_resources.pb.h"
#include "chrome/browser/media/router/discovery/mdns/media_sink_util.h"
#include "components/media_router/common/discovery/media_sink_internal.h"

namespace media_router {

using DiscoveryDevice = chrome_browser_media::proto::DiscoveryDevice;
using NetworkInfo = chrome_browser_media::proto::NetworkInfo;

// Creates a MediaSinkInternal from |discovery_device|. |cast_sink| is only
// valid if the returned result is |kOk|.
std::pair<absl::optional<MediaSinkInternal>, CreateCastMediaSinkResult>
CreateAccessCodeMediaSink(const DiscoveryDevice& discovery_device);

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_ACCESS_CODE_ACCESS_CODE_MEDIA_SINK_UTIL_H_
