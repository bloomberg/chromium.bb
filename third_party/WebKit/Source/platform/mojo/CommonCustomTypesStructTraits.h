// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CommonCustomTypesStructTraits_h
#define CommonCustomTypesStructTraits_h

#include "mojo/common/common_custom_types.mojom-blink.h"
#include "platform/PlatformExport.h"

namespace mojo {

template <>
struct PLATFORM_EXPORT
    StructTraits<mojo::common::mojom::String16DataView, ::WTF::String> {
  static WTF::Vector<uint16_t> data(const ::WTF::String&);
  static bool Read(mojo::common::mojom::String16DataView, ::WTF::String* out);
};

}  // namespace mojo

#endif  // CommonCustomTypesStructTraits_h
