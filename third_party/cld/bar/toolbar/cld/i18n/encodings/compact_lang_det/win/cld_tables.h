// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BAR_TOOLBAR_CLD_I18N_ENCODINGS_COMPACT_LANG_DET_WIN_CLD_TABLES_H_
#define BAR_TOOLBAR_CLD_I18N_ENCODINGS_COMPACT_LANG_DET_WIN_CLD_TABLES_H_

#include <tchar.h>
#include <windows.h>

#include "bar/toolbar/cld/i18n/encodings/compact_lang_det/win/cld_macros.h"
#include "bar/toolbar/cld/i18n/encodings/compact_lang_det/win/cld_scopedptr.h"


class ResourceInMemory;


class CldTables {
 public:
  // List of CLD tables to load from resources.
  typedef enum {
    TABLE_QUADS,
    TABLE_INDIRECT,
    TABLE_CTJKVZ,
    TABLE_COUNT,
  } TableIndex;
  // List of CLD parameters to load from resources.
  typedef enum {
    QUADS_KEY_MASK,
    QUADS_BUILD_DATE,
    CTJKVZ_STATE0,
    CTJKVZ_STATE0_SIZE,
    CTJKVZ_MAX_EXPAND_X4,
    CTJKVZ_SHIFT,
    CTJKVZ_BYTES,
    CTJKVZ_LOSUB,
    CTJKVZ_HIADD,
    PARAMETER_COUNT,
  } ParameterIndex;

  // Creates an empty CldTables instance.  All setup is done in Load().
  CldTables();
  // Clears everything, no need to explicitly call Unload().
  ~CldTables();

  // Loads all CLD tables and parameters from the resource DLL and replaces
  // CLD table descriptions with the ones pointing to the loaded data.
  // [in] resource_library - library to load resources from.
  //     Takes ownership of this library and will unload it in Unload().
  // Returns: 0 in case of success, Windows GetLastError() code otherwise.
  // Does not unload current state in case of failure.
  DWORD Load(HMODULE resource_library);
  // Returns true if it is loaded successfully.
  bool IsLoaded() const;
  // Unloads all resources and resource DLL.
  void Unload();

  const ResourceInMemory* get_table(TableIndex i) const;
  const DWORD get_parameter(ParameterIndex i) const;

 private:
  // Forward declaration for CldTables internal state type.
  struct State;

  // Does the actual resource loading.
  // [in] resource_library - library to load resources from.
  // [out] state - new state.  Must not be NULL.
  DWORD LoadState(SAFE_HMODULE* resource_library,
                  scoped_ptr<State>* state) const;
  // Loads tables from the resources.
  // [in] resource_library - library to load resources from.
  // [out] tables - loaded tables.  Must not be NULL.
  DWORD LoadTables(HMODULE resource_library,
                   scoped_array<ResourceInMemory>* tables) const;
  // Loads parameters from the resources.
  // [in] resource_library - library to load resources from.
  // [out] parameters - loaded parameters.  Must not be NULL.
  DWORD LoadParameters(HMODULE resource_library,
                       scoped_array<DWORD>* parameters) const;

  // Internal state, holds all loaded data and owns a resource library handler.
  scoped_ptr<State> state_;

  DISALLOW_COPY_AND_ASSIGN(CldTables);
};


#endif  // BAR_TOOLBAR_CLD_I18N_ENCODINGS_COMPACT_LANG_DET_WIN_CLD_TABLES_H_
