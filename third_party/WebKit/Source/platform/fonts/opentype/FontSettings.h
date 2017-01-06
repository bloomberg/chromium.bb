// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FontSettings_h
#define FontSettings_h

#include "platform/PlatformExport.h"
#include "wtf/Allocator.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/RefPtr.h"
#include "wtf/Vector.h"
#include "wtf/text/AtomicString.h"

namespace blink {

uint32_t atomicStringToFourByteTag(AtomicString tag);

template <typename T>
class FontTagValuePair {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();

 public:
  FontTagValuePair(const AtomicString& tag, T value)
      : m_tag(tag), m_value(value){};
  bool operator==(const FontTagValuePair& other) const {
    return m_tag == other.m_tag && m_value == other.m_value;
  };

  const AtomicString& tag() const { return m_tag; }
  T value() const { return m_value; }

 private:
  AtomicString m_tag;
  const T m_value;
};

template <typename T>
class FontSettings {
  WTF_MAKE_NONCOPYABLE(FontSettings);

 public:
  void append(const T& feature) { m_list.push_back(feature); }
  size_t size() const { return m_list.size(); }
  const T& operator[](int index) const { return m_list[index]; }
  const T& at(size_t index) const { return m_list.at(index); }
  bool operator==(const FontSettings& other) const {
    return m_list == other.m_list;
  };

 protected:
  FontSettings(){};

 private:
  Vector<T, 0> m_list;
};

using FontFeature = FontTagValuePair<int>;
using FontVariationAxis = FontTagValuePair<float>;

class PLATFORM_EXPORT FontFeatureSettings
    : public FontSettings<FontFeature>,
      public RefCounted<FontFeatureSettings> {
  WTF_MAKE_NONCOPYABLE(FontFeatureSettings);

 public:
  static PassRefPtr<FontFeatureSettings> create() {
    return adoptRef(new FontFeatureSettings());
  }

 private:
  FontFeatureSettings() = default;
};

class PLATFORM_EXPORT FontVariationSettings
    : public FontSettings<FontVariationAxis>,
      public RefCounted<FontVariationSettings> {
  WTF_MAKE_NONCOPYABLE(FontVariationSettings);

 public:
  static PassRefPtr<FontVariationSettings> create() {
    return adoptRef(new FontVariationSettings());
  }

  unsigned hash() const;

 private:
  FontVariationSettings() = default;
};

}  // namespace blink

#endif  // FontSettings_h
