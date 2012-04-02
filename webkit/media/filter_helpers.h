// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_MEDIA_FILTER_HELPERS_H_
#define WEBKIT_MEDIA_FILTER_HELPERS_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"

namespace media {
class ChunkDemuxerClient;
class DataSource;
class FFmpegVideoDecoder;
class FilterCollection;
class MessageLoopFactory;
}

namespace WebKit {
class WebURL;
}

namespace webkit_media {

class MediaStreamClient;

// Builds the required filters for handling media stream URLs and adds them to
// |filter_collection| returning true if successful.
//
// |filter_collection| is not modified if this method returns false.
bool BuildMediaStreamCollection(const WebKit::WebURL& url,
                                MediaStreamClient* client,
                                media::MessageLoopFactory* message_loop_factory,
                                media::FilterCollection* filter_collection);

// Builds the required filters for handling media source URLs, adds them to
// |filter_collection| and fills |video_decoder| returning true if successful.
//
// |filter_collection| is not modified if this method returns false.
bool BuildMediaSourceCollection(
    const WebKit::WebURL& url,
    const WebKit::WebURL& media_source_url,
    media::ChunkDemuxerClient* client,
    media::MessageLoopFactory* message_loop_factory,
    media::FilterCollection* filter_collection,
    scoped_refptr<media::FFmpegVideoDecoder>* video_decoder);

// Builds the required filters for handling regular URLs and adds them to
// |filter_collection| and fills |video_decoder| returning true if successful.
//
// |local_source| refers to whether the data being fetched requires network
// access.
//
// TODO(scherkus): a data source should be able to tell us this.
void BuildDefaultCollection(
    const scoped_refptr<media::DataSource>& data_source,
    bool local_source,
    media::MessageLoopFactory* message_loop_factory,
    media::FilterCollection* filter_collection,
    scoped_refptr<media::FFmpegVideoDecoder>* video_decoder);

}  // webkit_media

#endif  // WEBKIT_MEDIA_FILTER_HELPERS_H_
