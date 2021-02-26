// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_CHROMEOS_COMPONENT_EXTENSION_IME_MANAGER_DELEGATE_H_
#define UI_BASE_IME_CHROMEOS_COMPONENT_EXTENSION_IME_MANAGER_DELEGATE_H_

#include "base/component_export.h"
#include "base/files/file_path.h"
#include "base/macros.h"

class Profile;

namespace chromeos {

struct ComponentExtensionIME;

// Provides an interface to list/load/unload for component extension IME.
class COMPONENT_EXPORT(UI_BASE_IME_CHROMEOS)
    ComponentExtensionIMEManagerDelegate {
 public:
  virtual ~ComponentExtensionIMEManagerDelegate() {}

  // Lists installed component extension IMEs.
  virtual std::vector<ComponentExtensionIME> ListIME() = 0;

  // Loads component extension IME associated with |extension_id|.
  // Returns false if it fails, otherwise returns true.
  virtual void Load(Profile* profile,
                    const std::string& extension_id,
                    const std::string& manifest,
                    const base::FilePath& path) = 0;
};

}  // namespace chromeos

#endif  // UI_BASE_IME_CHROMEOS_COMPONENT_EXTENSION_IME_MANAGER_DELEGATE_H_
