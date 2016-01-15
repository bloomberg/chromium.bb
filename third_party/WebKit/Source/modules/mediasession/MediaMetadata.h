// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MediaMetadata_h
#define MediaMetadata_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "modules/ModulesExport.h"
#include "platform/heap/Handle.h"
#include "public/platform/modules/mediasession/WebMediaMetadata.h"

namespace blink {

class MediaMetadataInit;

// Implementation of MediaMetadata interface from the Media Session API.
class MODULES_EXPORT MediaMetadata final
    : public GarbageCollectedFinalized<MediaMetadata>
    , public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();
public:
    static MediaMetadata* create(const MediaMetadataInit&);

    String title() const;
    String artist() const;
    String album() const;

    WebMediaMetadata* data() { return &m_data; }

    DEFINE_INLINE_TRACE() { }

private:
    MediaMetadata(const MediaMetadataInit&);

    WebMediaMetadata m_data;
};

} // namespace blink

#endif // MediaMetadata_h
