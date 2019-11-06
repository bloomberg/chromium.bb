// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vp9_reference_frame_vector.h"

#include "media/gpu/vp9_picture.h"

namespace media {

Vp9ReferenceFrameVector::Vp9ReferenceFrameVector() {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

Vp9ReferenceFrameVector::~Vp9ReferenceFrameVector() {}

// Refresh the reference frame buffer slots with current frame
// based on refresh_frame_flags set in the frame_hdr.
void Vp9ReferenceFrameVector::Refresh(scoped_refptr<VP9Picture> pic) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(pic);
  const auto& frame_hdr = pic->frame_hdr;

  for (size_t i = 0, mask = frame_hdr->refresh_frame_flags; mask;
       mask >>= 1, ++i) {
    if (mask & 1)
      reference_frames_[i] = pic;
  }
}

void Vp9ReferenceFrameVector::Clear() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (auto& f : reference_frames_)
    f = nullptr;
}

// VP9 can maintains up to eight active reference frames and each
// frame can use up to three reference frames from this list.
// GetFrame will return the reference frame placed in reference_frames_[index]
scoped_refptr<VP9Picture> Vp9ReferenceFrameVector::GetFrame(
    size_t index) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return reference_frames_[index];
}

}  // namespace media
