// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia/ext/skia_utils_base.h"

#include <stdint.h>

#include "base/pickle.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkEncodedImageFormat.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkSerialProcs.h"

namespace skia {

bool ReadSkString(base::PickleIterator* iter, SkString* str) {
  int reply_length;
  const char* reply_text;

  if (!iter->ReadData(&reply_text, &reply_length))
    return false;

  if (str)
    str->set(reply_text, reply_length);
  return true;
}

bool ReadSkFontIdentity(base::PickleIterator* iter,
                        SkFontConfigInterface::FontIdentity* identity) {
  uint32_t reply_id;
  uint32_t reply_ttcIndex;
  int reply_length;
  const char* reply_text;

  if (!iter->ReadUInt32(&reply_id) ||
      !iter->ReadUInt32(&reply_ttcIndex) ||
      !iter->ReadData(&reply_text, &reply_length))
    return false;

  if (identity) {
    identity->fID = reply_id;
    identity->fTTCIndex = reply_ttcIndex;
    identity->fString.set(reply_text, reply_length);
  }
  return true;
}

bool ReadSkFontStyle(base::PickleIterator* iter, SkFontStyle* style) {
  uint16_t reply_weight;
  uint16_t reply_width;
  uint16_t reply_slant;

  if (!iter->ReadUInt16(&reply_weight) ||
      !iter->ReadUInt16(&reply_width) ||
      !iter->ReadUInt16(&reply_slant))
    return false;

  if (style) {
    *style = SkFontStyle(reply_weight,
                         reply_width,
                         static_cast<SkFontStyle::Slant>(reply_slant));
  }
  return true;
}

void WriteSkString(base::Pickle* pickle, const SkString& str) {
  pickle->WriteData(str.c_str(), str.size());
}

void WriteSkFontIdentity(base::Pickle* pickle,
                         const SkFontConfigInterface::FontIdentity& identity) {
  pickle->WriteUInt32(identity.fID);
  pickle->WriteUInt32(identity.fTTCIndex);
  WriteSkString(pickle, identity.fString);
}

void WriteSkFontStyle(base::Pickle* pickle, SkFontStyle style) {
  pickle->WriteUInt16(style.weight());
  pickle->WriteUInt16(style.width());
  pickle->WriteUInt16(style.slant());
}

sk_sp<SkData> ValidatingSerializeFlattenable(SkFlattenable* flattenable) {
  SkSerialProcs procs;
  procs.fImageProc = [](SkImage* image, void*) {
    // We choose to not trust natively encoded images, but we are fine to force
    // a "clean" encoded version for transport.
    return image->encodeToData(SkEncodedImageFormat::kPNG, 100);
  };
  return flattenable->serialize(&procs);
}

SkFlattenable* ValidatingDeserializeFlattenable(const void* data,
                                                size_t size,
                                                SkFlattenable::Type type) {
  SkDeserialProcs procs;
  procs.fImageProc = [](const void* data, size_t length, void*) {
    // Our custom encode is standard, so we can just call Skia to deserialize
    return SkImage::MakeFromEncoded(SkData::MakeWithCopy(data, length));
  };
  return SkFlattenable::Deserialize(type, data, size, &procs).release();
}

}  // namespace skia
