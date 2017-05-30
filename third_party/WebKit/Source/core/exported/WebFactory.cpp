// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/exported/WebFactory.h"
#include "platform/wtf/Assertions.h"

namespace blink {

WebFactory* WebFactory::factory_instance_ = nullptr;

WebFactory& WebFactory::GetInstance() {
  DCHECK(factory_instance_);
  return *factory_instance_;
}

void WebFactory::SetInstance(WebFactory& factory) {
  DCHECK_EQ(nullptr, factory_instance_);
  factory_instance_ = &factory;
}

}  // namespace blink
