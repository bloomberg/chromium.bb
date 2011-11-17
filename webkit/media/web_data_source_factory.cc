// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/media/web_data_source_factory.h"

#include "base/bind.h"
#include "base/logging.h"
#include "media/base/media_log.h"

namespace webkit_media {

class WebDataSourceFactory::BuildRequest
    : public media::AsyncDataSourceFactoryBase::BuildRequest {
 public:
  BuildRequest(const std::string& url, const BuildCallback& callback,
               WebDataSource* data_source,
               const WebDataSourceBuildObserverHack& build_observer);
  virtual ~BuildRequest();

 protected:
  // AsyncDataSourceFactoryBase::BuildRequest method.
  virtual void DoStart();

 private:
  void InitDone(media::PipelineStatus status);

  scoped_refptr<WebDataSource> data_source_;
  WebDataSourceBuildObserverHack build_observer_;

  DISALLOW_COPY_AND_ASSIGN(BuildRequest);
};

WebDataSourceFactory::WebDataSourceFactory(
    MessageLoop* render_loop,
    WebKit::WebFrame* frame,
    media::MediaLog* media_log,
    FactoryFunction factory_function,
    const WebDataSourceBuildObserverHack& build_observer)
    : render_loop_(render_loop),
      frame_(frame),
      media_log_(media_log),
      factory_function_(factory_function),
      build_observer_(build_observer) {
  DCHECK(render_loop_);
  DCHECK(frame_);
  DCHECK(media_log_);
  DCHECK(factory_function_);
}

WebDataSourceFactory::~WebDataSourceFactory() {}

media::DataSourceFactory* WebDataSourceFactory::Clone() const {
  return new WebDataSourceFactory(render_loop_, frame_, media_log_,
                                  factory_function_, build_observer_);
}

bool WebDataSourceFactory::AllowRequests() const {
  return true;
}

media::AsyncDataSourceFactoryBase::BuildRequest*
WebDataSourceFactory::CreateRequest(const std::string& url,
                                    const BuildCallback& callback) {
  WebDataSource* data_source = factory_function_(render_loop_, frame_,
                                                 media_log_);

  return new WebDataSourceFactory::BuildRequest(url, callback, data_source,
                                                build_observer_);
}

WebDataSourceFactory::BuildRequest::BuildRequest(
    const std::string& url,
    const BuildCallback& callback,
    WebDataSource* data_source,
    const WebDataSourceBuildObserverHack& build_observer)
    : AsyncDataSourceFactoryBase::BuildRequest(url, callback),
      data_source_(data_source),
      build_observer_(build_observer) {
}

WebDataSourceFactory::BuildRequest::~BuildRequest() {
  if (data_source_.get()) {
    data_source_->CancelInitialize();
    data_source_ = NULL;
  }
}

void WebDataSourceFactory::BuildRequest::DoStart() {
  data_source_->Initialize(url(), base::Bind(&BuildRequest::InitDone,
                                             base::Unretained(this)));
}

void WebDataSourceFactory::BuildRequest::InitDone(
    media::PipelineStatus status) {
  scoped_refptr<WebDataSource> data_source;

  data_source = (status == media::PIPELINE_OK) ? data_source_ : NULL;
  data_source_ = NULL;

  if (!build_observer_.is_null() && data_source.get()) {
    build_observer_.Run(data_source.get());
  }

  RequestComplete(status, data_source);
  // Don't do anything after this line. This object is deleted by
  // RequestComplete().
}

}  // namespace webkit_media
