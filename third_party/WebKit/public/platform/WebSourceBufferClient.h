// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebSourceBufferClient_h
#define WebSourceBufferClient_h

#include "WebMediaPlayer.h"
#include "WebString.h"

#include <tuple>
#include <vector>

namespace blink {

class WebSourceBufferClient {
public:
    virtual ~WebSourceBufferClient() { }

    // Complete media track info: track type, bytestream id, kind, label, language.
    typedef std::tuple<WebMediaPlayer::TrackType, WebString, WebString, WebString, WebString> MediaTrackInfo;

    // Notifies SourceBuffer that parsing of a new init segment has been completed successfully. The input parameter is a collection
    // of information about media tracks found in the new init segment. The return value is a vector of blink WebMediaPlayer track ids
    // assigned to each track of the input collection (the order of output track ids must match the input track information).
    virtual std::vector<WebMediaPlayer::TrackId> initializationSegmentReceived(const std::vector<MediaTrackInfo>& tracks) = 0;
};

} // namespace blink

#endif // WebSourceBufferClient_h
