// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_MEDIA_FILTER_HELPERS_H_
#define WEBKIT_MEDIA_FILTER_HELPERS_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_vector.h"
// TODO(fgalligan): Remove the dependency on FFmpeg.
#include "media/filters/ffmpeg_demuxer.h"

namespace base {
class MessageLoopProxy;
}

namespace media {
class AudioDecoder;
class ChunkDemuxer;
class DataSource;
class FilterCollection;
}

namespace webkit_media {

// Creates and adds the default set of audio decoders to |audio_decoders|.
void AddDefaultAudioDecoders(
    const scoped_refptr<base::MessageLoopProxy>& message_loop,
    ScopedVector<media::AudioDecoder>* audio_decoders);

// Builds the required filters for handling media source URLs, adds them to
// |filter_collection|.
void BuildMediaSourceCollection(
    const scoped_refptr<media::ChunkDemuxer>& demuxer,
    const scoped_refptr<base::MessageLoopProxy>& message_loop,
    media::FilterCollection* filter_collection);

// Builds the required filters for handling regular URLs and adds them to
// |filter_collection| and fills |video_decoder| returning true if successful.
void BuildDefaultCollection(
    const scoped_refptr<media::DataSource>& data_source,
    const scoped_refptr<base::MessageLoopProxy>& message_loop,
    media::FilterCollection* filter_collection,
    const media::FFmpegNeedKeyCB& need_key_cb);

}  // webkit_media

#endif  // WEBKIT_MEDIA_FILTER_HELPERS_H_
