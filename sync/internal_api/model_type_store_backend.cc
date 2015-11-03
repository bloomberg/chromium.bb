// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/model_type_store_backend.h"

namespace syncer_v2 {

ModelTypeStoreBackend::ModelTypeStoreBackend() {
  // ModelTypeStoreBackend is used on different thread from the one where it is
  // created.
  DetachFromThread();
}

ModelTypeStoreBackend::~ModelTypeStoreBackend() {
  DCHECK(CalledOnValidThread());
}

ModelTypeStore::Result ModelTypeStoreBackend::Init() {
  DCHECK(CalledOnValidThread());
  return ModelTypeStore::Result::SUCCESS;
}

}  // namespace syncer_v2
