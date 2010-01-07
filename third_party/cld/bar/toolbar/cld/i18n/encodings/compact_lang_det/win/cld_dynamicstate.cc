// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bar/toolbar/cld/i18n/encodings/compact_lang_det/win/cld_dynamicstate.h"

#include "bar/toolbar/cld/i18n/encodings/compact_lang_det/cldutil.h"
#include "bar/toolbar/cld/i18n/encodings/compact_lang_det/compact_lang_det.h"
#include "bar/toolbar/cld/i18n/encodings/compact_lang_det/win/cld_macros.h"
#include "bar/toolbar/cld/i18n/encodings/compact_lang_det/win/cld_scopedptr.h"
#include "bar/toolbar/cld/i18n/encodings/compact_lang_det/win/cld_tables.h"
#include "bar/toolbar/cld/i18n/encodings/compact_lang_det/win/cld_utf8statetable.h"
#include "bar/toolbar/cld/i18n/encodings/compact_lang_det/win/resourceinmemory.h"


// External declarations for CLD table descriptions we're replacing with
// data loaded from the resource DLL. Required here to get access to default
// values.
extern const UTF8PropObj compact_lang_det_generated_ctjkvz_b1_obj;
extern const cld::CLDTableSummary kQuadTable_obj;


// CldDynamicState::CldStructures.

struct CldDynamicState::CldStructures {
  // Creates an empty instance.  Due to the lack of default constructors for
  // CLD structures, use their current values as a initial ones and then
  // zero them out to avoid the temptation to use this side effect later.
  CldStructures()
      : ctjkvz(compact_lang_det_generated_ctjkvz_b1_obj),
        quads(kQuadTable_obj) {
    memset(&ctjkvz, 0, sizeof(ctjkvz));
    memset(&quads, 0, sizeof(quads));
    detection_tables.quadgram_obj = &quads;
    detection_tables.unigram_obj = &ctjkvz;
  }
  // Shallow copy of CLD data structures.
  void ShallowCopyFrom(const cld::CLDTableSummary& quads_to_copy,
                       const UTF8StateMachineObj& ctjkvz_to_copy) {
    memcpy(&quads, &quads_to_copy, sizeof(quads));
    memcpy(&ctjkvz, &ctjkvz_to_copy, sizeof(ctjkvz));
  }

  // Descriptions for internal CLD tables.
  cld::CLDTableSummary quads;
  UTF8StateMachineObj ctjkvz;
  // Cached pointers to the structures above, see
  // CldDynamicState::detection_tables().
  CompactLangDet::DetectionTables detection_tables;

  DISALLOW_COPY_AND_ASSIGN(CldStructures);
};


// CldDynamicState::State.

struct CldDynamicState::State {
  State() {}

  // Loaded CLD data tables.
  scoped_ptr<CldTables> tables;

  // New CLD internal structures pointing to the loaded CLD data tables.
  CldStructures cld_structures;

  DISALLOW_COPY_AND_ASSIGN(State);
};


// CldDynamicState.

CldDynamicState::CldDynamicState() {
}

CldDynamicState::~CldDynamicState() {
  Unload();
}


DWORD CldDynamicState::Load(CldTables* tables) {
  _ASSERT(tables);
  if (!tables)
    return ERROR_INVALID_PARAMETER;
  // First thing, assume tables ownership to do not leak it in case of error.
  scoped_ptr<CldTables> new_tables(tables);

  // Save current CLD internal data structures.
  scoped_ptr<State> new_state(new State);
  if (!new_state.get())
    return ERROR_OUTOFMEMORY;

  // Transfer tables ownership to new_state.
  swap(new_state->tables, new_tables);

  // Create new CLD internal data structures.
  RETURN_IF_ERROR(
      CreateCldStructures(*new_state->tables, &new_state->cld_structures));

  // Safely put new state in place.
  swap(state_, new_state);
  return 0;
}


bool CldDynamicState::IsLoaded() const {
  return state_.get();
}


void CldDynamicState::Unload() {
  state_.reset();
}


const CompactLangDet::DetectionTables*
    CldDynamicState::detection_tables() const {
  if (!state_.get())
    return NULL;
  return &state_->cld_structures.detection_tables;
}


DWORD CldDynamicState::CreateCldStructures(
    const CldTables& tables, CldStructures* cld_structures) const {
  _ASSERT(cld_structures);
  if (!cld_structures)
    return ERROR_INVALID_PARAMETER;

  // Get access to the tables.
  const ResourceInMemory* quads = tables.get_table(CldTables::TABLE_QUADS);
  const ResourceInMemory* indirect =
      tables.get_table(CldTables::TABLE_INDIRECT);
  const ResourceInMemory* ctjkvz = tables.get_table(CldTables::TABLE_CTJKVZ);

  if (!quads || !indirect || !ctjkvz)
    return ERROR_INVALID_DATA;

  // Check if loaded data is of the valid size.
  if (0 != quads->size() % sizeof(cld_structures->quads.kCLDTable[0]))
    return ERROR_INVALID_DATA;
  if (0 != indirect->size() % sizeof(cld_structures->quads.kCLDTableInd[0]))
    return ERROR_INVALID_DATA;

  // This is our new quads table description.
  cld::CLDTableSummary new_quads = {
    reinterpret_cast<const cld::IndirectProbBucket4*>(
        quads->data()),  // kCLDTable
    reinterpret_cast<const uint32*>(
        indirect->data()),  // kCLDTableInd
    quads->size() / sizeof(cld::IndirectProbBucket4),  // kCLDTableSize
    indirect->size() /
        sizeof(cld_structures->quads.kCLDTableInd[0]),  // kCLDTableIndSize
    tables.get_parameter(CldTables::QUADS_KEY_MASK),  // kCLDTableKeyMask
    tables.get_parameter(CldTables::QUADS_BUILD_DATE),  // kCLDTableBuildDate
  };

  // This is our new ctjkvz table description.
  UTF8StateMachineObj new_ctjkvz = {
    tables.get_parameter(CldTables::CTJKVZ_STATE0),  // state0
    tables.get_parameter(CldTables::CTJKVZ_STATE0_SIZE),  // state0_size
    ctjkvz->size(),  // total_size
    tables.get_parameter(CldTables::CTJKVZ_MAX_EXPAND_X4),  // max_expand
    tables.get_parameter(CldTables::CTJKVZ_SHIFT),  // entry_shift
    tables.get_parameter(CldTables::CTJKVZ_BYTES),  // bytes_per_entry
    tables.get_parameter(CldTables::CTJKVZ_LOSUB),  // losub
    tables.get_parameter(CldTables::CTJKVZ_HIADD),  // hiadd
    reinterpret_cast<const uint8*>(ctjkvz->data()),  // state_table
    compact_lang_det_generated_ctjkvz_b1_obj.remap_base,  // remap_base
    compact_lang_det_generated_ctjkvz_b1_obj.remap_string,  // remap_string
    compact_lang_det_generated_ctjkvz_b1_obj.fast_state,  // fast_state
  };

  // Set up new CldStructures instance.
  cld_structures->ShallowCopyFrom(new_quads, new_ctjkvz);
  return 0;
}
