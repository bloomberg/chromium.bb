// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_MEDIA_WEB_DATA_SOURCE_H_
#define WEBKIT_MEDIA_WEB_DATA_SOURCE_H_

#include "base/callback.h"
#include "media/base/filters.h"
#include "media/base/pipeline_status.h"

namespace webkit_media {

// An interface that allows WebMediaPlayerImpl::Proxy to communicate with the
// DataSource in the pipeline.
class WebDataSource : public media::DataSource {
 public:
  WebDataSource();
  virtual ~WebDataSource();

  // Initialize this object using |url|. This object calls |callback| when
  // initialization has completed.
  virtual void Initialize(const std::string& url,
                          const media::PipelineStatusCB& callback) = 0;

  // Called to cancel initialization. The callback passed in Initialize() will
  // be destroyed and will not be called after this method returns. Once this
  // method returns, the object will be in an uninitialized state and
  // Initialize() cannot be called again. The caller is expected to release
  // its handle to this object and never call it again.
  virtual void CancelInitialize() = 0;

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

// Temporary hack to allow WebMediaPlayerImpl::Proxy::AddDataSource() to
// be called when WebDataSource objects are created. This can be removed
// once WebMediaPlayerImpl::Proxy is fixed so it doesn't have to track
// WebDataSources. Proxy only has to track WebDataSources so it can call Abort()
// on them at shutdown. Once cancellation is added to DataSource and pause
// support in Demuxers cancel pending reads, Proxy shouldn't have to keep
// a WebDataSource list or call Abort().
typedef base::Callback<void(WebDataSource*)> WebDataSourceBuildObserverHack;

}  // namespace webkit_media

#endif  // WEBKIT_MEDIA_WEB_DATA_SOURCE_H_
