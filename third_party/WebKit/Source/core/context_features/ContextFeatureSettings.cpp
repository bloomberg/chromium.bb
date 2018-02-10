// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/context_features/ContextFeatureSettings.h"

#include "core/dom/ExecutionContext.h"
#include "public/platform/Platform.h"

namespace blink {

ContextFeatureSettings::ContextFeatureSettings(ExecutionContext& context)
    : Supplement<ExecutionContext>(context) {}

// static
const char ContextFeatureSettings::kSupplementName[] = "ContextFeatureSettings";

// static
ContextFeatureSettings* ContextFeatureSettings::From(
    ExecutionContext* context,
    CreationMode creation_mode) {
  ContextFeatureSettings* settings =
      Supplement<ExecutionContext>::From<ContextFeatureSettings>(context);
  if (!settings && creation_mode == CreationMode::kCreateIfNotExists) {
    settings = new ContextFeatureSettings(*context);
    Supplement<ExecutionContext>::ProvideTo(*context, settings);
  }
  return settings;
}

void ContextFeatureSettings::Trace(blink::Visitor* visitor) {
  Supplement<ExecutionContext>::Trace(visitor);
}

}  // namespace blink
