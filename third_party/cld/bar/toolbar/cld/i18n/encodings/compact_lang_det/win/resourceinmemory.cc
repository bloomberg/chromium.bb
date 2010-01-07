// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bar/toolbar/cld/i18n/encodings/compact_lang_det/win/resourceinmemory.h"

#include "bar/toolbar/cld/i18n/encodings/compact_lang_det/win/cld_scopedptr.h"


// ResourceInMemory::State.

struct ResourceInMemory::State {
  State()
      : resource(0) {
  }

  // Handle to the loaded resource.
  SAFE_RESOURCE resource;
  // Size of the loaded resource.
  DWORD size;
  // Pointer to the resource locked in memory.
  void* data;

  DISALLOW_COPY_AND_ASSIGN(State);
};


// ResourceInMemory.

ResourceInMemory::ResourceInMemory() {
}

ResourceInMemory::~ResourceInMemory() {
  Unload();
}


DWORD ResourceInMemory::size() const {
  if (!state_.get())
    return 0;
  return state_->size;
}

void* ResourceInMemory::data() const {
  if (!state_.get())
    return NULL;
  return state_->data;
}


DWORD ResourceInMemory::Load(HMODULE library, const TCHAR* resource_id,
                             const TCHAR* resource_type) {
  return LoadState(library, resource_id, resource_type, &state_);
}


bool ResourceInMemory::IsLoaded() const {
  return NULL != state_.get();
}


void ResourceInMemory::Unload() {
  state_.reset();
}


DWORD ResourceInMemory::LoadState(HMODULE library, const TCHAR* resource_id,
                                  const TCHAR* resource_type,
                                  scoped_ptr<State>* state) const {
  _ASSERT(state);
  if (!state)
    return ERROR_INVALID_PARAMETER;

  // Find the resource.
  HRSRC resource_source = ::FindResource(library, resource_id, resource_type);
  if (!resource_source)
    return ::GetLastError();

  // Allocate a new state.
  scoped_ptr<State> new_state(new State);
  if (!new_state.get())
    return ERROR_OUTOFMEMORY;

  // Check resource size.
  new_state->size = ::SizeofResource(library, resource_source);
  if (!new_state->size)
    return ::GetLastError();

  // Load resource.
  new_state->resource.reset(::LoadResource(library, resource_source));
  if (!new_state->resource.get())
    return ::GetLastError();

  // Lock resource in memory.
  new_state->data = ::LockResource(new_state->resource.get());
  if (!new_state->data)
    return ERROR_OUTOFMEMORY;

  // Safely put new state in place.
  swap(*state, new_state);
  return 0;
}
