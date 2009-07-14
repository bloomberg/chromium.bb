/*
 * Copyright 2008, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * NaCl low-level runtime library interfaces.
 */

#ifndef NATIVE_CLIENT_SRC_UNTRUSTED_AV_NACL_AV_H_
#define NATIVE_CLIENT_SRC_UNTRUSTED_AV_NACL_AV_H_

#include <sys/audio_video.h>

/**
 * @file
 * Defines the API in the
 * <a href="group__audio__video.html">Basic Multimedia Interface</a>
 *
 * @addtogroup audio_video
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 *  @nacl
 *  Initializes the multimedia system.  Should be called from the main
 *  thread.
 *
 *  @param[in] subsystems a bit set consisting of NACL_SUBSYSTEM_VIDEO,
 *  and/or NACL_SUBSYSTEM_AUDIO.
 *  @return 0 on success, -1 on failure (sets errno appropriately)
 */
extern int nacl_multimedia_init(int subsystems);
/**
 *  @nacl
 *  Shuts down the multimedia system.  Successfully initialized subsystems
 *  must be shut down prior to making this call.  This functio should be
 *  called from the same thread that initialized the multimedia system.
 *
 *  @return 0 on success, -1 on failure (sets errno appropriately)
 */
extern int nacl_multimedia_shutdown();
/**
 *  @nacl
 *  Whether or not the nacl app is embedded in the browser.
 *
 *  @param[out] embedded set to 0 if not embedded, 1 if embedded.
 *
 *  @return 0 on success, -1 on failure (sets errno appropriately)
 */
extern int nacl_multimedia_is_embedded(int *embedded);
/**
 *  @nacl
 *  Gets width & height of embedded region in the browser.  Non-embedded
 *  applications return a width & height of 0.
 *
 *  @param[out] width width of application region in browser.
 *  @param[out] height height of application region in browser.
 *
 *  @return 0 on success, -1 on failure (sets errno appropriately)
 */
extern int nacl_multimedia_get_embed_size(int *width, int *height);
/**
 *  @nacl
 *  Initializes a video output window.  A Native Client
 *  application can have only one window open at a time.
 *  nacl_multimedia_init() with the NACL_SUBSYSTEM_VIDEO flag must be
 *  called before this function.  This function should be called
 *  from the same thread that initialized the multimedia system.
 *
 *  @param[in] width width of output window in pixels
 *  @param[in] height height of output window in pixels
 *  @return 0 on success, -1 on failure (sets errno appropriately)
 */
extern int nacl_video_init(int width, int height);
/**
 *  @nacl
 *  Shuts down and closes video output window.  If the application
 *  made a successful call to nacl_video_init(), it must call
 *  nacl_video_shutdown() before calling nacl_multimedia_shutdown().
 *  This function should be called from the same thread that initialized
 *  the video subsystem.
 *
 *  @return 0 on success, -1 on failure (sets errno appropriately)
 */
extern int nacl_video_shutdown();
/**
 *  @nacl
 *  Updates the video window with new frame buffer data.  This
 *  function should be called from the same thread that initialized
 *  the video subsystem.
 *
 *  @param[in] data framebuffer data, in source format and size
 *  specified in nacl_video_init().
 *  @return 0 on success, -1 on failure (sets errno appropriately)
 */
extern int nacl_video_update(const void *data);
/**
 *  @nacl
 *  Polls a nacl video window for user events.  This function
 *  should be called from the same thread that initialized the
 *  video subsystem.
 *
 *  @param[out] event filled in with a valid event on success.
 *  @return 0 on success, event data is valid.  -1 on failure,
 *  no event pending.  (sets errno appropriately)
 */
extern int nacl_video_poll_event(union NaClMultimediaEvent *event);
/**
 *  @nacl
 *  Initializes audio subsystem.  nacl_multimedia_init() with
 *  the NACL_SUBSYSTEM_AUDIO flag must be called before this function.
 *  This function should be called from the same thread that initialized
 *  the multimedia system.
 *
 *  @param[in] format the desired audio format; NACL_AUDIO_FORMAT_STEREO_44K
 *  or NACL_AUDIO_FORMAT_STEREO_48K
 *  @param[in] desired_samples the desired number of samples for each buffer
 *  @param[out] obtained_samples the obtained number of samples for
 *  each buffer.  Note that this might differ from desired_samples.
 *  @return 0 on success, -1 on failure (sets errno appropriately -- if
 *  errno is ENODATA then no event was pending; other values of errno
 *  indicate failure.)
 */
extern int nacl_audio_init(enum NaClAudioFormat format,
                           int desired_samples, int *obtained_samples);
/**
 *  @nacl
 *  Shuts down the audio subsystem.  If the application
 *  made a successful call to nacl_audio_init(), it must call
 *  nacl_audio_shutdown() before calling nacl_multimedia_shutdown().
 *  This function should be called from the same thread that
 *  initialized the audio subsystem.
 *
 *  @return 0 on success, -1 on failure (sets errno appropriately)
 */
extern int nacl_audio_shutdown();
/**
 *  @nacl
 *  Streams audio data to device.  Call from a dedicated audio pthread.
 *  Input data is streamed out to the audio device.  Then, this function
 *  will sleep the thread until the audio device is ready for the next
 *  chunk of audio data.  Upon waking up, the function returns the size
 *  for the next chunk of audio data.
 *
 *  @param[in] data source data, 2 channels, 16 bits per channel.  The
 *  expected size of this data is the size argument returned by the
 *  previous call to nacl_audio_stream().  On the first call to
 *  nacl_audio_stream(), this argument is ignored.
 *  @param[out] size number of data bytes to feed the next call to
 *  nacl_audio_stream().  Returned size guaranteed not to be larger than
 *  kNaClAudioBufferLength.
 *  @return 0 on success, -1 on failure (sets errno appropriately)
 */
extern int nacl_audio_stream(const void *data, size_t *size);

#ifdef __cplusplus
}
#endif

/**
 * @}
 * End of Audio/Video group
 */

#endif  /* NATIVE_CLIENT_SRC_UNTRUSTED_AV_NACL_AV_H_ */
