// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_RENDERER_MEDIA_ANDROID_WEBMEDIAPLAYER_PROXY_ANDROID_H_
#define WEBKIT_RENDERER_MEDIA_ANDROID_WEBMEDIAPLAYER_PROXY_ANDROID_H_

#include <string>

#include "base/time.h"
#include "googleurl/src/gurl.h"
#include "media/base/android/demuxer_stream_player_params.h"
#include "media/base/android/media_player_android.h"

namespace webkit_media {

// An interface to facilitate the IPC between the browser side
// android::MediaPlayer and the WebMediaPlayerImplAndroid in the renderer
// process.
// Implementation is provided by content::WebMediaPlayerProxyImplAndroid.
class WebMediaPlayerProxyAndroid {
 public:
  virtual ~WebMediaPlayerProxyAndroid();

  // Initialize a MediaPlayerAndroid object in browser process
  virtual void Initialize(
      int player_id,
      const GURL& url,
      media::MediaPlayerAndroid::SourceType source_type,
      const GURL& first_party_for_cookies) = 0;

  // Start the player.
  virtual void Start(int player_id) = 0;

  // Pause the player.
  virtual void Pause(int player_id) = 0;

  // Perform seek on the player.
  virtual void Seek(int player_id, base::TimeDelta time) = 0;

  // Release resources for the player.
  virtual void ReleaseResources(int player_id) = 0;

  // Destroy the player in the browser process
  virtual void DestroyPlayer(int player_id) = 0;

  // Request the player to enter fullscreen.
  virtual void EnterFullscreen(int player_id) = 0;

  // Request the player to exit fullscreen.
  virtual void ExitFullscreen(int player_id) = 0;

#if defined(GOOGLE_TV)
  // Request an external surface for out-of-band compositing.
  virtual void RequestExternalSurface(int player_id,
                                      const gfx::RectF& geometry) = 0;
#endif

  // Inform the media source player that the demuxer is ready.
  virtual void DemuxerReady(
      int player_id,
      const media::MediaPlayerHostMsg_DemuxerReady_Params&) = 0;

  // Return the data to the media source player when data is ready.
  virtual void ReadFromDemuxerAck(
      int player_id,
      const media::MediaPlayerHostMsg_ReadFromDemuxerAck_Params& params) = 0;

  virtual void GenerateKeyRequest(int player_id,
                                  const std::string& key_system,
                                  const std::string& type,
                                  const std::vector<uint8>& init_data) = 0;

  virtual void AddKey(int player_id,
                      const std::string& key_system,
                      const std::vector<uint8>& key,
                      const std::vector<uint8>& init_data,
                      const std::string& session_id) = 0;

  virtual void CancelKeyRequest(int player_id,
                                const std::string& key_system,
                                const std::string& session_id) = 0;

  // Inform the media source player of changed duration from demuxer.
  virtual void DurationChanged(int player_id,
                               const base::TimeDelta& duration) = 0;
};

}  // namespace webkit_media

#endif  // WEBKIT_RENDERER_MEDIA_ANDROID_WEBMEDIAPLAYER_PROXY_ANDROID_H_
