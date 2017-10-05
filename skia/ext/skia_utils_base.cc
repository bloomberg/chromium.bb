// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia/ext/skia_utils_base.h"

#include <stdint.h>

#include "base/pickle.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkWriteBuffer.h"

namespace skia {
namespace {

class CodecDisallowingPixelSerializer : public SkPixelSerializer {
 public:
  CodecDisallowingPixelSerializer() = default;
  ~CodecDisallowingPixelSerializer() override = default;

 protected:
  bool onUseEncodedData(const void* data, size_t len) override {
    CHECK(false) << "We should not have codec backed image filters";
    return false;
  }

  SkData* onEncode(const SkPixmap&) override { return nullptr; }
};

}  // namespace

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
  SkBinaryWriteBuffer writer;
  writer.setPixelSerializer(sk_make_sp<CodecDisallowingPixelSerializer>());
  writer.writeFlattenable(flattenable);
  size_t size = writer.bytesWritten();
  auto data = SkData::MakeUninitialized(size);
  writer.writeToMemory(data->writable_data());
  return data;
}

}  // namespace skia
