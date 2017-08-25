// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/mojo/CommonCustomTypesStructTraits.h"

#include <cstring>

namespace mojo {

// static
void* StructTraits<common::mojom::String16DataView, WTF::String>::SetUpContext(
    const WTF::String& input) {
  // If it is null (i.e., StructTraits<>::IsNull() returns true), this method is
  // guaranteed not to be called.
  DCHECK(!input.IsNull());

  if (!input.Is8Bit())
    return nullptr;

  return new base::string16(input.Characters8(),
                            input.Characters8() + input.length());
}

// static
base::span<const uint16_t>
StructTraits<common::mojom::String16DataView, WTF::String>::data(
    const WTF::String& input,
    void* context) {
  auto contextObject = static_cast<base::string16*>(context);
  DCHECK_EQ(input.Is8Bit(), !!contextObject);

  if (contextObject) {
    return base::make_span(
        reinterpret_cast<const uint16_t*>(contextObject->data()),
        contextObject->size());
  }

  return base::make_span(
      reinterpret_cast<const uint16_t*>(input.Characters16()), input.length());
}

// static
bool StructTraits<common::mojom::String16DataView, WTF::String>::Read(
    common::mojom::String16DataView data,
    WTF::String* out) {
  ArrayDataView<uint16_t> view;
  data.GetDataDataView(&view);
  *out = WTF::String(reinterpret_cast<const UChar*>(view.data()), view.size());
  return true;
}

}  // namespace mojo
