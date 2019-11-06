// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/engines/broker/engine_cleanup_results_impl.h"

#include <utility>

namespace chrome_cleaner {

EngineCleanupResultsImpl::EngineCleanupResultsImpl(
    InterfaceMetadataObserver* metadata_observer)
    : binding_(this), metadata_observer_(metadata_observer) {}

EngineCleanupResultsImpl::~EngineCleanupResultsImpl() = default;

void EngineCleanupResultsImpl::BindToCallbacks(
    mojom::EngineCleanupResultsAssociatedPtrInfo* ptr_info,
    DoneCallback done_callback) {
  binding_.Bind(mojo::MakeRequest(ptr_info));
  // There's no need to call set_connection_error_handler on this since it's an
  // associated interface. Any errors will be handled on the main EngineCommands
  // interface.
  done_callback_ = std::move(done_callback);
}

void EngineCleanupResultsImpl::Done(uint32_t result_code) {
  if (metadata_observer_)
    metadata_observer_->ObserveCall(CURRENT_FILE_AND_METHOD);
  DCHECK(done_callback_);
  std::move(done_callback_).Run(result_code);
}

}  // namespace chrome_cleaner
