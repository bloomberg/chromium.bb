// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/media/web_data_source_factory.h"

#include "base/bind.h"
#include "base/logging.h"

namespace webkit_glue {

static void DataSourceInitDone(
    WebDataSourceBuildObserverHack* build_observer,
    const media::DataSourceFactory::BuildCB& callback,
    const scoped_refptr<WebDataSource>& data_source,
    media::PipelineStatus status) {

  if (status != media::PIPELINE_OK) {
    callback.Run(status, NULL);
    return;
  }

  if (build_observer)
    build_observer->Run(data_source.get());

  callback.Run(status, data_source.get());
}

WebDataSourceFactory::WebDataSourceFactory(
    MessageLoop* render_loop,
    WebKit::WebFrame* frame,
    FactoryFunction factory_function,
    WebDataSourceBuildObserverHack* build_observer)
    : render_loop_(render_loop),
      frame_(frame),
      factory_function_(factory_function),
      build_observer_(build_observer) {
  DCHECK(render_loop_);
  DCHECK(frame_);
  DCHECK(factory_function_);
}

WebDataSourceFactory::~WebDataSourceFactory() {}

void WebDataSourceFactory::Build(const std::string& url,
                                 const BuildCB& callback) {
  scoped_refptr<WebDataSource> data_source =
      factory_function_(render_loop_, frame_);

  data_source->Initialize(
      url, base::Bind(&DataSourceInitDone,
                      build_observer_,
                      callback,
                      data_source));
}

media::DataSourceFactory* WebDataSourceFactory::Clone() const {
  return new WebDataSourceFactory(render_loop_, frame_, factory_function_,
                                  build_observer_);
}

}  // namespace webkit_glue
