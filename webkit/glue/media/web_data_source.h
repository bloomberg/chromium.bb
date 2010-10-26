// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_MEDIA_WEB_DATA_SOURCE_H_
#define WEBKIT_GLUE_MEDIA_WEB_DATA_SOURCE_H_

#include "media/base/filters.h"

namespace webkit_glue {

// An interface that allows WebMediaPlayerImpl::Proxy to communicate with the
// DataSource in the pipeline.
class WebDataSource : public media::DataSource {
 public:
  WebDataSource();
  virtual ~WebDataSource();

  // Returns true if the media resource has a single origin, false otherwise.
  //
  // Method called on the render thread.
  virtual bool HasSingleOrigin() = 0;

  // This method is used to unblock any read calls that would cause the
  // media pipeline to stall.
  //
  // Method called on the render thread.
  virtual void Abort() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebDataSource);
};

}  // namespace webkit_glue

#endif  // WEBKIT_GLUE_MEDIA_WEB_DATA_SOURCE_H_
