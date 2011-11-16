// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_MEDIA_BUFFERED_DATA_SOURCE_FACTORY_H_
#define WEBKIT_GLUE_MEDIA_BUFFERED_DATA_SOURCE_FACTORY_H_

#include "base/memory/ref_counted.h"
#include "media/base/async_filter_factory_base.h"
#include "webkit/glue/media/web_data_source.h"

class MessageLoop;

namespace media {
class MediaLog;
}

namespace WebKit {
class WebFrame;
}

namespace webkit_glue {

class WebDataSourceFactory : public media::AsyncDataSourceFactoryBase {
 public:
  typedef WebDataSource* (*FactoryFunction)(MessageLoop* render_loop,
                                            WebKit::WebFrame* frame,
                                            media::MediaLog* media_log);

  WebDataSourceFactory(MessageLoop* render_loop, WebKit::WebFrame* frame,
                       media::MediaLog* media_log,
                       FactoryFunction factory_function,
                       const WebDataSourceBuildObserverHack& build_observer);
  virtual ~WebDataSourceFactory();

  // DataSourceFactory method.
  virtual media::DataSourceFactory* Clone() const OVERRIDE;

 protected:
  // AsyncDataSourceFactoryBase methods.
  virtual bool AllowRequests() const OVERRIDE;
  virtual AsyncDataSourceFactoryBase::BuildRequest* CreateRequest(
      const std::string& url, const BuildCallback& callback) OVERRIDE;

 private:
  class BuildRequest;

  MessageLoop* render_loop_;
  WebKit::WebFrame* frame_;
  scoped_refptr<media::MediaLog> media_log_;
  FactoryFunction factory_function_;
  WebDataSourceBuildObserverHack build_observer_;

  DISALLOW_COPY_AND_ASSIGN(WebDataSourceFactory);
};

}  // namespace webkit_glue

#endif  // WEBKIT_GLUE_MEDIA_BUFFERED_DATA_SOURCE_FACTORY_H_
