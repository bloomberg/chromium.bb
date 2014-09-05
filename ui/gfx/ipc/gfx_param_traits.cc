// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/ipc/gfx_param_traits.h"

#include <string>

#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/rect_f.h"

namespace {

struct SkBitmap_Data {
  // The color type for the bitmap (bits per pixel, etc).
  SkColorType fColorType;

  // The alpha type for the bitmap (opaque, premul, unpremul).
  SkAlphaType fAlphaType;

  // The width of the bitmap in pixels.
  uint32 fWidth;

  // The height of the bitmap in pixels.
  uint32 fHeight;

  void InitSkBitmapDataForTransfer(const SkBitmap& bitmap) {
    const SkImageInfo& info = bitmap.info();
    fColorType = info.fColorType;
    fAlphaType = info.fAlphaType;
    fWidth = info.fWidth;
    fHeight = info.fHeight;
  }

  // Returns whether |bitmap| successfully initialized.
  bool InitSkBitmapFromData(SkBitmap* bitmap,
                            const char* pixels,
                            size_t pixels_size) const {
    if (!bitmap->tryAllocPixels(
            SkImageInfo::Make(fWidth, fHeight, fColorType, fAlphaType)))
      return false;
    if (pixels_size != bitmap->getSize())
      return false;
    memcpy(bitmap->getPixels(), pixels, pixels_size);
    return true;
  }
};

}  // namespace

namespace IPC {

void ParamTraits<gfx::Point>::Write(Message* m, const gfx::Point& p) {
  m->WriteInt(p.x());
  m->WriteInt(p.y());
}

bool ParamTraits<gfx::Point>::Read(const Message* m, PickleIterator* iter,
                                   gfx::Point* r) {
  int x, y;
  if (!m->ReadInt(iter, &x) ||
      !m->ReadInt(iter, &y))
    return false;
  r->set_x(x);
  r->set_y(y);
  return true;
}

void ParamTraits<gfx::Point>::Log(const gfx::Point& p, std::string* l) {
  l->append(base::StringPrintf("(%d, %d)", p.x(), p.y()));
}

void ParamTraits<gfx::PointF>::Write(Message* m, const gfx::PointF& v) {
  ParamTraits<float>::Write(m, v.x());
  ParamTraits<float>::Write(m, v.y());
}

bool ParamTraits<gfx::PointF>::Read(const Message* m,
                                      PickleIterator* iter,
                                      gfx::PointF* r) {
  float x, y;
  if (!ParamTraits<float>::Read(m, iter, &x) ||
      !ParamTraits<float>::Read(m, iter, &y))
    return false;
  r->set_x(x);
  r->set_y(y);
  return true;
}

void ParamTraits<gfx::PointF>::Log(const gfx::PointF& v, std::string* l) {
  l->append(base::StringPrintf("(%f, %f)", v.x(), v.y()));
}

void ParamTraits<gfx::Size>::Write(Message* m, const gfx::Size& p) {
  DCHECK_GE(p.width(), 0);
  DCHECK_GE(p.height(), 0);
  int values[2] = { p.width(), p.height() };
  m->WriteBytes(&values, sizeof(int) * 2);
}

bool ParamTraits<gfx::Size>::Read(const Message* m,
                                  PickleIterator* iter,
                                  gfx::Size* r) {
  const char* char_values;
  if (!m->ReadBytes(iter, &char_values, sizeof(int) * 2))
    return false;
  const int* values = reinterpret_cast<const int*>(char_values);
  if (values[0] < 0 || values[1] < 0)
    return false;
  r->set_width(values[0]);
  r->set_height(values[1]);
  return true;
}

void ParamTraits<gfx::Size>::Log(const gfx::Size& p, std::string* l) {
  l->append(base::StringPrintf("(%d, %d)", p.width(), p.height()));
}

void ParamTraits<gfx::SizeF>::Write(Message* m, const gfx::SizeF& p) {
  float values[2] = { p.width(), p.height() };
  m->WriteBytes(&values, sizeof(float) * 2);
}

bool ParamTraits<gfx::SizeF>::Read(const Message* m,
                                   PickleIterator* iter,
                                   gfx::SizeF* r) {
  const char* char_values;
  if (!m->ReadBytes(iter, &char_values, sizeof(float) * 2))
    return false;
  const float* values = reinterpret_cast<const float*>(char_values);
  r->set_width(values[0]);
  r->set_height(values[1]);
  return true;
}

void ParamTraits<gfx::SizeF>::Log(const gfx::SizeF& p, std::string* l) {
  l->append(base::StringPrintf("(%f, %f)", p.width(), p.height()));
}

void ParamTraits<gfx::Vector2d>::Write(Message* m, const gfx::Vector2d& p) {
  int values[2] = { p.x(), p.y() };
  m->WriteBytes(&values, sizeof(int) * 2);
}

bool ParamTraits<gfx::Vector2d>::Read(const Message* m,
                                      PickleIterator* iter,
                                      gfx::Vector2d* r) {
  const char* char_values;
  if (!m->ReadBytes(iter, &char_values, sizeof(int) * 2))
    return false;
  const int* values = reinterpret_cast<const int*>(char_values);
  r->set_x(values[0]);
  r->set_y(values[1]);
  return true;
}

void ParamTraits<gfx::Vector2d>::Log(const gfx::Vector2d& v, std::string* l) {
  l->append(base::StringPrintf("(%d, %d)", v.x(), v.y()));
}

void ParamTraits<gfx::Vector2dF>::Write(Message* m, const gfx::Vector2dF& p) {
  float values[2] = { p.x(), p.y() };
  m->WriteBytes(&values, sizeof(float) * 2);
}

bool ParamTraits<gfx::Vector2dF>::Read(const Message* m,
                                      PickleIterator* iter,
                                      gfx::Vector2dF* r) {
  const char* char_values;
  if (!m->ReadBytes(iter, &char_values, sizeof(float) * 2))
    return false;
  const float* values = reinterpret_cast<const float*>(char_values);
  r->set_x(values[0]);
  r->set_y(values[1]);
  return true;
}

void ParamTraits<gfx::Vector2dF>::Log(const gfx::Vector2dF& v, std::string* l) {
  l->append(base::StringPrintf("(%f, %f)", v.x(), v.y()));
}

void ParamTraits<gfx::Rect>::Write(Message* m, const gfx::Rect& p) {
  int values[4] = { p.x(), p.y(), p.width(), p.height() };
  m->WriteBytes(&values, sizeof(int) * 4);
}

bool ParamTraits<gfx::Rect>::Read(const Message* m,
                                  PickleIterator* iter,
                                  gfx::Rect* r) {
  const char* char_values;
  if (!m->ReadBytes(iter, &char_values, sizeof(int) * 4))
    return false;
  const int* values = reinterpret_cast<const int*>(char_values);
  if (values[2] < 0 || values[3] < 0)
    return false;
  r->SetRect(values[0], values[1], values[2], values[3]);
  return true;
}

void ParamTraits<gfx::Rect>::Log(const gfx::Rect& p, std::string* l) {
  l->append(base::StringPrintf("(%d, %d, %d, %d)", p.x(), p.y(),
                               p.width(), p.height()));
}

void ParamTraits<gfx::RectF>::Write(Message* m, const gfx::RectF& p) {
  float values[4] = { p.x(), p.y(), p.width(), p.height() };
  m->WriteBytes(&values, sizeof(float) * 4);
}

bool ParamTraits<gfx::RectF>::Read(const Message* m,
                                   PickleIterator* iter,
                                   gfx::RectF* r) {
  const char* char_values;
  if (!m->ReadBytes(iter, &char_values, sizeof(float) * 4))
    return false;
  const float* values = reinterpret_cast<const float*>(char_values);
  r->SetRect(values[0], values[1], values[2], values[3]);
  return true;
}

void ParamTraits<gfx::RectF>::Log(const gfx::RectF& p, std::string* l) {
  l->append(base::StringPrintf("(%f, %f, %f, %f)", p.x(), p.y(),
                               p.width(), p.height()));
}

void ParamTraits<SkBitmap>::Write(Message* m, const SkBitmap& p) {
  size_t fixed_size = sizeof(SkBitmap_Data);
  SkBitmap_Data bmp_data;
  bmp_data.InitSkBitmapDataForTransfer(p);
  m->WriteData(reinterpret_cast<const char*>(&bmp_data),
               static_cast<int>(fixed_size));
  size_t pixel_size = p.getSize();
  SkAutoLockPixels p_lock(p);
  m->WriteData(reinterpret_cast<const char*>(p.getPixels()),
               static_cast<int>(pixel_size));
}

bool ParamTraits<SkBitmap>::Read(const Message* m,
                                 PickleIterator* iter,
                                 SkBitmap* r) {
  const char* fixed_data;
  int fixed_data_size = 0;
  if (!m->ReadData(iter, &fixed_data, &fixed_data_size) ||
     (fixed_data_size <= 0)) {
    NOTREACHED();
    return false;
  }
  if (fixed_data_size != sizeof(SkBitmap_Data))
    return false;  // Message is malformed.

  const char* variable_data;
  int variable_data_size = 0;
  if (!m->ReadData(iter, &variable_data, &variable_data_size) ||
     (variable_data_size < 0)) {
    NOTREACHED();
    return false;
  }
  const SkBitmap_Data* bmp_data =
      reinterpret_cast<const SkBitmap_Data*>(fixed_data);
  return bmp_data->InitSkBitmapFromData(r, variable_data, variable_data_size);
}

void ParamTraits<SkBitmap>::Log(const SkBitmap& p, std::string* l) {
  l->append("<SkBitmap>");
}

}  // namespace IPC
