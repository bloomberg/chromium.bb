// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/dev/printing_dev.h"

#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/module_impl.h"

namespace pp {

namespace {

static const char kPPPPrintingInterface[] = PPP_PRINTING_DEV_INTERFACE;

uint32_t QuerySupportedFormats(PP_Instance instance) {
  void* object =
      pp::Instance::GetPerInstanceObject(instance, kPPPPrintingInterface);
  if (!object)
    return 0;
  return static_cast<Printing_Dev*>(object)->QuerySupportedPrintOutputFormats();
}

int32_t Begin(PP_Instance instance,
              const struct PP_PrintSettings_Dev* print_settings) {
  void* object =
      pp::Instance::GetPerInstanceObject(instance, kPPPPrintingInterface);
  if (!object)
    return 0;
  return static_cast<Printing_Dev*>(object)->PrintBegin(*print_settings);
}

PP_Resource PrintPages(PP_Instance instance,
                       const struct PP_PrintPageNumberRange_Dev* page_ranges,
                       uint32_t page_range_count) {
  void* object =
      pp::Instance::GetPerInstanceObject(instance, kPPPPrintingInterface);
  if (!object)
    return 0;
  return static_cast<Printing_Dev*>(object)->PrintPages(
      page_ranges, page_range_count).detach();
}

void End(PP_Instance instance) {
  void* object =
      pp::Instance::GetPerInstanceObject(instance, kPPPPrintingInterface);
  if (object)
    static_cast<Printing_Dev*>(object)->PrintEnd();
}

const PPP_Printing_Dev ppp_printing = {
  &QuerySupportedFormats,
  &Begin,
  &PrintPages,
  &End
};

}  // namespace

Printing_Dev::Printing_Dev(Instance* instance)
      : associated_instance_(instance) {
  pp::Module::Get()->AddPluginInterface(kPPPPrintingInterface, &ppp_printing);
  associated_instance_->AddPerInstanceObject(kPPPPrintingInterface, this);
}

Printing_Dev::~Printing_Dev() {
  associated_instance_->RemovePerInstanceObject(kPPPPrintingInterface, this);
}

}  // namespace pp
