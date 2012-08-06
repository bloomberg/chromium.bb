// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/multi_resolution_image_resource_fetcher.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "ui/gfx/size.h"
#include "webkit/glue/image_decoder.h"
#include "third_party/skia/include/core/SkBitmap.h"

using WebKit::WebFrame;
using WebKit::WebURLRequest;
using WebKit::WebURLResponse;

namespace webkit_glue {

MultiResolutionImageResourceFetcher::MultiResolutionImageResourceFetcher(
    const GURL& image_url,
    WebFrame* frame,
    int id,
    WebURLRequest::TargetType target_type,
    const Callback& callback)
    : callback_(callback),
      id_(id),
      image_url_(image_url) {
  fetcher_.reset(new ResourceFetcher(
      image_url, frame, target_type,
      base::Bind(&MultiResolutionImageResourceFetcher::OnURLFetchComplete,
                 base::Unretained(this))));
}

MultiResolutionImageResourceFetcher::~MultiResolutionImageResourceFetcher() {
  if (!fetcher_->completed())
    fetcher_->Cancel();
}

void MultiResolutionImageResourceFetcher::OnURLFetchComplete(
    const WebURLResponse& response,
    const std::string& data) {
  std::vector<SkBitmap> bitmaps;
  if (!response.isNull() && response.httpStatusCode() == 200) {
    // Request succeeded, try to convert it to an image.
    bitmaps = ImageDecoder::DecodeAll(
        reinterpret_cast<const unsigned char*>(data.data()), data.size());
  } // else case:
    // If we get here, it means no image from server or couldn't decode the
    // response as an image. The delegate will see an empty vector.

  // Take a reference to the callback as running the callback may lead to our
  // destruction.
  Callback callback = callback_;
  callback.Run(this, bitmaps);
}

}  // namespace webkit_glue
