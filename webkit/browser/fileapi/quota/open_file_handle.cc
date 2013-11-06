// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/browser/fileapi/quota/open_file_handle.h"

#include "webkit/browser/fileapi/quota/open_file_handle_context.h"
#include "webkit/browser/fileapi/quota/quota_reservation.h"

namespace fileapi {

OpenFileHandle::~OpenFileHandle() {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
}

int64 OpenFileHandle::UpdateMaxWrittenOffset(int64 offset) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());

  int64 new_file_size = 0;
  int64 growth = 0;
  context_->UpdateMaxWrittenOffset(offset, &new_file_size, &growth);

  if (growth > 0)
    reservation_->ConsumeReservation(growth);

  return new_file_size;
}

int64 OpenFileHandle::base_file_size() const {
  return context_->base_file_size();
}

OpenFileHandle::OpenFileHandle(QuotaReservation* reservation,
                               OpenFileHandleContext* context)
    : reservation_(reservation),
      context_(context) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
}

}  // namespace fileapi
