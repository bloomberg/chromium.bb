// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/image_resource_fetcher.h"

#include "base/callback.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "ui/gfx/size.h"
#include "webkit/glue/image_decoder.h"
#include "third_party/skia/include/core/SkBitmap.h"

using WebKit::WebFrame;
using WebKit::WebURLRequest;
using WebKit::WebURLResponse;

namespace webkit_glue {

ImageResourceFetcher::ImageResourceFetcher(
    const GURL& image_url,
    WebFrame* frame,
    int id,
    int image_size,
    WebURLRequest::TargetType target_type,
    Callback* callback)
    : callback_(callback),
      id_(id),
      image_url_(image_url),
      image_size_(image_size) {
  fetcher_.reset(new ResourceFetcher(
      image_url, frame, target_type,
      NewCallback(this, &ImageResourceFetcher::OnURLFetchComplete)));
}

ImageResourceFetcher::~ImageResourceFetcher() {
  if (!fetcher_->completed())
    fetcher_->Cancel();
}

void ImageResourceFetcher::OnURLFetchComplete(
    const WebURLResponse& response,
    const std::string& data) {
  SkBitmap bitmap;
  if (!response.isNull() && response.httpStatusCode() == 200) {
    // Request succeeded, try to convert it to an image.
    ImageDecoder decoder(gfx::Size(image_size_, image_size_));
    bitmap = decoder.Decode(
        reinterpret_cast<const unsigned char*>(data.data()), data.size());
  } // else case:
    // If we get here, it means no image from server or couldn't decode the
    // response as an image. The delegate will see a null image, indicating
    // that an error occurred.

  // Take care to clear callback_ before running the callback as it may lead to
  // our destruction.
  scoped_ptr<Callback> callback;
  callback.swap(callback_);
  callback->Run(this, bitmap);
}

}  // namespace webkit_glue
