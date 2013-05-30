// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_RENDERER_MEDIA_TEXTTRACK_IMPL_H_
#define WEBKIT_RENDERER_MEDIA_TEXTTRACK_IMPL_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "media/base/text_track.h"

namespace webkit_media {

class WebInbandTextTrackImpl;

class TextTrackImpl : public media::TextTrack {
 public:
  explicit TextTrackImpl(WebInbandTextTrackImpl* text_track);
  virtual ~TextTrackImpl();

  virtual void addWebVTTCue(const base::TimeDelta& start,
                            const base::TimeDelta& end,
                            const std::string& id,
                            const std::string& content,
                            const std::string& settings) OVERRIDE;

 private:
  scoped_ptr<WebInbandTextTrackImpl> text_track_;
  DISALLOW_COPY_AND_ASSIGN(TextTrackImpl);
};

}  // namespace webkit_media

#endif  // WEBKIT_RENDERER_MEDIA_TEXTTRACK_IMPL_H_

