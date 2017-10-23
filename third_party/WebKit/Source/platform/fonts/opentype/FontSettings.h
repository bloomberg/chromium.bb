// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FontSettings_h
#define FontSettings_h

#include "platform/PlatformExport.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/RefCounted.h"
#include "platform/wtf/RefPtr.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/text/AtomicString.h"
#include "platform/wtf/text/StringBuilder.h"

namespace blink {

uint32_t AtomicStringToFourByteTag(AtomicString tag);

template <typename T>
class FontTagValuePair {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();

 public:
  FontTagValuePair(const AtomicString& tag, T value)
      : tag_(tag), value_(value){};
  bool operator==(const FontTagValuePair& other) const {
    return tag_ == other.tag_ && value_ == other.value_;
  };

  const AtomicString& Tag() const { return tag_; }
  T Value() const { return value_; }

 private:
  AtomicString tag_;
  const T value_;
};

template <typename T>
class FontSettings {
  WTF_MAKE_NONCOPYABLE(FontSettings);

 public:
  void Append(const T& feature) { list_.push_back(feature); }
  size_t size() const { return list_.size(); }
  const T& operator[](int index) const { return list_[index]; }
  const T& at(size_t index) const { return list_.at(index); }
  bool operator==(const FontSettings& other) const {
    return list_ == other.list_;
  };
  String ToString() const {
    StringBuilder builder;
    size_t num_features = size();
    for (size_t i = 0; i < num_features; ++i) {
      if (i > 0)
        builder.Append(",");
      const AtomicString& tag = at(i).Tag();
      builder.Append(tag);
      builder.Append("=");
      builder.AppendNumber(at(i).Value());
    }
    return builder.ToString();
  }

 protected:
  FontSettings(){};

 private:
  Vector<T, 0> list_;
};

using FontFeature = FontTagValuePair<int>;
using FontVariationAxis = FontTagValuePair<float>;

class PLATFORM_EXPORT FontFeatureSettings
    : public FontSettings<FontFeature>,
      public RefCounted<FontFeatureSettings> {
  WTF_MAKE_NONCOPYABLE(FontFeatureSettings);

 public:
  static scoped_refptr<FontFeatureSettings> Create() {
    return WTF::AdoptRef(new FontFeatureSettings());
  }

 private:
  FontFeatureSettings() = default;
};

class PLATFORM_EXPORT FontVariationSettings
    : public FontSettings<FontVariationAxis>,
      public RefCounted<FontVariationSettings> {
  WTF_MAKE_NONCOPYABLE(FontVariationSettings);

 public:
  static scoped_refptr<FontVariationSettings> Create() {
    return WTF::AdoptRef(new FontVariationSettings());
  }

  unsigned GetHash() const;

 private:
  FontVariationSettings() = default;
};

}  // namespace blink

#endif  // FontSettings_h
