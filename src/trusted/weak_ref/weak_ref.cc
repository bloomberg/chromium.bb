/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#define NACL_LOG_MODULE_NAME "weak_ref"

#include "native_client/src/trusted/weak_ref/weak_ref.h"

#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/shared/platform/nacl_sync.h"
#include "native_client/src/shared/platform/nacl_sync_checked.h"
#include "native_client/src/shared/platform/nacl_sync_raii.h"

namespace nacl {

AnchoredResource::AnchoredResource(WeakRefAnchor* anchor)
    : anchor_(anchor->Ref()) {
  NaClXMutexCtor(&mu_);
}

AnchoredResource::~AnchoredResource() {
  anchor_->Unref();
  NaClMutexDtor(&mu_);
  NaClLog(4, "~AnchoredResource: this 0x%" NACL_PRIxPTR "\n", (uintptr_t) this);
}

WeakRefAnchor::WeakRefAnchor()
    : abandoned_(false) {
  NaClXMutexCtor(&mu_);
}

WeakRefAnchor::~WeakRefAnchor() {
  NaClMutexDtor(&mu_);
}  // only via Unref

bool WeakRefAnchor::is_abandoned() {
  nacl::MutexLocker take(&mu_);
  NaClLog(4, "is_abandoned: %d\n", abandoned_);
  return abandoned_;
}

void WeakRefAnchor::Abandon() {
  NaClLog(4,
          "Entered WeakRefAnchor::Abandon: this 0x%" NACL_PRIxPTR "\n",
          (uintptr_t) this);
  do {
    nacl::MutexLocker take(&mu_);
    abandoned_ = true;
  } while (0);

  NaClLog(4, "Leaving WeakRefAnchor::Abandon\n");
}

}  // namespace nacl
