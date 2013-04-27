// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_MEDIA_ANDROID_MEDIA_SOURCE_DELEGATE_H_
#define WEBKIT_MEDIA_ANDROID_MEDIA_SOURCE_DELEGATE_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time.h"
#include "media/base/decryptor.h"
#include "media/base/demuxer.h"
#include "media/base/pipeline_status.h"
#include "media/base/ranges.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebMediaPlayer.h"

namespace media {
class ChunkDemuxer;
class DecoderBuffer;
class DemuxerStream;
class MediaLog;
struct MediaPlayerHostMsg_ReadFromDemuxerAck_Params;
}

namespace WebKit {
class WebFrame;
class WebMediaSource;
}

namespace webkit_media {

class ProxyDecryptor;
class WebMediaPlayerProxyAndroid;

class MediaSourceDelegate : public media::DemuxerHost {
 public:
  typedef base::Callback<void(WebKit::WebMediaPlayer::NetworkState)>
      UpdateNetworkStateCB;

  MediaSourceDelegate(WebKit::WebFrame* frame,
                      WebKit::WebMediaPlayerClient* client,
                      WebMediaPlayerProxyAndroid* proxy,
                      int player_id,
                      media::MediaLog* media_log);
  virtual ~MediaSourceDelegate();

  void Initialize(scoped_ptr<WebKit::WebMediaSource> media_source,
                  const UpdateNetworkStateCB& update_network_state_cb);

  const WebKit::WebTimeRanges& Buffered();
  size_t DecodedFrameCount() const;
  size_t DroppedFrameCount() const;
  size_t AudioDecodedByteCount() const;
  size_t VideoDecodedByteCount() const;

  WebKit::WebMediaPlayer::MediaKeyException GenerateKeyRequest(
      const WebKit::WebString& key_system,
      const unsigned char* init_data,
      size_t init_data_length);
  WebKit::WebMediaPlayer::MediaKeyException AddKey(
      const WebKit::WebString& key_system,
      const unsigned char* key,
      size_t key_length,
      const unsigned char* init_data,
      size_t init_data_length,
      const WebKit::WebString& session_id);
  WebKit::WebMediaPlayer::MediaKeyException CancelKeyRequest(
      const WebKit::WebString& key_system,
      const WebKit::WebString& session_id);

  void Seek(base::TimeDelta time);

  // Called when DemuxerStreamPlayer needs to read data from ChunkDemuxer.
  // If it's the first request after the seek, |seek_done| will be true.
  void OnReadFromDemuxer(media::DemuxerStream::Type type, bool seek_done);

 private:
  // Methods inherited from DemuxerHost.
  virtual void SetTotalBytes(int64 total_bytes) OVERRIDE;
  virtual void AddBufferedByteRange(int64 start, int64 end) OVERRIDE;
  virtual void AddBufferedTimeRange(base::TimeDelta start,
                                    base::TimeDelta end) OVERRIDE;
  virtual void SetDuration(base::TimeDelta duration) OVERRIDE;
  virtual void OnDemuxerError(media::PipelineStatus status) OVERRIDE;

  // Callbacks for ChunkDemuxer & Decryptor.
  void OnDemuxerInitDone(media::PipelineStatus status);
  void OnDemuxerOpened();
  void OnKeyAdded(const std::string& key_system, const std::string& session_id);
  void OnKeyError(const std::string& key_system,
                  const std::string& session_id,
                  media::Decryptor::KeyError error_code,
                  int system_code);
  void OnKeyMessage(const std::string& key_system,
                    const std::string& session_id,
                    const std::string& message,
                    const std::string& default_url);
  void OnNeedKey(const std::string& key_system,
                 const std::string& type,
                 const std::string& session_id,
                 scoped_ptr<uint8[]> init_data,
                 int init_data_size);
  void OnDecryptorReady(media::Decryptor*);

  // Reads an access unit from the demuxer stream |stream| and stores it in
  // the |index|th access unit in |params|.
  void ReadFromDemuxerStream(
      media::DemuxerStream* stream,
      media::MediaPlayerHostMsg_ReadFromDemuxerAck_Params* params,
      size_t index);
  void OnBufferReady(
      media::DemuxerStream* stream,
      media::MediaPlayerHostMsg_ReadFromDemuxerAck_Params* params,
      size_t index,
      media::DemuxerStream::Status status,
      const scoped_refptr<media::DecoderBuffer>& buffer);

  void NotifyDemuxerReady(const std::string& key_system);

  base::WeakPtrFactory<MediaSourceDelegate> weak_this_;

  WebKit::WebMediaPlayerClient* const client_;
  WebMediaPlayerProxyAndroid* proxy_;
  int player_id_;

  scoped_refptr<media::MediaLog> media_log_;
  UpdateNetworkStateCB update_network_state_cb_;

  scoped_ptr<media::ChunkDemuxer> chunk_demuxer_;
  scoped_ptr<WebKit::WebMediaSource> media_source_;

  media::PipelineStatistics statistics_;
  media::Ranges<base::TimeDelta> buffered_time_ranges_;
  // Keep a list of buffered time ranges.
  WebKit::WebTimeRanges buffered_web_time_ranges_;

  // The decryptor that manages decryption keys and decrypts encrypted frames.
  scoped_ptr<ProxyDecryptor> decryptor_;

  // The currently selected key system. Empty string means that no key system
  // has been selected.
  WebKit::WebString current_key_system_;

  // Temporary for EME v0.1. In the future the init data type should be passed
  // through GenerateKeyRequest() directly from WebKit.
  std::string init_data_type_;

  scoped_ptr<media::MediaPlayerHostMsg_ReadFromDemuxerAck_Params> audio_params_;
  scoped_ptr<media::MediaPlayerHostMsg_ReadFromDemuxerAck_Params> video_params_;

  bool seeking_;

  DISALLOW_COPY_AND_ASSIGN(MediaSourceDelegate);
};

}  // namespace webkit_media
#endif  // WEBKIT_MEDIA_ANDROID_MEDIA_SOURCE_DELEGATE_H_

