// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_HLS_ITEMS_H_
#define MEDIA_FORMATS_HLS_ITEMS_H_

#include <cstddef>
#include "media/base/media_export.h"
#include "media/formats/hls/parse_context.h"
#include "media/formats/hls/parse_status.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace media {
namespace hls {

// An 'Item' is a lexical item in an HLS manifest which has been determined to
// have some type based on its context, but has yet been fully parsed,
// validated, or undergone variable substitution.

enum class TagKind {
  kUnknown,
  kM3u,
  kXVersion,
  kInf,
  kMaxValue = kInf,
};

// An item which has been determined to of a known or unknown tag type, but not
// a comment.
struct MEDIA_EXPORT TagItem {
  // TODO(crbug.com/1275317): These constructors should be removed
  TagItem(TagKind, SourceString content);
  ~TagItem();
  TagItem(const TagItem&);
  TagItem(TagItem&&);
  TagItem& operator=(const TagItem&);
  TagItem& operator=(TagItem&&);

  TagKind kind;

  // The content of the tag, not including the tag type prefix.
  SourceString content;
};

// A URI. This may be a URI line or a URI appearing within a tag.
struct MEDIA_EXPORT UriItem {
  // TODO(crbug.com/1275317): These constructors should be removed
  explicit UriItem(SourceString content);
  ~UriItem();
  UriItem(const UriItem&);
  UriItem(UriItem&&);
  UriItem& operator=(const UriItem&);
  UriItem& operator=(UriItem&&);

  SourceString content;
};

using GetNextLineItemResult = absl::variant<TagItem, UriItem>;

// Returns the next line-level item from the source text. Automatically skips
// empty lines.
MEDIA_EXPORT ParseStatus::Or<GetNextLineItemResult> GetNextLineItem(
    SourceLineIterator* src);

}  // namespace hls
}  // namespace media

#endif  // MEDIA_FORMATS_HLS_ITEMS_H_
