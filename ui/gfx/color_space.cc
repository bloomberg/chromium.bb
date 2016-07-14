// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/color_space.h"

#include <map>

#include "base/lazy_instance.h"
#include "base/synchronization/lock.h"

namespace gfx {

namespace {
static const size_t kMinProfileLength = 128;
static const size_t kMaxProfileLength = 4 * 1024 * 1024;
}  // namespace

// The structure used to look up GlobalData structures.
struct ColorSpace::Key {
  Key(ColorSpace::Type type, const std::vector<char>& icc_profile)
      : type(type), icc_profile(icc_profile) {}

  bool operator<(const Key& other) const {
    if (type < other.type)
      return true;
    if (type > other.type)
      return false;
    if (type != Type::ICC_PROFILE)
      return false;

    if (icc_profile.size() < other.icc_profile.size())
      return true;
    if (icc_profile.size() > other.icc_profile.size())
      return false;
    for (size_t i = 0; i < icc_profile.size(); ++i) {
      if (icc_profile[i] < other.icc_profile[i])
        return true;
      if (icc_profile[i] > other.icc_profile[i])
        return false;
    }
    return false;
  }

  ColorSpace::Type type;
  const std::vector<char> icc_profile;
};

// Because this structure is shared across gfx::ColorSpace objects on
// different threads, it needs to be thread-safe.
class ColorSpace::GlobalData
    : public base::RefCountedThreadSafe<ColorSpace::GlobalData> {
 public:
  static void Get(const Key& key, scoped_refptr<GlobalData>* value) {
    base::AutoLock lock(map_lock_.Get());
    auto insert_result = map_.Get().insert(std::make_pair(key, nullptr));
    if (insert_result.second)
      insert_result.first->second = new GlobalData(key, insert_result.first);
    *value = make_scoped_refptr(insert_result.first->second);
  }

  const std::vector<char>& GetICCProfile() const { return icc_profile_; }

 private:
  friend class base::RefCountedThreadSafe<GlobalData>;

  GlobalData(const Key& key, std::map<Key, GlobalData*>::iterator iterator)
      : iterator_(iterator) {
    // TODO: Compute the ICC profile for named color spaces.
    if (key.type == Type::ICC_PROFILE)
      icc_profile_ = key.icc_profile;
  }
  ~GlobalData() {
    base::AutoLock lock(map_lock_.Get());
    map_.Get().erase(iterator_);
  }

  std::vector<char> icc_profile_;

  // In order to remove |this| from |map_| when its last reference goes away,
  // keep in |iterator_| the corresponding iterator in |map_|.
  std::map<Key, GlobalData*>::iterator iterator_;
  // The |map_| tracks the existing GlobalData instances, which are owned by
  // ColorSpace instances. Note that |map_| must be leaky because GlobalData
  // instances will reach back into it at unpredictable times during tear-down.
  static base::LazyInstance<std::map<Key, GlobalData*>>::Leaky map_;
  static base::LazyInstance<base::Lock>::Leaky map_lock_;
};

base::LazyInstance<std::map<ColorSpace::Key, ColorSpace::GlobalData*>>::Leaky
    ColorSpace::GlobalData::map_ = LAZY_INSTANCE_INITIALIZER;
base::LazyInstance<base::Lock>::Leaky
    ColorSpace::GlobalData::map_lock_ = LAZY_INSTANCE_INITIALIZER;

ColorSpace::ColorSpace() = default;
ColorSpace::ColorSpace(ColorSpace&& other) = default;
ColorSpace::ColorSpace(const ColorSpace& other) = default;
ColorSpace& ColorSpace::operator=(const ColorSpace& other) = default;
ColorSpace::~ColorSpace() = default;

bool ColorSpace::operator==(const ColorSpace& other) const {
  if (type_ == Type::ICC_PROFILE && other.type_ == Type::ICC_PROFILE)
    return global_data_ == other.global_data_;
  return type_ == other.type_;
}

bool ColorSpace::operator<(const ColorSpace& other) const {
  // Note that this does a pointer-based comparision.
  if (type_ == Type::ICC_PROFILE && other.type_ == Type::ICC_PROFILE)
    return global_data_.get() < other.global_data_.get();
  return type_ < other.type_;
}

// static
ColorSpace ColorSpace::FromICCProfile(const std::vector<char>& icc_profile) {
  ColorSpace color_space;
  if (IsValidProfileLength(icc_profile.size())) {
    color_space.type_ = Type::ICC_PROFILE;
    Key key(Type::ICC_PROFILE, icc_profile);
    GlobalData::Get(key, &color_space.global_data_);
  }
  return color_space;
}

#if !defined(OS_WIN) && !defined(OS_MACOSX) && !defined(USE_X11)
// static
ColorSpace ColorSpace::FromBestMonitor() {
  return ColorSpace();
}
#endif

const std::vector<char>& ColorSpace::GetICCProfile() const {
  if (!global_data_) {
    Key key(type_, std::vector<char>());
    GlobalData::Get(key, &global_data_);
  }
  return global_data_->GetICCProfile();
}

// static
bool ColorSpace::IsValidProfileLength(size_t length) {
  return length >= kMinProfileLength && length <= kMaxProfileLength;
}

}  // namespace gfx
