// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/mediasession/MediaMetadata.h"

#include "modules/mediasession/MediaMetadataInit.h"
#include "wtf/text/WTFString.h"

namespace blink {

// static
MediaMetadata* MediaMetadata::create(const MediaMetadataInit& metadata)
{
    return new MediaMetadata(metadata);
}

MediaMetadata::MediaMetadata(const MediaMetadataInit& metadata)
{
    m_data.title = metadata.title();
    m_data.artist = metadata.artist();
    m_data.album = metadata.album();
}

String MediaMetadata::title() const
{
    return m_data.title;
}

String MediaMetadata::artist() const
{
    return m_data.artist;
}

String MediaMetadata::album() const
{
    return m_data.album;
}

} // namespace blink
