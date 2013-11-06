// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/browser/fileapi/quota/quota_reservation.h"

#include "base/bind.h"
#include "webkit/browser/fileapi/quota/open_file_handle.h"
#include "webkit/browser/fileapi/quota/quota_reservation_buffer.h"

namespace fileapi {

void QuotaReservation::RefreshReservation(
    int64 size,
    const StatusCallback& callback) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  DCHECK(!running_refresh_request_);
  if (!reservation_manager())
    return;

  running_refresh_request_ = true;

  reservation_manager()->ReserveQuota(
      origin(), type(), size - remaining_quota_,
      base::Bind(&QuotaReservation::AdaptDidUpdateReservedQuota,
                 weak_ptr_factory_.GetWeakPtr(),
                 size, callback));

  if (running_refresh_request_)
    remaining_quota_ = 0;
}

scoped_ptr<OpenFileHandle> QuotaReservation::GetOpenFileHandle(
    const base::FilePath& platform_path) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  return reservation_buffer_->GetOpenFileHandle(this, platform_path);
}

void QuotaReservation::OnClientCrash() {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());

  if (remaining_quota_) {
    reservation_buffer_->PutReservationToBuffer(remaining_quota_);
    remaining_quota_ = 0;
  }
}

void QuotaReservation::ConsumeReservation(int64 size) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  DCHECK_LT(0, size);
  DCHECK_LT(size, remaining_quota_);

  remaining_quota_ -= size;
  reservation_buffer_->PutReservationToBuffer(size);
}

QuotaReservationManager* QuotaReservation::reservation_manager() {
  return reservation_buffer_->reservation_manager();
}

const GURL& QuotaReservation::origin() const {
  return reservation_buffer_->origin();
}

FileSystemType QuotaReservation::type() const {
  return reservation_buffer_->type();
}

QuotaReservation::QuotaReservation(
    QuotaReservationBuffer* reservation_buffer)
    : running_refresh_request_(false),
      remaining_quota_(0),
      reservation_buffer_(reservation_buffer),
      weak_ptr_factory_(this) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
}

QuotaReservation::~QuotaReservation() {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());

  if (remaining_quota_ && reservation_manager()) {
    reservation_manager()->ReleaseReservedQuota(
        origin(), type(), remaining_quota_);
  }
}

// static
bool QuotaReservation::AdaptDidUpdateReservedQuota(
    const base::WeakPtr<QuotaReservation>& reservation,
    int64 new_reserved_size,
    const StatusCallback& callback,
    base::PlatformFileError error) {
  if (!reservation)
    return false;

  reservation->DidUpdateReservedQuota(new_reserved_size, callback, error);
  return true;
}

void QuotaReservation::DidUpdateReservedQuota(
    int64 new_reserved_size,
    const StatusCallback& callback,
    base::PlatformFileError error) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  DCHECK(running_refresh_request_);
  running_refresh_request_ = false;

  if (error == base::PLATFORM_FILE_OK)
    remaining_quota_ = new_reserved_size;
  callback.Run(error);
}

}  // namespace fileapi
