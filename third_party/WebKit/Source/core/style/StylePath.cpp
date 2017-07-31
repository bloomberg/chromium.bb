// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/style/StylePath.h"

#include <memory>
#include "core/css/CSSPathValue.h"
#include "core/svg/SVGPathByteStream.h"
#include "core/svg/SVGPathUtilities.h"
#include "platform/graphics/Path.h"
#include "platform/wtf/PtrUtil.h"

namespace blink {

StylePath::StylePath(std::unique_ptr<SVGPathByteStream> path_byte_stream)
    : byte_stream_(std::move(path_byte_stream)),
      path_length_(std::numeric_limits<float>::quiet_NaN()) {
  DCHECK(byte_stream_);
}

StylePath::~StylePath() {}

RefPtr<StylePath> StylePath::Create(
    std::unique_ptr<SVGPathByteStream> path_byte_stream) {
  return AdoptRef(new StylePath(std::move(path_byte_stream)));
}

StylePath* StylePath::EmptyPath() {
  DEFINE_STATIC_REF(StylePath, empty_path,
                    StylePath::Create(SVGPathByteStream::Create()));
  return empty_path;
}

const Path& StylePath::GetPath() const {
  if (!path_) {
    path_ = WTF::WrapUnique(new Path);
    BuildPathFromByteStream(*byte_stream_, *path_);
  }
  return *path_;
}

float StylePath::length() const {
  if (std::isnan(path_length_))
    path_length_ = GetPath().length();
  return path_length_;
}

bool StylePath::IsClosed() const {
  return GetPath().IsClosed();
}

CSSValue* StylePath::ComputedCSSValue() const {
  return cssvalue::CSSPathValue::Create(const_cast<StylePath*>(this));
}

bool StylePath::operator==(const BasicShape& o) const {
  if (!IsSameType(o))
    return false;
  const StylePath& other = ToStylePath(o);
  return *byte_stream_ == *other.byte_stream_;
}

void StylePath::GetPath(Path&, const FloatRect&) {
  // Callers should use GetPath() overload, which avoids making a copy.
  NOTREACHED();
}

RefPtr<BasicShape> StylePath::Blend(const BasicShape*, double) const {
  // TODO(ericwilligers): Implement animation for offset-path.
  NOTREACHED();
  return nullptr;
}

}  // namespace blink
