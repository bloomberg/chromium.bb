// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_exclusion.h"

#include "platform/wtf/text/WTFString.h"

namespace blink {

bool NGExclusion::operator==(const NGExclusion& other) const {
  return std::tie(other.rect, other.type) == std::tie(rect, type);
}

String NGExclusion::ToString() const {
  return String::Format("Rect: %s Type: %d", rect.ToString().Ascii().Data(),
                        type);
}

bool NGExclusion::MaybeCombineWith(const NGExclusion& other) {
  if (other.rect.BlockEndOffset() < rect.BlockEndOffset())
    return false;

  if (other.type != type)
    return false;

  switch (other.type) {
    case NGExclusion::kFloatLeft: {
      if (other.rect.offset == rect.InlineEndBlockStartOffset()) {
        rect.size = {other.rect.InlineSize() + rect.InlineSize(),
                     other.rect.BlockSize()};
        return true;
      }
    }
    case NGExclusion::kFloatRight: {
      if (rect.offset == other.rect.InlineEndBlockStartOffset()) {
        rect.offset = other.rect.offset;
        rect.size = {other.rect.InlineSize() + rect.InlineSize(),
                     other.rect.BlockSize()};
        return true;
      }
    }
    default:
      return false;
  }
}

std::ostream& operator<<(std::ostream& stream, const NGExclusion& value) {
  return stream << value.ToString();
}

NGExclusions::NGExclusions()
    : last_left_float(nullptr), last_right_float(nullptr) {}

NGExclusions::NGExclusions(const NGExclusions& other) {
  for (const auto& exclusion : other.storage)
    Add(*exclusion);
}

void NGExclusions::Add(const NGExclusion& exclusion) {
  storage.push_back(WTF::MakeUnique<NGExclusion>(exclusion));
  if (exclusion.type == NGExclusion::kFloatLeft) {
    last_left_float = storage.rbegin()->get();
  } else if (exclusion.type == NGExclusion::kFloatRight) {
    last_right_float = storage.rbegin()->get();
  }
}

inline NGExclusions& NGExclusions::operator=(const NGExclusions& other) {
  storage.Clear();
  last_left_float = nullptr;
  last_right_float = nullptr;
  for (const auto& exclusion : other.storage)
    Add(*exclusion);
  return *this;
}

}  // namespace blink
