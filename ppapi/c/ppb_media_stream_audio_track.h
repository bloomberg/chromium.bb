/* Copyright 2014 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From ppb_media_stream_audio_track.idl modified Thu Jan 23 14:08:10 2014. */

#ifndef PPAPI_C_PPB_MEDIA_STREAM_AUDIO_TRACK_H_
#define PPAPI_C_PPB_MEDIA_STREAM_AUDIO_TRACK_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/pp_var.h"

#define PPB_MEDIASTREAMAUDIOTRACK_INTERFACE_0_1 \
    "PPB_MediaStreamAudioTrack;0.1" /* dev */
/**
 * @file
 * Defines the <code>PPB_MediaStreamAudioTrack</code> interface. Used for
 * receiving audio frames from a MediaStream audio track in the browser.
 * This interface is still in development (Dev API status) and may change.
 */


/**
 * @addtogroup Enums
 * @{
 */
/**
 * This enumeration contains audio track attributes which are used by
 * <code>Configure()</code>.
 */
typedef enum {
  /**
   * Attribute list terminator.
   */
  PP_MEDIASTREAMAUDIOTRACK_ATTRIB_NONE = 0,
  /**
   * The maximum number of frames to hold in the input buffer.
   * Note: this is only used as advisory; the browser may allocate more or fewer
   * based on available resources. How many frames to buffer depends on usage -
   * request at least 2 to make sure latency doesn't cause lost frames. If
   * the plugin expects to hold on to more than one frame at a time (e.g. to do
   * multi-frame processing), it should request that many more.
   */
  PP_MEDIASTREAMAUDIOTRACK_ATTRIB_BUFFERED_FRAMES = 1,
  /**
   * The sample rate of audio frames. The attribute value is a
   * <code>PP_AudioFrame_SampleRate</code>.
   */
  PP_MEDIASTREAMAUDIOTRACK_ATTRIB_SAMPLE_RATE = 2,
  /**
   * The sample size of audio frames in bytes. The attribute value is a
   * <code>PP_AudioFrame_SampleSize</code>.
   */
  PP_MEDIASTREAMAUDIOTRACK_ATTRIB_SAMPLE_SIZE = 3,
  /**
   * The number of channels in audio frames.
   *
   * Supported values: 1, 2
   */
  PP_MEDIASTREAMAUDIOTRACK_ATTRIB_CHANNELS = 4,
  /**
   * The duration of audio frames in milliseconds.
   *
   * Valid range: 10 to 10000
   */
  PP_MEDIASTREAMAUDIOTRACK_ATTRIB_DURATION = 5
} PP_MediaStreamAudioTrack_Attrib;
/**
 * @}
 */

/**
 * @addtogroup Interfaces
 * @{
 */
struct PPB_MediaStreamAudioTrack_0_1 { /* dev */
  /**
   * Determines if a resource is a MediaStream audio track resource.
   *
   * @param[in] resource The <code>PP_Resource</code> to test.
   *
   * @return A <code>PP_Bool</code> with <code>PP_TRUE</code> if the given
   * resource is a Mediastream audio track resource or <code>PP_FALSE</code>
   * otherwise.
   */
  PP_Bool (*IsMediaStreamAudioTrack)(PP_Resource resource);
  /**
   * Configures underlying frame buffers for incoming frames.
   * If the application doesn't want to drop frames, then the
   * <code>PP_MEDIASTREAMAUDIOTRACK_ATTRIB_BUFFERED_FRAMES</code> should be
   * chosen such that inter-frame processing time variability won't overrun the
   * input buffer. If the buffer is overfilled, then frames will be dropped.
   * The application can detect this by examining the timestamp on returned
   * frames. If <code>Configure()</code> is not called, default settings will be
   * used.
   * Example usage from plugin code:
   * @code
   * int32_t attribs[] = {
   *     PP_MEDIASTREAMAUDIOTRACK_ATTRIB_BUFFERED_FRAMES, 4,
   *     PP_MEDIASTREAMAUDIOTRACK_ATTRIB_DURATION, 10,
   *     PP_MEDIASTREAMAUDIOTRACK_ATTRIB_NONE};
   * track_if->Configure(track, attribs, callback);
   * @endcode
   *
   * @param[in] audio_track A <code>PP_Resource</code> corresponding to an audio
   * resource.
   * @param[in] attrib_list A list of attribute name-value pairs in which each
   * attribute is immediately followed by the corresponding desired value.
   * The list is terminated by
   * <code>PP_MEDIASTREAMAUDIOTRACK_ATTRIB_NONE</code>.
   * @param[in] callback A <code>PP_CompletionCallback</code> to be called upon
   * completion of <code>Configure()</code>.
   *
   * @return An int32_t containing a result code from <code>pp_errors.h</code>.
   */
  int32_t (*Configure)(PP_Resource audio_track,
                       const int32_t attrib_list[],
                       struct PP_CompletionCallback callback);
  /**
   * Gets attribute value for a given attribute name.
   *
   * @param[in] audio_track A <code>PP_Resource</code> corresponding to an audio
   * resource.
   * @param[in] attrib A <code>PP_MediaStreamAudioTrack_Attrib</code> for
   * querying.
   * @param[out] value A int32_t for storing the attribute value on success.
   * Otherwise, the value will not be changed.
   *
   * @return An int32_t containing a result code from <code>pp_errors.h</code>.
   */
  int32_t (*GetAttrib)(PP_Resource audio_track,
                       PP_MediaStreamAudioTrack_Attrib attrib,
                       int32_t* value);
  /**
   * Returns the track ID of the underlying MediaStream audio track.
   *
   * @param[in] audio_track The <code>PP_Resource</code> to check.
   *
   * @return A <code>PP_Var</code> containing the MediaStream track ID as
   * a string.
   */
  struct PP_Var (*GetId)(PP_Resource audio_track);
  /**
   * Checks whether the underlying MediaStream track has ended.
   * Calls to GetFrame while the track has ended are safe to make and will
   * complete, but will fail.
   *
   * @param[in] audio_track The <code>PP_Resource</code> to check.
   *
   * @return A <code>PP_Bool</code> with <code>PP_TRUE</code> if the given
   * MediaStream track has ended or <code>PP_FALSE</code> otherwise.
   */
  PP_Bool (*HasEnded)(PP_Resource audio_track);
  /**
   * Gets the next audio frame from the MediaStream track.
   * If internal processing is slower than the incoming frame rate, new frames
   * will be dropped from the incoming stream. Once the input buffer is full,
   * frames will be dropped until <code>RecycleFrame()</code> is called to free
   * a spot for another frame to be buffered.
   * If there are no frames in the input buffer,
   * <code>PP_OK_COMPLETIONPENDING</code> will be returned immediately and the
   * <code>callback</code> will be called, when a new frame is received or an
   * error happens.
   *
   * @param[in] audio_track A <code>PP_Resource</code> corresponding to an audio
   * resource.
   * @param[out] frame A <code>PP_Resource</code> corresponding to an AudioFrame
   * resource.
   * @param[in] callback A <code>PP_CompletionCallback</code> to be called upon
   * completion of GetFrame().
   *
   * @return An int32_t containing a result code from <code>pp_errors.h</code>.
   * Returns PP_ERROR_NOMEMORY if <code>max_buffered_frames</code> frames buffer
   * was not allocated successfully.
   */
  int32_t (*GetFrame)(PP_Resource audio_track,
                      PP_Resource* frame,
                      struct PP_CompletionCallback callback);
  /**
   * Recycles a frame returned by <code>GetFrame()</code>, so the track can
   * reuse the underlying buffer of this frame. And the frame will become
   * invalid. The caller should release all references it holds to
   * <code>frame</code> and not use it anymore.
   *
   * @param[in] audio_track A <code>PP_Resource</code> corresponding to an audio
   * resource.
   * @param[in] frame A <code>PP_Resource</code> corresponding to an AudioFrame
   * resource returned by <code>GetFrame()</code>.
   *
   * @return An int32_t containing a result code from <code>pp_errors.h</code>.
   */
  int32_t (*RecycleFrame)(PP_Resource audio_track, PP_Resource frame);
  /**
   * Closes the MediaStream audio track and disconnects it from the audio
   * source. After calling <code>Close()</code>, no new frames will be received.
   *
   * @param[in] audio_track A <code>PP_Resource</code> corresponding to a
   * MediaStream audio track resource.
   */
  void (*Close)(PP_Resource audio_track);
};
/**
 * @}
 */

#endif  /* PPAPI_C_PPB_MEDIA_STREAM_AUDIO_TRACK_H_ */

