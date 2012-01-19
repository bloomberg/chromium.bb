// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/media/filter_helpers.h"

#include "media/base/filter_collection.h"
#include "media/base/message_loop_factory.h"
#include "media/filters/chunk_demuxer_factory.h"
#include "media/filters/dummy_demuxer_factory.h"
#include "media/filters/ffmpeg_audio_decoder.h"
#include "media/filters/ffmpeg_demuxer_factory.h"
#include "media/filters/ffmpeg_video_decoder.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebURL.h"
#include "webkit/media/media_stream_client.h"

namespace webkit_media {

// Constructs and adds the default audio/video decoders to |filter_collection|.
static void AddDefaultDecodersToCollection(
    media::MessageLoopFactory* message_loop_factory,
    media::FilterCollection* filter_collection) {
  filter_collection->AddAudioDecoder(new media::FFmpegAudioDecoder(
      message_loop_factory->GetMessageLoop("AudioDecoderThread")));
  filter_collection->AddVideoDecoder(new media::FFmpegVideoDecoder(
      message_loop_factory->GetMessageLoop("VideoDecoderThread")));
}

bool BuildMediaStreamCollection(const WebKit::WebURL& url,
                                MediaStreamClient* client,
                                media::MessageLoopFactory* message_loop_factory,
                                media::FilterCollection* filter_collection) {
  if (!client)
    return false;

  scoped_refptr<media::VideoDecoder> video_decoder = client->GetVideoDecoder(
      url, message_loop_factory);
  if (!video_decoder)
    return false;

  filter_collection->AddVideoDecoder(video_decoder);

  // TODO(vrk/wjia): Setting true for local_source is under the assumption
  // that the MediaStream represents a local webcam. This will need to
  // change in the future when GetVideoDecoder is no longer hardcoded to
  // only return CaptureVideoDecoders.
  scoped_ptr<media::DemuxerFactory> demuxer_factory(
      new media::DummyDemuxerFactory(true, false, true));
  filter_collection->SetDemuxerFactory(demuxer_factory.Pass());

  return true;
}

bool BuildMediaSourceCollection(const WebKit::WebURL& url,
                                const WebKit::WebURL& media_source_url,
                                media::ChunkDemuxerClient* client,
                                media::MessageLoopFactory* message_loop_factory,
                                media::FilterCollection* filter_collection) {
  if (media_source_url.isEmpty() || url != media_source_url)
    return false;

  scoped_ptr<media::DemuxerFactory> demuxer_factory(
      new media::ChunkDemuxerFactory(client));
  filter_collection->SetDemuxerFactory(demuxer_factory.Pass());

  AddDefaultDecodersToCollection(message_loop_factory, filter_collection);
  return true;
}

void BuildDefaultCollection(const scoped_refptr<media::DataSource>& data_source,
                            media::MessageLoopFactory* message_loop_factory,
                            media::FilterCollection* filter_collection) {
  scoped_ptr<media::DemuxerFactory> demuxer_factory(
      new media::FFmpegDemuxerFactory(
          data_source, message_loop_factory->GetMessageLoop("PipelineThread")));
  filter_collection->SetDemuxerFactory(demuxer_factory.Pass());

  AddDefaultDecodersToCollection(message_loop_factory, filter_collection);
}

}  // webkit_media
