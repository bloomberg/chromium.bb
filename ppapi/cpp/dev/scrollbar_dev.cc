// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "ppapi/cpp/dev/scrollbar_dev.h"

#include "ppapi/cpp/common.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/module_impl.h"
#include "ppapi/cpp/rect.h"

namespace {

DeviceFuncs<PPB_Scrollbar_Dev> scrollbar_f(PPB_SCROLLBAR_DEV_INTERFACE);

}  // namespace

namespace pp {

Scrollbar_Dev::Scrollbar_Dev(PP_Resource resource) : Widget_Dev(resource) {
}

Scrollbar_Dev::Scrollbar_Dev(const Instance& instance, bool vertical) {
  if (!scrollbar_f)
    return;
  PassRefFromConstructor(scrollbar_f->Create(instance.pp_instance(),
                                             BoolToPPBool(vertical)));
}

Scrollbar_Dev::Scrollbar_Dev(const Scrollbar_Dev& other)
    : Widget_Dev(other) {
}

Scrollbar_Dev& Scrollbar_Dev::operator=(const Scrollbar_Dev& other) {
  Scrollbar_Dev copy(other);
  swap(copy);
  return *this;
}

void Scrollbar_Dev::swap(Scrollbar_Dev& other) {
  Resource::swap(other);
}

uint32_t Scrollbar_Dev::GetThickness() {
  if (!scrollbar_f)
    return 0;
  return scrollbar_f->GetThickness();
}

uint32_t Scrollbar_Dev::GetValue() {
  if (!scrollbar_f)
    return 0;
  return scrollbar_f->GetValue(pp_resource());
}

void Scrollbar_Dev::SetValue(uint32_t value) {
  if (scrollbar_f)
    scrollbar_f->SetValue(pp_resource(), value);
}

void Scrollbar_Dev::SetDocumentSize(uint32_t size) {
  if (scrollbar_f)
    scrollbar_f->SetDocumentSize(pp_resource(), size);
}

void Scrollbar_Dev::SetTickMarks(const Rect* tick_marks, uint32_t count) {
  if (!scrollbar_f)
    return;

  std::vector<PP_Rect> temp;
  temp.resize(count);
  for (uint32_t i = 0; i < count; ++i)
    temp[i] = tick_marks[i];

  scrollbar_f->SetTickMarks(pp_resource(), count ? &temp[0] : NULL, count);
}

void Scrollbar_Dev::ScrollBy(PP_ScrollBy_Dev unit, int32_t multiplier) {
  if (scrollbar_f)
    scrollbar_f->ScrollBy(pp_resource(), unit, multiplier);
}

}  // namespace pp
