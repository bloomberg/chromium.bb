// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/interfaces/video_transformation_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<media::mojom::VideoTransformationDataView,
                  media::VideoTransformation>::
    Read(media::mojom::VideoTransformationDataView input,
         media::VideoTransformation* output) {
  if (!input.ReadRotation(&output->rotation))
    return false;

  output->mirrored = input.mirrored();

  return true;
}

}  // namespace mojo
