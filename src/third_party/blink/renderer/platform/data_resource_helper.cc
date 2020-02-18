// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/data_resource_helper.h"

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_data.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "ui/base/resource/resource_bundle.h"

namespace blink {

String GetDataResourceAsASCIIString(const char* resource) {
  StringBuilder builder;
  const WebData& resource_data = Platform::Current()->GetDataResource(resource);
  builder.ReserveCapacity(resource_data.size());
  resource_data.ForEachSegment([&builder](const char* segment,
                                          size_t segment_size,
                                          size_t segment_offset) {
    builder.Append(segment, segment_size);
    return true;
  });

  String data_string = builder.ToString();
  DCHECK(!data_string.IsEmpty());
  DCHECK(data_string.ContainsOnlyASCIIOrEmpty());
  return data_string;
}

String UncompressResourceAsString(int resource_id) {
  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  std::string uncompressed = bundle.DecompressDataResourceScaled(
      resource_id, bundle.GetMaxScaleFactor());
  return String::FromUTF8(uncompressed.data());
}

}  // namespace blink
