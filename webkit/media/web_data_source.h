// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_MEDIA_WEB_DATA_SOURCE_H_
#define WEBKIT_MEDIA_WEB_DATA_SOURCE_H_

#include "base/callback.h"
#include "media/base/data_source.h"
#include "media/base/pipeline_status.h"

class GURL;

namespace webkit_media {

// An interface that allows WebMediaPlayerImpl::Proxy to communicate with the
// DataSource in the pipeline.
class WebDataSource : public media::DataSource {
 public:
  WebDataSource();
  virtual ~WebDataSource();

  // Initialize this object using |url|. This object calls |callback| when
  // initialization has completed.
  //
  // Method called on the render thread.
  virtual void Initialize(const GURL& url,
                          const media::PipelineStatusCB& status_cb) = 0;

  // Returns true if the media resource has a single origin, false otherwise.
  //
  // Method called on the render thread.
  virtual bool HasSingleOrigin() = 0;

  // Cancels initialization, any pending loaders, and any pending read calls
  // from the demuxer. The caller is expected to release its reference to this
  // object and never call it again.
  //
  // Method called on the render thread.
  virtual void Abort() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebDataSource);
};

}  // namespace webkit_media

#endif  // WEBKIT_MEDIA_WEB_DATA_SOURCE_H_
