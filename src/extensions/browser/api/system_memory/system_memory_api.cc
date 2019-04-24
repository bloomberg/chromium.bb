// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/system_memory/system_memory_api.h"

#include "base/bind.h"
#include "extensions/browser/api/system_memory/memory_info_provider.h"

namespace extensions {

using api::system_memory::MemoryInfo;

SystemMemoryGetInfoFunction::SystemMemoryGetInfoFunction() {
}

SystemMemoryGetInfoFunction::~SystemMemoryGetInfoFunction() {
}

ExtensionFunction::ResponseAction SystemMemoryGetInfoFunction::Run() {
  MemoryInfoProvider::Get()->StartQueryInfo(
      base::Bind(&SystemMemoryGetInfoFunction::OnGetMemoryInfoCompleted, this));
  // StartQueryInfo responds asynchronously.
  return RespondLater();
}

void SystemMemoryGetInfoFunction::OnGetMemoryInfoCompleted(bool success) {
  if (success)
    Respond(OneArgument(MemoryInfoProvider::Get()->memory_info().ToValue()));
  else
    Respond(Error("Error occurred when querying memory information."));
}

}  // namespace extensions
