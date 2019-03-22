// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_CONTENT_METADATA_H_
#define COMPONENTS_FEED_CORE_CONTENT_METADATA_H_

#include <string>

#include "base/time/time.h"

namespace feed {

// Native counterpart of ContentMetadata.java.
struct ContentMetadata {
  ContentMetadata();
  ContentMetadata(const ContentMetadata&);
  ContentMetadata(ContentMetadata&&);
  ~ContentMetadata();

  // A link to the underlying article.
  std::string url;

  // The title of the article.
  std::string title;

  // The time the article was published, independent of when the device was
  // downloaded this metadata.
  base::Time time_published;

  // A link to a thumbnail.
  std::string image_url;

  // The name of the publisher.
  std::string publisher;

  // A link to the favicon for the publisher's domain.
  std::string favicon_url;

  // A short description of the article, human readable.
  std::string snippet;
};

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_CONTENT_METADATA_H_
