// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/media/filter_helpers.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "media/base/filter_collection.h"
#include "media/base/media_switches.h"
#include "media/filters/chunk_demuxer.h"
#include "media/filters/ffmpeg_audio_decoder.h"
#include "media/filters/ffmpeg_demuxer.h"
#include "media/filters/ffmpeg_video_decoder.h"
#include "media/filters/opus_audio_decoder.h"
#include "media/filters/vpx_video_decoder.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebURL.h"

namespace webkit_media {

void AddDefaultAudioDecoders(
    const scoped_refptr<base::MessageLoopProxy>& message_loop,
    ScopedVector<media::AudioDecoder>* audio_decoders) {
  audio_decoders->push_back(new media::FFmpegAudioDecoder(message_loop));

  const CommandLine* cmd_line = CommandLine::ForCurrentProcess();
  if (cmd_line->HasSwitch(switches::kEnableOpusPlayback)) {
    audio_decoders->push_back(new media::OpusAudioDecoder(message_loop));
  }
}

// Constructs and adds the default video decoders to |filter_collection|.
//
// Note that decoders in the |filter_collection| are initialized in order.
static void AddDefaultDecodersToCollection(
    const scoped_refptr<base::MessageLoopProxy>& message_loop,
    media::FilterCollection* filter_collection) {

  scoped_refptr<media::FFmpegVideoDecoder> ffmpeg_video_decoder =
      new media::FFmpegVideoDecoder(message_loop);
  filter_collection->GetVideoDecoders()->push_back(ffmpeg_video_decoder);

  // TODO(phajdan.jr): Remove ifdefs when libvpx with vp9 support is released
  // (http://crbug.com/174287) .
#if !defined(MEDIA_DISABLE_LIBVPX)
  const CommandLine* cmd_line = CommandLine::ForCurrentProcess();
  if (cmd_line->HasSwitch(switches::kEnableVp9Playback)) {
    scoped_refptr<media::VpxVideoDecoder> vpx_video_decoder =
        new media::VpxVideoDecoder(message_loop);
    filter_collection->GetVideoDecoders()->push_back(vpx_video_decoder);
  }
#endif  // !defined(MEDIA_DISABLE_LIBVPX)
}

void BuildMediaSourceCollection(
    const scoped_refptr<media::ChunkDemuxer>& demuxer,
    const scoped_refptr<base::MessageLoopProxy>& message_loop,
    media::FilterCollection* filter_collection) {
  DCHECK(demuxer);
  filter_collection->SetDemuxer(demuxer);

  // Remove GPUVideoDecoder until it supports codec config changes.
  // TODO(acolwell): Remove this once http://crbug.com/151045 is fixed.
  DCHECK_LE(filter_collection->GetVideoDecoders()->size(), 1u);
  filter_collection->GetVideoDecoders()->clear();

  AddDefaultDecodersToCollection(message_loop, filter_collection);
}

void BuildDefaultCollection(
    const scoped_refptr<media::DataSource>& data_source,
    const scoped_refptr<base::MessageLoopProxy>& message_loop,
    media::FilterCollection* filter_collection,
    const media::FFmpegNeedKeyCB& need_key_cb) {
  filter_collection->SetDemuxer(new media::FFmpegDemuxer(
      message_loop, data_source, need_key_cb));

  AddDefaultDecodersToCollection(message_loop, filter_collection);
}

}  // webkit_media
