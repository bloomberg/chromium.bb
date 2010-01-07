// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BAR_TOOLBAR_CLD_I18N_ENCODINGS_COMPACT_LANG_DET_WIN_RESOURCEINMEMORY_H_
#define BAR_TOOLBAR_CLD_I18N_ENCODINGS_COMPACT_LANG_DET_WIN_RESOURCEINMEMORY_H_

#include <tchar.h>
#include <windows.h>

#include "bar/toolbar/cld/i18n/encodings/compact_lang_det/win/cld_scopedptr.h"


// Loads a resource of specified type and id and keeps it locked in memory.
// Exposes pointer to the resource's raw data and size.
class ResourceInMemory {
 public:
  // Creates an empty instance.  All setup is done in Load().
  ResourceInMemory();
  // Clears everything, no need to explicitly call Unload().
  ~ResourceInMemory();

  // Loads a resource from DLL and keeps it in memory, ready for access.
  // [in] library - handle of a library to load the resource from.
  //                library must not be unloaded until all resources in memory
  //                are unloaded.
  // [in] resource_id - id of the resource to load.
  // [in] resource_type - type of the resource to load.
  // Returns: 0 in case of success, Windows GetLastError() code otherwise.
  // Does not unload currently owned resource in case of failure.
  DWORD Load(HMODULE library, const TCHAR* resource_id,
             const TCHAR* resource_type);
  // Returns true if it is loaded successfully.
  bool IsLoaded() const;
  // Unloads the resource.
  void Unload();

  // Size of the loaded resource.
  // Returns 0 if resource was not successfully loaded.
  DWORD size() const;
  // Pointer to the resource data.
  // Returns NULL if resource was not successfully loaded.
  void* data() const;

 private:
  // Forward declaration for ResourceInMemory internal state type.
  struct State;

  // Does the actual resource loading.
  // [in] library - handle of a library to load the resource from.
  // [in] resource_id - id of the resource to load.
  // [in] resource_type - type of the resource to load.
  // [out] state - newly loaded state.  Must not be NULL.
  DWORD LoadState(HMODULE library, const TCHAR* resource_id,
                  const TCHAR* resource_type, scoped_ptr<State>* state) const;

  // Internal state, holds all loaded data and owns a resource library handler.
  scoped_ptr<State> state_;

  DISALLOW_COPY_AND_ASSIGN(ResourceInMemory);
};


#endif  // BAR_TOOLBAR_CLD_I18N_ENCODINGS_COMPACT_LANG_DET_WIN_RESOURCEINMEMORY_H_
