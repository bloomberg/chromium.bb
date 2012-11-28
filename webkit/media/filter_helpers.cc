// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/media/filter_helpers.h"

#include "base/bind.h"
#include "media/base/filter_collection.h"
#include "media/filters/decrypting_audio_decoder.h"
#include "media/filters/decrypting_video_decoder.h"
#include "media/filters/chunk_demuxer.h"
#include "media/filters/dummy_demuxer.h"
#include "media/filters/ffmpeg_audio_decoder.h"
#include "media/filters/ffmpeg_demuxer.h"
#include "media/filters/ffmpeg_video_decoder.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebURL.h"
#include "webkit/media/crypto/proxy_decryptor.h"
#include "webkit/media/media_stream_client.h"

namespace webkit_media {

// Constructs and adds the default audio/video decoders to |filter_collection|.
// Note that decoders in the |filter_collection| are ordered. The first
// audio/video decoder in the |filter_collection| that supports the input
// audio/video stream will be selected as the audio/video decoder in the media
// pipeline. This is done by trying to initialize the decoder with the input
// stream. Some decoder may only accept certain types of streams. For example,
// DecryptingVideoDecoder only supports encrypted video stream.
static void AddDefaultDecodersToCollection(
    const scoped_refptr<base::MessageLoopProxy>& message_loop,
    media::FilterCollection* filter_collection,
    ProxyDecryptor* proxy_decryptor) {
  scoped_refptr<media::FFmpegAudioDecoder> ffmpeg_audio_decoder =
      new media::FFmpegAudioDecoder(message_loop);

  scoped_refptr<media::DecryptingAudioDecoder> decrypting_audio_decoder =
      new media::DecryptingAudioDecoder(
          message_loop,
          base::Bind(&ProxyDecryptor::RequestDecryptorNotification,
                     base::Unretained(proxy_decryptor)));

  filter_collection->GetAudioDecoders()->push_back(ffmpeg_audio_decoder);
  filter_collection->GetAudioDecoders()->push_back(decrypting_audio_decoder);

  scoped_refptr<media::DecryptingVideoDecoder> decrypting_video_decoder =
      new media::DecryptingVideoDecoder(
          message_loop,
          base::Bind(&ProxyDecryptor::RequestDecryptorNotification,
                     base::Unretained(proxy_decryptor)));

  scoped_refptr<media::FFmpegVideoDecoder> ffmpeg_video_decoder =
      new media::FFmpegVideoDecoder(message_loop, proxy_decryptor);

  // TODO(xhwang): Ideally we should have decrypting video decoder after
  // regular video decoder since in the real world most videos are not
  // encrypted. For now FFmpegVideoDecoder can also do decryption
  // (decrypt-only), and we perfer DecryptingVideoDecoder (decrypt-and-decode)
  // to FFmpegVideoDecoder. Fix this order when we move decryption out of
  // FFmpegVideoDecoder.
  filter_collection->GetVideoDecoders()->push_back(decrypting_video_decoder);
  filter_collection->GetVideoDecoders()->push_back(ffmpeg_video_decoder);
}

bool BuildMediaStreamCollection(
    const WebKit::WebURL& url,
    MediaStreamClient* client,
    const scoped_refptr<base::MessageLoopProxy>& message_loop,
    media::FilterCollection* filter_collection) {
  if (!client)
    return false;

  scoped_refptr<media::VideoDecoder> video_decoder = client->GetVideoDecoder(
      url, message_loop);
  if (!video_decoder)
    return false;

  // Remove all other decoders and just use the MediaStream one.
  // NOTE: http://crbug.com/110800 is about replacing this ad-hockery with
  // something more designed.
  filter_collection->GetVideoDecoders()->clear();
  filter_collection->GetVideoDecoders()->push_back(video_decoder);

  filter_collection->SetDemuxer(new media::DummyDemuxer(true, false));

  return true;
}

void BuildMediaSourceCollection(
    const scoped_refptr<media::ChunkDemuxer>& demuxer,
    const scoped_refptr<base::MessageLoopProxy>& message_loop,
    media::FilterCollection* filter_collection,
    ProxyDecryptor* proxy_decryptor) {
  DCHECK(demuxer);
  filter_collection->SetDemuxer(demuxer);

  // Remove GPUVideoDecoder until it supports codec config changes.
  // TODO(acolwell): Remove this once http://crbug.com/151045 is fixed.
  DCHECK_LE(filter_collection->GetVideoDecoders()->size(), 1u);
  filter_collection->GetVideoDecoders()->clear();

  AddDefaultDecodersToCollection(message_loop, filter_collection,
                                 proxy_decryptor);
}

void BuildDefaultCollection(
    const scoped_refptr<media::DataSource>& data_source,
    const scoped_refptr<base::MessageLoopProxy>& message_loop,
    media::FilterCollection* filter_collection,
    ProxyDecryptor* proxy_decryptor) {
  filter_collection->SetDemuxer(new media::FFmpegDemuxer(
      message_loop, data_source));

  AddDefaultDecodersToCollection(message_loop, filter_collection,
                                 proxy_decryptor);
}

}  // webkit_media
