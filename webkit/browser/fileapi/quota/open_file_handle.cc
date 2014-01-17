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

void OpenFileHandle::UpdateMaxWrittenOffset(int64 offset) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());

  int64 growth = context_->UpdateMaxWrittenOffset(offset);
  if (growth > 0)
    reservation_->ConsumeReservation(growth);
}

int64 OpenFileHandle::GetEstimatedFileSize() const {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  return context_->GetEstimatedFileSize();
}

OpenFileHandle::OpenFileHandle(QuotaReservation* reservation,
                               OpenFileHandleContext* context)
    : reservation_(reservation),
      context_(context) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
}

}  // namespace fileapi
