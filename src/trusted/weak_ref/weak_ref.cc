/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/trusted/weak_ref/weak_ref.h"

#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/platform/nacl_sync.h"
#include "native_client/src/shared/platform/nacl_sync_checked.h"

namespace nacl {

AnchoredResource::AnchoredResource(WeakRefAnchor* anchor)
    : anchor_(anchor) {
  NaClXMutexCtor(&mu_);
}

AnchoredResource::~AnchoredResource() {
  NaClMutexDtor(&mu_);
}

void AnchoredResource::Abandon() {
  NaClXMutexLock(&mu_);
  anchor_ = NULL;
  reset_mu();
  NaClXMutexUnlock(&mu_);
  Unref();
}

WeakRefAnchor::WeakRefAnchor()
    : abandoned_(false) {
  NaClXMutexCtor(&mu_);
}

void WeakRefAnchor::Abandon() {
  NaClXMutexLock(&mu_);
  for (container_type::iterator it = tracked_.begin();
       it != tracked_.end(); ++it) {
    (*it)->Abandon();  // eager resource release
  }
  abandoned_ = true;
  NaClXMutexUnlock(&mu_);
}

}  // namespace nacl
