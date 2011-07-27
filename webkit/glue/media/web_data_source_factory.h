// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_MEDIA_BUFFERED_DATA_SOURCE_FACTORY_H_
#define WEBKIT_GLUE_MEDIA_BUFFERED_DATA_SOURCE_FACTORY_H_

#include "media/base/filter_factories.h"
#include "webkit/glue/media/web_data_source.h"

class MessageLoop;

namespace WebKit {
class WebFrame;
}

namespace webkit_glue {

class WebDataSourceFactory : public media::DataSourceFactory {
 public:
  typedef base::Callback<WebDataSource*(MessageLoop*,
                                        WebKit::WebFrame*)> FactoryFunction;

  WebDataSourceFactory(MessageLoop* render_loop, WebKit::WebFrame* frame,
                       FactoryFunction factory_function,
                       WebDataSourceBuildObserverHack* build_observer);

  virtual ~WebDataSourceFactory();

  // DataSourceFactory methods.
  virtual void Build(const std::string& url, const BuildCB& callback) OVERRIDE;
  virtual media::DataSourceFactory* Clone() const OVERRIDE;

 private:
  class SharedState;

  // Constructor used by Clone().
  explicit WebDataSourceFactory(
      const scoped_refptr<SharedState>& shared_state);

  // Keeps track of whether this is a factory created by Clone().
  bool is_clone_;

  // State shared between original factory and its clones.
  scoped_refptr<SharedState> shared_state_;

  DISALLOW_COPY_AND_ASSIGN(WebDataSourceFactory);
};

}  // namespace webkit_glue

#endif  // WEBKIT_GLUE_MEDIA_BUFFERED_DATA_SOURCE_FACTORY_H_
