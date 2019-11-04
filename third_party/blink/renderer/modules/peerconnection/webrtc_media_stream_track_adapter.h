// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_WEBRTC_MEDIA_STREAM_TRACK_ADAPTER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_WEBRTC_MEDIA_STREAM_TRACK_ADAPTER_H_

#include <memory>

#include "base/synchronization/waitable_event.h"
#include "third_party/blink/public/platform/web_media_stream.h"
#include "third_party/blink/public/platform/web_media_stream_track.h"
#include "third_party/blink/renderer/modules/mediastream/remote_media_stream_track_adapter.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/modules/peerconnection/media_stream_video_webrtc_sink.h"
#include "third_party/blink/renderer/platform/peerconnection/webrtc_audio_sink.h"
#include "third_party/blink/renderer/platform/wtf/thread_safe_ref_counted.h"
#include "third_party/webrtc/api/media_stream_interface.h"

namespace blink {

class PeerConnectionDependencyFactory;
struct WebRtcMediaStreamTrackAdapterTraits;

// This is a mapping between a webrtc and blink media stream track. It takes
// care of creation, initialization and disposing of tracks independently of
// media streams.
// There are different sinks/adapters used whether the track is local or remote
// and whether it is an audio or video track; this adapter hides that fact and
// lets you use a single class for any type of track.
class MODULES_EXPORT WebRtcMediaStreamTrackAdapter
    : public WTF::ThreadSafeRefCounted<WebRtcMediaStreamTrackAdapter,
                                       WebRtcMediaStreamTrackAdapterTraits> {
 public:
  // Invoke on the main thread. The returned adapter is fully initialized, see
  // |is_initialized|. The adapter will keep a reference to the |main_thread|.
  static scoped_refptr<WebRtcMediaStreamTrackAdapter> CreateLocalTrackAdapter(
      blink::PeerConnectionDependencyFactory* factory,
      const scoped_refptr<base::SingleThreadTaskRunner>& main_thread,
      const blink::WebMediaStreamTrack& web_track);
  // Invoke on the webrtc signaling thread. Initialization finishes on the main
  // thread in a post, meaning returned adapters are ensured to be initialized
  // in posts to the main thread, see |is_initialized|. The adapter will keep
  // references to the |main_thread| and |webrtc_track|.
  static scoped_refptr<WebRtcMediaStreamTrackAdapter> CreateRemoteTrackAdapter(
      blink::PeerConnectionDependencyFactory* factory,
      const scoped_refptr<base::SingleThreadTaskRunner>& main_thread,
      const scoped_refptr<webrtc::MediaStreamTrackInterface>& webrtc_track);
  // Must be called before all external references are released (i.e. before
  // destruction). Invoke on the main thread. Disposing may finish
  // asynchronously using the webrtc signaling thread and the main thread. After
  // calling this method it is safe to release all external references to the
  // adapter.
  void Dispose();

  bool is_initialized() const;
  void InitializeOnMainThread();
  // These methods must be called on the main thread.
  // TODO(hbos): Allow these methods to be called on any thread and make them
  // const. https://crbug.com/756436
  const blink::WebMediaStreamTrack& web_track();
  webrtc::MediaStreamTrackInterface* webrtc_track();
  bool IsEqual(const blink::WebMediaStreamTrack& web_track);

  // For testing.
  blink::WebRtcAudioSink* GetLocalTrackAudioSinkForTesting() {
    return local_track_audio_sink_.get();
  }
  blink::MediaStreamVideoWebRtcSink* GetLocalTrackVideoSinkForTesting() {
    return local_track_video_sink_.get();
  }
  blink::RemoteAudioTrackAdapter* GetRemoteAudioTrackAdapterForTesting() {
    return remote_audio_track_adapter_.get();
  }
  blink::RemoteVideoTrackAdapter* GetRemoteVideoTrackAdapterForTesting() {
    return remote_video_track_adapter_.get();
  }

 protected:
  friend class WTF::ThreadSafeRefCounted<WebRtcMediaStreamTrackAdapter,
                                         WebRtcMediaStreamTrackAdapterTraits>;
  friend struct WebRtcMediaStreamTrackAdapterTraits;

  WebRtcMediaStreamTrackAdapter(
      blink::PeerConnectionDependencyFactory* factory,
      const scoped_refptr<base::SingleThreadTaskRunner>& main_thread);
  virtual ~WebRtcMediaStreamTrackAdapter();

 private:
  // Initialization of local tracks occurs on the main thread.
  void InitializeLocalAudioTrack(const blink::WebMediaStreamTrack& web_track);
  void InitializeLocalVideoTrack(const blink::WebMediaStreamTrack& web_track);
  // Initialization of remote tracks starts on the webrtc signaling thread and
  // finishes on the main thread.
  void InitializeRemoteAudioTrack(
      const scoped_refptr<webrtc::AudioTrackInterface>& webrtc_audio_track);
  void InitializeRemoteVideoTrack(
      const scoped_refptr<webrtc::VideoTrackInterface>& webrtc_video_track);
  void FinalizeRemoteTrackInitializationOnMainThread();
  void EnsureTrackIsInitialized();

  // Disposing starts and finishes on the main thread. Local tracks and remote
  // video tracks are disposed synchronously. Remote audio tracks are disposed
  // asynchronously with a jump to the webrtc signaling thread and back.
  void DisposeLocalAudioTrack();
  void DisposeLocalVideoTrack();
  void DisposeRemoteAudioTrack();
  void DisposeRemoteVideoTrack();
  void UnregisterRemoteAudioTrackAdapterOnSignalingThread();
  void FinalizeRemoteTrackDisposingOnMainThread();

  // Pointer to a |PeerConnectionDependencyFactory| owned by the |RenderThread|.
  // It's valid for the lifetime of |RenderThread|.
  blink::PeerConnectionDependencyFactory* const factory_;
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_;

  // Part of the initialization of remote tracks occurs on the signaling thread.
  // |remote_track_can_complete_initialization_| allows waiting until that part
  // of the process is finished, so that full initialization of the track can be
  // completed on the main thread.
  base::WaitableEvent remote_track_can_complete_initialization_;
  bool is_initialized_;
  bool is_disposed_;
  blink::WebMediaStreamTrack web_track_;
  scoped_refptr<webrtc::MediaStreamTrackInterface> webrtc_track_;
  // If the track is local, a sink is added to the local webrtc track that is
  // owned by us.
  std::unique_ptr<blink::WebRtcAudioSink> local_track_audio_sink_;
  std::unique_ptr<blink::MediaStreamVideoWebRtcSink> local_track_video_sink_;
  // If the track is remote, an adapter is used that listens to notifications on
  // the remote webrtc track and notifies Blink.
  scoped_refptr<blink::RemoteAudioTrackAdapter> remote_audio_track_adapter_;
  scoped_refptr<blink::RemoteVideoTrackAdapter> remote_video_track_adapter_;

  DISALLOW_COPY_AND_ASSIGN(WebRtcMediaStreamTrackAdapter);
};

struct MODULES_EXPORT WebRtcMediaStreamTrackAdapterTraits {
  // Ensure destruction occurs on main thread so that "Web" and other resources
  // are destroyed on the correct thread.
  static void Destruct(const WebRtcMediaStreamTrackAdapter* adapter);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_WEBRTC_MEDIA_STREAM_TRACK_ADAPTER_H_
