// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_MEDIA_MEDIA_STREAM_AUDIO_RENDERER_H_
#define WEBKIT_MEDIA_MEDIA_STREAM_AUDIO_RENDERER_H_

#include "base/memory/ref_counted.h"
#include "media/base/media_export.h"

namespace webkit_media {

class MediaStreamAudioRenderer
    : public base::RefCountedThreadSafe<MediaStreamAudioRenderer> {
 public:
  // Starts rendering audio.
  virtual void Play() = 0;

  // Temporarily suspends rendering audio.
  virtual void Pause() = 0;

  // Stops all operations in preparation for being deleted.
  virtual void Stop() = 0;

  // Sets the output volume.
  virtual void SetVolume(float volume) = 0;

 protected:
  friend class base::RefCountedThreadSafe<MediaStreamAudioRenderer>;

  MediaStreamAudioRenderer();
  virtual ~MediaStreamAudioRenderer();

 private:
  DISALLOW_COPY_AND_ASSIGN(MediaStreamAudioRenderer);
};

}  // namespace webkit_media

#endif  // WEBKIT_MEDIA_MEDIA_STREAM_AUDIO_RENDERER_H_
