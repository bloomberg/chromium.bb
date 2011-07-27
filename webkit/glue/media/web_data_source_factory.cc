// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/media/web_data_source_factory.h"

#include <set>

#include "base/bind.h"
#include "base/logging.h"
#include "base/synchronization/lock.h"

namespace webkit_glue {

// The state shared between a factory and its clones. The pointers
// passed to the original factory are only valid for the lifetime of
// that factory. This object allows the clones to detect when the original
// factory goes away and prevents them from dereferencing invalid pointers.
class WebDataSourceFactory::SharedState
    : public base::RefCountedThreadSafe<SharedState> {
 public:
  SharedState(MessageLoop* render_loop, WebKit::WebFrame* frame,
              FactoryFunction factory_function,
              WebDataSourceBuildObserverHack* build_observer);
  virtual ~SharedState();

  // Start building a source for |url|.
  void Build(const std::string& url, const BuildCB& callback);

  // Clears state so pending build's are aborted and new Build() calls
  // report an error.
  void Clear();

 private:
  // Called when initialization for a source completes.
  void InitDone(
      const media::DataSourceFactory::BuildCB& callback,
      const scoped_refptr<WebDataSource>& data_source,
      media::PipelineStatus status);

  base::Lock lock_;
  MessageLoop* render_loop_;
  WebKit::WebFrame* frame_;
  FactoryFunction factory_function_;

  WebDataSourceBuildObserverHack* build_observer_;
  typedef std::set<scoped_refptr<WebDataSource> > SourceSet;
  SourceSet pending_sources_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(SharedState);
};

WebDataSourceFactory::WebDataSourceFactory(
    MessageLoop* render_loop,
    WebKit::WebFrame* frame,
    FactoryFunction factory_function,
    WebDataSourceBuildObserverHack* build_observer)
    : is_clone_(false),
      shared_state_(new SharedState(render_loop, frame, factory_function,
                                    build_observer)) {
  DCHECK(render_loop);
  DCHECK(frame);
  DCHECK(!factory_function.is_null());
}

WebDataSourceFactory::WebDataSourceFactory(
    const scoped_refptr<SharedState>& shared_state)
    : is_clone_(true),
      shared_state_(shared_state) {
  DCHECK(shared_state);
}

WebDataSourceFactory::~WebDataSourceFactory() {
  if (!is_clone_)
    shared_state_->Clear();
}

void WebDataSourceFactory::Build(const std::string& url,
                                 const BuildCB& callback) {
  shared_state_->Build(url, callback);
}

media::DataSourceFactory* WebDataSourceFactory::Clone() const {
  return new WebDataSourceFactory(shared_state_);
}

WebDataSourceFactory::SharedState::SharedState(
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
  DCHECK(!factory_function_.is_null());
}

WebDataSourceFactory::SharedState::~SharedState() {}

void WebDataSourceFactory::SharedState::Clear() {
  base::AutoLock auto_lock(lock_);
  render_loop_ = NULL;
  frame_ = NULL;
  factory_function_.Reset();
  build_observer_ = NULL;

  for (SourceSet::iterator itr = pending_sources_.begin();
       itr != pending_sources_.end();
       ++itr) {
    (*itr)->CancelInitialize();
  }

  pending_sources_.clear();
}

void WebDataSourceFactory::SharedState::Build(const std::string& url,
                                              const BuildCB& callback) {
  scoped_refptr<WebDataSource> data_source;
  media::PipelineStatusCB init_cb;
  {
    base::AutoLock auto_lock(lock_);

    if (!factory_function_.is_null())
      data_source = factory_function_.Run(render_loop_, frame_);

    if (data_source.get()) {
      pending_sources_.insert(data_source.get());
      init_cb = base::Bind(&SharedState::InitDone, this, callback, data_source);
    }
  }

  if (init_cb.is_null()) {
    callback.Run(media::PIPELINE_ERROR_URL_NOT_FOUND, NULL);
    return;
  }

  data_source->Initialize(url, init_cb);
}

void WebDataSourceFactory::SharedState::InitDone(
    const media::DataSourceFactory::BuildCB& callback,
    const scoped_refptr<WebDataSource>& data_source,
    media::PipelineStatus status) {
  {
    base::AutoLock auto_lock(lock_);

    // Make sure we don't get one of these calls after Clear() has been called.
    DCHECK(!factory_function_.is_null());

    pending_sources_.erase(data_source.get());

    if (build_observer_ && status == media::PIPELINE_OK)
      build_observer_->Run(data_source.get());
  }

  callback.Run(status, data_source.get());
}

}  // namespace webkit_glue
