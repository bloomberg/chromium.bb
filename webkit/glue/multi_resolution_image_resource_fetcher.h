// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_MULTI_RESOLUTION_IMAGE_RESOURCE_FETCHER_H_
#define WEBKIT_GLUE_MULTI_RESOLUTION_IMAGE_RESOURCE_FETCHER_H_

#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "webkit/glue/resource_fetcher.h"
#include "webkit/glue/webkit_glue_export.h"

class SkBitmap;

namespace webkit_glue {

// A resource fetcher that returns all (differently-sized) frames in
// an image. Useful for favicons.
class MultiResolutionImageResourceFetcher{
 public:
  typedef base::Callback<void(MultiResolutionImageResourceFetcher*,
                              const std::vector<SkBitmap>&)> Callback;

  WEBKIT_GLUE_EXPORT MultiResolutionImageResourceFetcher(
      const GURL& image_url,
      WebKit::WebFrame* frame,
      int id,
      WebKit::WebURLRequest::TargetType target_type,
      const Callback& callback);

  WEBKIT_GLUE_EXPORT virtual ~MultiResolutionImageResourceFetcher();

  // URL of the image we're downloading.
  const GURL& image_url() const { return image_url_; }

  // Unique identifier for the request.
  int id() const { return id_; }

 private:
  // ResourceFetcher::Callback. Decodes the image and invokes callback_.
  void OnURLFetchComplete(const WebKit::WebURLResponse& response,
                          const std::string& data);

  Callback callback_;

  // Unique identifier for the request.
  const int id_;

  // URL of the image.
  const GURL image_url_;

  // Does the actual download.
  scoped_ptr<ResourceFetcher> fetcher_;

  DISALLOW_COPY_AND_ASSIGN(MultiResolutionImageResourceFetcher);
};

}  // namespace webkit_glue

#endif  // WEBKIT_GLUE_MULTI_RESOLUTION_IMAGE_RESOURCE_FETCHER_H_
