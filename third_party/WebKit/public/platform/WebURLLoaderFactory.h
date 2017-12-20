// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebURLLoaderFactory_h
#define WebURLLoaderFactory_h

#include "base/memory/scoped_refptr.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace blink {

class WebURLLoader;
class WebURLRequest;

// An abstract interface to create a URLLoader. It is expected that each
// loading context holds its own per-context WebURLLoaderFactory.
class WebURLLoaderFactory {
 public:
  virtual ~WebURLLoaderFactory() = default;

  // Returns a new WebURLLoader instance. This should internally choose
  // the most appropriate URLLoaderFactory implementation.
  virtual std::unique_ptr<WebURLLoader> CreateURLLoader(
      const WebURLRequest&,
      scoped_refptr<base::SingleThreadTaskRunner>) = 0;
};

}  // namespace blink

#endif  // WebURLLoaderFactory_h
