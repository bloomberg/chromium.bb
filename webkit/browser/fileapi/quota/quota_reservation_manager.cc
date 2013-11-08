// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/browser/fileapi/quota/quota_reservation_manager.h"

#include "webkit/browser/fileapi/quota/quota_reservation.h"
#include "webkit/browser/fileapi/quota/quota_reservation_buffer.h"

namespace fileapi {

QuotaReservationManager::QuotaReservationManager(QuotaBackend* backend)
    : backend_(backend),
      weak_ptr_factory_(this) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
}

QuotaReservationManager::~QuotaReservationManager() {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
}

void QuotaReservationManager::ReserveQuota(
    const GURL& origin,
    FileSystemType type,
    int64 size,
    const ReserveQuotaCallback& callback) {
  backend_->ReserveQuota(origin, type, size, callback);
}

void QuotaReservationManager::ReleaseReservedQuota(
    const GURL& origin,
    FileSystemType type,
    int64 size) {
  backend_->ReleaseReservedQuota(origin, type, size);
}

void QuotaReservationManager::CommitQuotaUsage(
    const GURL& origin,
    FileSystemType type,
    int64 delta,
    const StatusCallback& callback) {
  backend_->CommitQuotaUsage(origin, type, delta, callback);
}

void QuotaReservationManager::IncreaseDirtyCount(const GURL& origin,
                                                 FileSystemType type) {
  backend_->IncreaseDirtyCount(origin, type);
}

void QuotaReservationManager::DecreaseDirtyCount(const GURL& origin,
                                                 FileSystemType type) {
  backend_->DecreaseDirtyCount(origin, type);
}

scoped_refptr<QuotaReservationBuffer>
QuotaReservationManager::GetReservationBuffer(
    const GURL& origin,
    FileSystemType type) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  QuotaReservationBuffer** buffer =
      &reservation_buffers_[std::make_pair(origin, type)];
  if (!*buffer) {
    *buffer = new QuotaReservationBuffer(
        weak_ptr_factory_.GetWeakPtr(), origin, type);
  }
  return make_scoped_refptr(*buffer);
}

void QuotaReservationManager::ReleaseReservationBuffer(
    QuotaReservationBuffer* reservation_buffer) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  std::pair<GURL, FileSystemType> key(reservation_buffer->origin(),
                                      reservation_buffer->type());
  DCHECK_EQ(reservation_buffers_[key], reservation_buffer);
  reservation_buffers_.erase(key);
}

scoped_refptr<QuotaReservation> QuotaReservationManager::CreateReservation(
    const GURL& origin,
    FileSystemType type) {
  return GetReservationBuffer(origin, type)->CreateReservation();;
}

}  // namespace fileapi
