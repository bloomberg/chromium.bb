// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_MEDIA_ACTIVE_LOADER_H_
#define WEBKIT_MEDIA_ACTIVE_LOADER_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"

namespace WebKit {
class WebURLLoader;
}

namespace webkit_media {

class BufferedResourceLoader;

// Wraps an active WebURLLoader with some additional state and maintains a
// reference to its parent.
//
// Handles deferring and deletion of loaders.
class ActiveLoader {
 public:
  // Creates an ActiveLoader with a reference to its parent. The initial state
  // assumes that the loader has started loading and has not been deferred.
  //
  // ActiveLoader takes ownership of |loader|.
  ActiveLoader(const scoped_refptr<BufferedResourceLoader>& parent,
               WebKit::WebURLLoader* loader);
  ~ActiveLoader();

  // Starts or stops deferring the resource load.
  void SetDeferred(bool deferred);
  bool deferred() { return deferred_; }

  // Cancels the URL loader associated with this object.
  void Cancel();

 private:
  friend class BufferedDataSourceTest;

  scoped_refptr<BufferedResourceLoader> parent_;
  scoped_ptr<WebKit::WebURLLoader> loader_;
  bool deferred_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ActiveLoader);
};

}  // namespace webkit_media

#endif  // WEBKIT_MEDIA_ACTIVE_LOADER_H_
