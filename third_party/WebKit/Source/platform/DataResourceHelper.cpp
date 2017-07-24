// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/DataResourceHelper.h"

#include "platform/wtf/text/StringBuilder.h"
#include "public/platform/Platform.h"
#include "public/platform/WebData.h"

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
  DCHECK(data_string.ContainsOnlyASCII());
  return data_string;
}

}  // namespace blink
