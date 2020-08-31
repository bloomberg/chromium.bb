// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_MODULES_MEDIASTREAM_MEDIA_STREAM_VIDEO_SOURCE_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_MODULES_MEDIASTREAM_MEDIA_STREAM_VIDEO_SOURCE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/threading/thread_checker.h"
#include "media/base/video_frame.h"
#include "media/capture/video_capture_types.h"
#include "third_party/blink/public/common/media/video_capture.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom-shared.h"
#include "third_party/blink/public/platform/modules/mediastream/media_stream_types.h"
#include "third_party/blink/public/platform/modules/mediastream/secure_display_link_tracker.h"
#include "third_party/blink/public/platform/modules/mediastream/web_platform_media_stream_source.h"
#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/platform/web_media_stream_source.h"
#include "third_party/blink/public/platform/web_media_stream_track.h"
#include "third_party/blink/public/web/modules/mediastream/encoded_video_frame.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace blink {

class MediaStreamVideoTrack;
class VideoTrackAdapter;
class VideoTrackAdapterSettings;

// MediaStreamVideoSource is an interface used for sending video frames to a
// MediaStreamVideoTrack.
// https://dev.w3.org/2011/webrtc/editor/getusermedia.html
// The purpose of this base class is to be able to implement different
// MediaStreamVideoSources such as local video capture, video sources received
// on a PeerConnection or a source created in NaCl.
// All methods calls will be done from the main render thread.
class BLINK_MODULES_EXPORT MediaStreamVideoSource
    : public WebPlatformMediaStreamSource {
 public:
  enum {
    // Default resolution. If no constraints are specified and the delegate
    // support it, this is the resolution that will be used.
    kDefaultWidth = 640,
    kDefaultHeight = 480,

    kDefaultFrameRate = 30,
    kUnknownFrameRate = 0,
  };

  enum class RestartResult { IS_RUNNING, IS_STOPPED, INVALID_STATE };
  // RestartCallback is used for both the StopForRestart and Restart operations.
  using RestartCallback = base::OnceCallback<void(RestartResult)>;

  MediaStreamVideoSource();
  ~MediaStreamVideoSource() override;

  // Returns the MediaStreamVideoSource object owned by |source|.
  static MediaStreamVideoSource* GetVideoSource(
      const WebMediaStreamSource& source);

  // Puts |track| in the registered tracks list.
  void AddTrack(MediaStreamVideoTrack* track,
                const VideoTrackAdapterSettings& track_adapter_settings,
                const VideoCaptureDeliverFrameCB& frame_callback,
                const EncodedVideoFrameCB& encoded_frame_callback,
                const VideoTrackSettingsCallback& settings_callback,
                const VideoTrackFormatCallback& format_callback,
                ConstraintsOnceCallback callback);
  void RemoveTrack(MediaStreamVideoTrack* track, base::OnceClosure callback);

  // Reconfigures this MediaStreamVideoSource to use |adapter_settings| on
  // |track|, as long as |track| is connected to this source.
  // Do not invoke if |track| is connected to a different source, as the
  // internal state of |track| might become inconsistent with that of its
  // source.
  void ReconfigureTrack(MediaStreamVideoTrack* track,
                        const VideoTrackAdapterSettings& adapter_settings);

  // Tries to temporarily stop this source so that it can be later restarted
  // with a different video format. Unlike MediaStreamVideoSource::StopSource(),
  // a temporary stop for restart does not change the ready state of the source.
  // Once the attempt to temporarily stop the source is completed, |callback|
  // is invoked with IS_STOPPED if the source actually stopped, or IS_RUNNING
  // if the source did not stop and is still running.
  // This method can only be called after a source has started. This can be
  // verified by checking that the IsRunning() method returns true.
  // Any attempt to invoke StopForRestart() before the source has started
  // results in no action and |callback| invoked with INVALID_STATE.
  void StopForRestart(RestartCallback callback);

  // Tries to restart a source that was previously temporarily stopped using the
  // supplied |new_format|. This method can be invoked only after a successful
  // call to StopForRestart().
  // Once the attempt to restart the source is completed, |callback| is invoked
  // with IS_RUNNING if the source restarted and IS_STOPPED if the source
  // remained stopped. Note that it is not guaranteed that the source actually
  // restarts using |new_format| as its configuration. After a successful
  // restart, the actual configured format for the source (if available) can be
  // obtained with a call to GetCurrentFormat().
  // Note also that, since frames are delivered on a different thread, it is
  // possible that frames using the old format are delivered for a while after
  // a successful restart. Code relying on Restart() cannot assume that new
  // frames are guaranteed to arrive in the new format until the first frame in
  // the new format is received.
  // This method can only be called after a successful stop for restart (i.e.,
  // after the callback passed to StopForRestart() is invoked with a value of
  // IS_STOPPED). Any attempt to invoke Restart() when the source is not in this
  // state results in no action and |callback| invoked with INVALID_STATE.
  void Restart(const media::VideoCaptureFormat& new_format,
               RestartCallback callback);

  // Called by |track| to notify the source whether it has any paths to a
  // consuming endpoint.
  void UpdateHasConsumers(MediaStreamVideoTrack* track, bool has_consumers);

  void UpdateCapturingLinkSecure(MediaStreamVideoTrack* track, bool is_secure);

  // Request underlying source to capture a new frame.
  virtual void RequestRefreshFrame() {}

  // Optionally overridden by subclasses to implement handling frame drop
  // events.
  virtual void OnFrameDropped(media::VideoCaptureFrameDropReason reason) {}

  // Optionally overridden by subclasses to implement handling log messages.
  virtual void OnLog(const std::string& message) {}

  // Enables or disables an heuristic to detect frames from rotated devices.
  void SetDeviceRotationDetection(bool enabled);

  // Returns the task runner where video frames will be delivered on.
  base::SingleThreadTaskRunner* io_task_runner() const;

  // Implementations must return the capture format if available.
  // Implementations supporting devices of type MEDIA_DEVICE_VIDEO_CAPTURE
  // must return a value.
  virtual base::Optional<media::VideoCaptureFormat> GetCurrentFormat() const;

  // Implementations must return the capture parameters if available.
  // Implementations supporting devices of type MEDIA_DEVICE_VIDEO_CAPTURE
  // must return a value. The format in the returned VideoCaptureParams must
  // coincide with the value returned by GetCurrentFormat().
  virtual base::Optional<media::VideoCaptureParams> GetCurrentCaptureParams()
      const;

  // Returns true if encoded output can be enabled in the source.
  virtual bool SupportsEncodedOutput() const;

  // Notifies the source about that the number of encoded sinks have been
  // updated. Note: Can only be called if the number of encoded sinks have
  // actually changed!
  void UpdateNumEncodedSinks();

  bool IsRunning() const { return state_ == STARTED; }

  size_t NumTracks() const {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    return tracks_.size();
  }

  base::WeakPtr<MediaStreamVideoSource> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 protected:
  // MediaStreamSource implementation.
  void DoChangeSource(const MediaStreamDevice& new_device) override;
  void DoStopSource() override;

  // Sets ready state and notifies the ready state to all registered tracks.
  virtual void SetReadyState(WebMediaStreamSource::ReadyState state);

  // Sets muted state and notifies it to all registered tracks.
  virtual void SetMutedState(bool state);

  // An implementation must start capturing frames after this method is called.
  // When the source has started or failed to start OnStartDone must be called.
  // An implementation must call |frame_callback| on the IO thread with the
  // captured frames, and |encoded_frame_callback| with encoded frames if
  // supported and enabled via OnEncodedSinkEnabled.
  virtual void StartSourceImpl(VideoCaptureDeliverFrameCB frame_callback,
                               EncodedVideoFrameCB encoded_frame_callback) = 0;
  void OnStartDone(mojom::MediaStreamRequestResult result);

  // A subclass that supports restart must override this method such that it
  // immediately stop producing video frames after this method is called.
  // The stop is intended to be temporary and to be followed by a restart. Thus,
  // connected tracks should not be disconnected or notified about the source no
  // longer producing frames. Once the source is stopped, the implementation
  // must invoke OnStopForRestartDone() with true. If the source cannot stop,
  // OnStopForRestartDone() must invoked with false.
  // It can be assumed that this method is invoked only when the source is
  // running.
  // Note that if this method is overridden, RestartSourceImpl() must also be
  // overridden following the respective contract. Otherwise, behavior is
  // undefined.
  // The default implementation does not support restart and just calls
  // OnStopForRestartDone() with false.
  virtual void StopSourceForRestartImpl();

  // This method should be called by implementations once an attempt to stop
  // for restart using StopSourceForRestartImpl() is completed.
  // |did_stop_for_restart| must true if the source is stopped and false if
  // the source is running.
  void OnStopForRestartDone(bool did_stop_for_restart);

  // A subclass that supports restart must override this method such that it
  // tries to start producing frames after this method is called. If successful,
  // the source should return to the same state as if it was started normally
  // and invoke OnRestartDone() with true. The implementation should preferably
  // restart to produce frames with the format specified in |new_format|.
  // However, if this is not possible, the implementation is allowed to restart
  // using a different format. In this case OnRestartDone() should be invoked
  // with true as well. If it is impossible to restart the source with any
  // format, the source should remain stopped and OnRestartDone() should be
  // invoked with false.
  // This method can only be invoked when the source is temporarily stopped
  // after a successful OnStopForRestartDone(). Otherwise behavior is undefined.
  // Note that if this method is overridden, StopSourceForRestartImpl() must
  // also be overridden following the respective contract. Otherwise, behavior
  // is undefined.
  virtual void RestartSourceImpl(const media::VideoCaptureFormat& new_format);

  // This method should be called by implementations once an attempt to restart
  // the source completes. |did_restart| must be true if the source is running
  // and false if the source is stopped.
  void OnRestartDone(bool did_restart);

  // An implementation must immediately stop producing video frames after this
  // method has been called. After this method has been called,
  // MediaStreamVideoSource may be deleted.
  virtual void StopSourceImpl() = 0;

  // Optionally overridden by subclasses to act on whether there are any
  // consumers present. When none are present, the source can stop delivering
  // frames, giving it the option of running in an "idle" state to minimize
  // resource usage.
  virtual void OnHasConsumers(bool has_consumers) {}

  // Optionally overridden by subclasses to act on whether the capturing link
  // has become secure or insecure.
  virtual void OnCapturingLinkSecured(bool is_secure) {}

  // Optionally overridden by subclasses to implement changing source.
  virtual void ChangeSourceImpl(const MediaStreamDevice& new_device) {}

  // Optionally override by subclasses to implement encoded source control.
  // The method is called when at least one encoded sink has been added.
  virtual void OnEncodedSinkEnabled() {}

  // Optionally override by subclasses to implement encoded source control.
  // The method is called when the last encoded sink has been removed.
  virtual void OnEncodedSinkDisabled() {}

  enum State {
    NEW,
    STARTING,
    STOPPING_FOR_RESTART,
    STOPPED_FOR_RESTART,
    RESTARTING,
    STARTED,
    ENDED
  };
  State state() const { return state_; }

  THREAD_CHECKER(thread_checker_);

 private:
  // Trigger all cached callbacks from AddTrack. AddTrack is successful
  // if the capture delegate has started and the constraints provided in
  // AddTrack match the format that was used to start the device.
  // Note that it must be ok to delete the MediaStreamVideoSource object
  // in the context of the callback. If gUM fails, the implementation will
  // simply drop the references to the blink source and track which will lead
  // to this object being deleted.
  void FinalizeAddPendingTracks();

  // Actually adds |track| to this source, provided the source has started.
  void FinalizeAddTrack(MediaStreamVideoTrack* track,
                        const VideoCaptureDeliverFrameCB& frame_callback,
                        const VideoTrackAdapterSettings& adapter_settings);
  void StartFrameMonitoring();
  void UpdateTrackSettings(MediaStreamVideoTrack* track,
                           const VideoTrackAdapterSettings& adapter_settings);
  void DidStopSource(RestartResult result);
  void NotifyCapturingLinkSecured(size_t num_encoded_sinks);
  size_t CountEncodedSinks() const;

  State state_;

  struct PendingTrackInfo {
    PendingTrackInfo(
        MediaStreamVideoTrack* track,
        const VideoCaptureDeliverFrameCB& frame_callback,
        const EncodedVideoFrameCB& encoded_frame_callback,
        const VideoTrackSettingsCallback& settings_callback,
        const VideoTrackFormatCallback& format_callback,
        std::unique_ptr<VideoTrackAdapterSettings> adapter_settings,
        ConstraintsOnceCallback callback);
    PendingTrackInfo(PendingTrackInfo&& other);
    PendingTrackInfo& operator=(PendingTrackInfo&& other);
    ~PendingTrackInfo();

    MediaStreamVideoTrack* track;
    VideoCaptureDeliverFrameCB frame_callback;
    EncodedVideoFrameCB encoded_frame_callback;
    VideoTrackSettingsCallback settings_callback;
    VideoTrackFormatCallback format_callback;
    // TODO(guidou): Make |adapter_settings| a regular field instead of a
    // unique_ptr.
    std::unique_ptr<VideoTrackAdapterSettings> adapter_settings;
    ConstraintsOnceCallback callback;
  };
  std::vector<PendingTrackInfo> pending_tracks_;

  // |restart_callback_| is used for notifying both StopForRestart and Restart,
  // since it is impossible to have a situation where there can be callbacks
  // for both at the same time.
  RestartCallback restart_callback_;

  // |track_adapter_| delivers video frames to the tracks on the IO-thread.
  scoped_refptr<VideoTrackAdapter> track_adapter_;

  // Tracks that currently are connected to this source.
  std::vector<MediaStreamVideoTrack*> tracks_;

  // Tracks that have no paths to a consuming endpoint, and so do not need
  // frames delivered from the source. This is a subset of |tracks_|.
  std::vector<MediaStreamVideoTrack*> suspended_tracks_;

  // This is used for tracking if all connected video sinks are secure.
  SecureDisplayLinkTracker<MediaStreamVideoTrack> secure_tracker_;

  // This flag enables a heuristic to detect device rotation based on frame
  // size.
  bool enable_device_rotation_detection_ = false;

  // Callback that needs to trigger after removing the track. If this object
  // died before this callback is resolved, we still need to trigger the
  // callback to notify the caller that the request is canceled.
  base::OnceClosure remove_last_track_callback_;

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<MediaStreamVideoSource> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(MediaStreamVideoSource);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_WEB_MODULES_MEDIASTREAM_MEDIA_STREAM_VIDEO_SOURCE_H_
