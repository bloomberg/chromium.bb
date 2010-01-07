// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BAR_TOOLBAR_CLD_I18N_ENCODINGS_COMPACT_LANG_DET_WIN_CLD_DYNAMICSTATE_H_
#define BAR_TOOLBAR_CLD_I18N_ENCODINGS_COMPACT_LANG_DET_WIN_CLD_DYNAMICSTATE_H_

#include <tchar.h>
#include <windows.h>

#include "bar/toolbar/cld/i18n/encodings/compact_lang_det/win/cld_macros.h"
#include "bar/toolbar/cld/i18n/encodings/compact_lang_det/win/cld_scopedptr.h"


class CldTables;

namespace CompactLangDet {
  struct DetectionTables;
}  // namespace CompactLangDet


// Loads CLD tables and and sets up CLD internal structures to use the loaded
// tables instead of built-in ones.
class CldDynamicState {
 public:
  // Creates an empty CldDynamicState instance.  All setup is done in Load().
  CldDynamicState();
  // Clears everything, no need to explicitly call Unload().
  ~CldDynamicState();

  // Loads all CLD tables and parameters from the resource DLL and replaces
  // CLD table descriptions with the ones pointing to the loaded data.
  // [in] tables - loaded CLD data.  Takes an ownership of tables.
  //               Must not be NULL.
  // Returns: 0 in case of success, Windows GetLastError() code otherwise.
  DWORD Load(CldTables* tables);
  // Returns true if it is loaded successfully.
  bool IsLoaded() const;
  // Unloads all resources and resource DLL and reverts CLD data description
  // to the original ones.
  void Unload();

  // Returns a pointer to structure encapsulating CLD data tables required for
  // language detection. Returned pointer is guaranteed to be valid until
  // state is unloaded. Returns NULL if state is not loaded.
  const CompactLangDet::DetectionTables* detection_tables() const;

 private:
  // Forward declaration for CldDynamicState internal state types.
  struct CldStructures;
  struct State;

  // Prepares new data for CLD internal structures based on current state.
  // [in] tables - loaded CLD data tables.
  // [out] cld_structures - place to put new data in.
  DWORD CreateCldStructures(const CldTables& tables,
                            CldStructures* cld_structures) const;

  // Internal state, holds all loaded data and owns tables.
  scoped_ptr<State> state_;

  DISALLOW_COPY_AND_ASSIGN(CldDynamicState);
};


#endif  // BAR_TOOLBAR_CLD_I18N_ENCODINGS_COMPACT_LANG_DET_WIN_CLD_DYNAMICSTATE_H_
