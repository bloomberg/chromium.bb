// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bar/toolbar/cld/i18n/encodings/compact_lang_det/win/cld_tables.h"

#include "bar/toolbar/cld/i18n/encodings/compact_lang_det/win/cld_macros.h"
#include "bar/toolbar/cld/i18n/encodings/compact_lang_det/win/cld_resourceids.h"
#include "bar/toolbar/cld/i18n/encodings/compact_lang_det/win/cld_scopedptr.h"
#include "bar/toolbar/cld/i18n/encodings/compact_lang_det/win/resourceinmemory.h"


// CldTables::State.

struct CldTables::State {
  State()
      : resources(0) {
  }

  const ResourceInMemory* get_table(TableIndex i) const {
    if (NULL != tables.get() && 0 <= i && i < TABLE_COUNT)
      return &tables[i];
    _ASSERT(0);
    return NULL;
  }

  const DWORD get_parameter(ParameterIndex i) const {
    if (NULL != parameters.get() && 0 <= i && i < PARAMETER_COUNT)
      return parameters[i];
    _ASSERT(0);
    return 0;
  }

  // Loaded resource DLL.
  SAFE_HMODULE resources;
  // Big CLD tables.
  scoped_array<ResourceInMemory> tables;
  // Table parameters.
  scoped_array<DWORD> parameters;

  DISALLOW_COPY_AND_ASSIGN(State);
};


// CldTables.

CldTables::CldTables() {
}

CldTables::~CldTables() {
  Unload();
}


DWORD CldTables::Load(HMODULE resource_library) {
  // First thing, assume the ownership of the resource_library to do not leak it
  // in case of failure.
  SAFE_HMODULE owned_resource_library(resource_library);

  return LoadState(&owned_resource_library, &state_);
}


bool CldTables::IsLoaded() const {
  return NULL != state_.get();
}


void CldTables::Unload() {
  state_.reset();
}


const ResourceInMemory* CldTables::get_table(TableIndex i) const {
  _ASSERT(state_.get());
  if (!state_.get())
    return NULL;
  return state_->get_table(i);
}

const DWORD CldTables::get_parameter(ParameterIndex i) const {
  _ASSERT(state_.get());
  if (!state_.get())
    return 0;
  return state_->get_parameter(i);
}


DWORD CldTables::LoadState(SAFE_HMODULE* resource_library,
                           scoped_ptr<State>* state) const {
  _ASSERT(resource_library);
  _ASSERT(state);
  if (!resource_library || !state)
    return ERROR_INVALID_PARAMETER;

  // Allocate a new state.
  scoped_ptr<State> new_state(new State);
  if (!new_state.get())
    return ERROR_OUTOFMEMORY;

  // Transfer resource_library ownership to new_state.
  swap(new_state->resources, *resource_library);

  // Load data from resources.
  RETURN_IF_ERROR(
      LoadTables(new_state->resources.get(), &new_state->tables));

  RETURN_IF_ERROR(
      LoadParameters(new_state->resources.get(), &new_state->parameters));

  // Safely put new state in place.
  swap(*state, new_state);
  return 0;
}


DWORD CldTables::LoadTables(HMODULE resource_library,
                            scoped_array<ResourceInMemory>* tables) const {
  _ASSERT(tables);
  if (!tables)
    return ERROR_INVALID_PARAMETER;

  // Mapping from the table index to the corresponding resource identifier.
  static const TCHAR* resource_ids[] = {
    IDR_CLD_TABLE_QUADS,
    IDR_CLD_TABLE_INDIRECT,
    IDR_CLD_TABLE_CTJKVZ,
  };
  COMPILE_ASSERT(arraysize(resource_ids) == TABLE_COUNT, k_table_index_changed);

  // Allocate a new copy of tables.
  scoped_array<ResourceInMemory> new_tables(
      new ResourceInMemory[arraysize(resource_ids)]);
  if (!new_tables.get())
    return ERROR_OUTOFMEMORY;

  // Load tables one by one.
  for (size_t i = 0; i < arraysize(resource_ids); ++i) {
    RETURN_IF_ERROR(
        new_tables[i].Load(resource_library, resource_ids[i], RT_RCDATA));
  }

  // Safely put new tables in place.
  swap(*tables, new_tables);
  return 0;
}


DWORD CldTables::LoadParameters(HMODULE resource_library,
                                scoped_array<DWORD>* parameters) const {
  _ASSERT(parameters);
  if (!parameters)
    return ERROR_INVALID_PARAMETER;

  // Mapping from the parameter index to the corresponding resource identifier.
  static const TCHAR* resource_ids[] = {
    IDR_CLD_QUAD_TABLE_KEY_MASK,
    IDR_CLD_QUAD_TABLE_BUILD_DATE,
    IDR_CLD_CTJKVZ_TABLE_STATE0,
    IDR_CLD_CTJKVZ_TABLE_STATE0_SIZE,
    IDR_CLD_CTJKVZ_TABLE_MAX_EXPAND_X4,
    IDR_CLD_CTJKVZ_TABLE_SHIFT,
    IDR_CLD_CTJKVZ_TABLE_BYTES,
    IDR_CLD_CTJKVZ_TABLE_LOSUB,
    IDR_CLD_CTJKVZ_TABLE_HIADD,
  };
  COMPILE_ASSERT(arraysize(resource_ids) == PARAMETER_COUNT,
                 k_table_parameter_index_changed);

  // Allocate a new copy of parameters.
  scoped_array<DWORD> new_parameters(new DWORD[arraysize(resource_ids)]);
  if (!new_parameters.get())
    return ERROR_OUTOFMEMORY;

  // Load parameters one by one.
  for (size_t i = 0; i < arraysize(resource_ids); ++i) {
    ResourceInMemory resource;
    RETURN_IF_ERROR(
        resource.Load(resource_library, resource_ids[i], RT_RCDATA));
    if (resource.size() != sizeof(DWORD))
      return ERROR_INVALID_DATA;

    new_parameters[i] = *reinterpret_cast<DWORD*>(resource.data());
  }

  // Safely put new parameters in place.
  swap(*parameters, new_parameters);
  return 0;
}
