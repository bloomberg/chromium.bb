// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/mojo/CommonCustomTypesStructTraits.h"

#include "base/strings/latin1_string_conversions.h"
#include <cstring>

namespace mojo {

// static
WTF::Vector<uint16_t>
StructTraits<mojo::common::mojom::String16DataView, ::WTF::String>::data(
    const ::WTF::String& str) {
  // TODO(zqzhang): Need to find a way to avoid another memory copy here. A
  // broader is question is that should sending String16 over mojo be avoided?
  // See https://crbug.com/653209

  base::string16 str16 = base::Latin1OrUTF16ToUTF16(
      str.length(), str.is8Bit() ? str.characters8() : nullptr /* latin1 */,
      str.is8Bit() ? nullptr : str.characters16() /* utf16 */);
  WTF::Vector<uint16_t> rawData(str16.size());
  memcpy(rawData.data(), str16.data(), str16.size() * sizeof(uint16_t));
  return rawData;
}

// static
bool StructTraits<mojo::common::mojom::String16DataView, ::WTF::String>::Read(
    mojo::common::mojom::String16DataView data,
    ::WTF::String* out) {
  mojo::ArrayDataView<uint16_t> view;
  data.GetDataDataView(&view);
  if (view.is_null())
    return false;
  *out =
      ::WTF::String(reinterpret_cast<const UChar*>(view.data()), view.size());
  return true;
}

}  // namespace mojo
