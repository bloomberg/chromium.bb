// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/media/media_resource_loader_bridge_factory.h"

#include "base/format_macros.h"
#include "base/string_util.h"
#include "base/stringprintf.h"

namespace {

// A constant for an unknown position.
const int64 kPositionNotSpecified = -1;

}  // namespace

namespace webkit_glue {

MediaResourceLoaderBridgeFactory::MediaResourceLoaderBridgeFactory(
    const GURL& referrer,
    const std::string& frame_origin,
    const std::string& main_frame_origin,
    int origin_pid,
    int appcache_host_id,
    int32 routing_id)
    : referrer_(referrer),
      frame_origin_(frame_origin),
      main_frame_origin_(main_frame_origin),
      origin_pid_(origin_pid),
      appcache_host_id_(appcache_host_id),
      routing_id_(routing_id) {
}

MediaResourceLoaderBridgeFactory::~MediaResourceLoaderBridgeFactory() {}

ResourceLoaderBridge* MediaResourceLoaderBridgeFactory::CreateBridge(
        const GURL& url,
        int load_flags,
        int64 first_byte_position,
        int64 last_byte_position) {
  webkit_glue::ResourceLoaderBridge::RequestInfo request_info;
  request_info.method = "GET";
  request_info.url = url;
  request_info.first_party_for_cookies = url;
  request_info.referrer = referrer_;
  request_info.frame_origin = frame_origin_;
  request_info.main_frame_origin = main_frame_origin_;
  request_info.headers = GenerateHeaders(first_byte_position,
                                         last_byte_position);
  request_info.load_flags = load_flags;
  request_info.requestor_pid = origin_pid_;
  request_info.request_type = ResourceType::MEDIA;
  request_info.appcache_host_id = appcache_host_id_;
  request_info.routing_id = routing_id_;
  return webkit_glue::ResourceLoaderBridge::Create(request_info);
}

MediaResourceLoaderBridgeFactory::MediaResourceLoaderBridgeFactory() {}

// static
const std::string MediaResourceLoaderBridgeFactory::GenerateHeaders (
    int64 first_byte_position, int64 last_byte_position) {
  // Construct the range header.
  std::string header;
  if (first_byte_position > kPositionNotSpecified &&
      last_byte_position > kPositionNotSpecified) {
    if (first_byte_position <= last_byte_position) {
      header = base::StringPrintf("Range: bytes=%" PRId64 "-%" PRId64,
                                  first_byte_position,
                                  last_byte_position);
    }
  } else if (first_byte_position > kPositionNotSpecified) {
    header = base::StringPrintf("Range: bytes=%" PRId64 "-",
                                first_byte_position);
  } else if (last_byte_position > kPositionNotSpecified) {
    NOTIMPLEMENTED() << "Suffix range not implemented";
  }
  return header;
}

}  // namespace webkit_glue
