// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebMediaPlayerSource_h
#define WebMediaPlayerSource_h

#include "WebCommon.h"
#include "WebMediaStream.h"
#include "WebURL.h"

namespace blink {

class BLINK_PLATFORM_EXPORT WebMediaPlayerSource {
 public:
  WebMediaPlayerSource();
  explicit WebMediaPlayerSource(const WebURL&);
  explicit WebMediaPlayerSource(const WebMediaStream&);
  ~WebMediaPlayerSource();

  bool IsURL() const;
  WebURL GetAsURL() const;

  bool IsMediaStream() const;
  WebMediaStream GetAsMediaStream() const;

 private:
  WebURL url_;
  WebMediaStream media_stream_;
};

}  // namespace blink

#endif  // WebMediaPlayerSource_h
