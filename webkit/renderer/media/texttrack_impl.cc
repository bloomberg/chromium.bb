// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/renderer/media/texttrack_impl.h"

#include "third_party/WebKit/public/web/WebInbandTextTrackClient.h"
#include "webkit/renderer/media/webinbandtexttrack_impl.h"

namespace webkit_media {

TextTrackImpl::TextTrackImpl(WebInbandTextTrackImpl* text_track)
    : text_track_(text_track) {
  // This impl object assumes ownership of the text_track object.
}

TextTrackImpl::~TextTrackImpl() {
}

void TextTrackImpl::addWebVTTCue(const base::TimeDelta& start,
                                 const base::TimeDelta& end,
                                 const std::string& id,
                                 const std::string& content,
                                 const std::string& settings) {
  if (WebKit::WebInbandTextTrackClient* client = text_track_->client())
    client->addWebVTTCue(start.InSecondsF(),
                         end.InSecondsF(),
                         WebKit::WebString::fromUTF8(id),
                         WebKit::WebString::fromUTF8(content),
                         WebKit::WebString::fromUTF8(settings));
}

}  // namespace webkit_media
