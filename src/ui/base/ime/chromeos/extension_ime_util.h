// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_CHROMEOS_EXTENSION_IME_UTIL_H_
#define UI_BASE_IME_CHROMEOS_EXTENSION_IME_UTIL_H_

#include <string>

#include "base/auto_reset.h"
#include "base/component_export.h"

namespace chromeos {

// Extension IME related utilities.
namespace extension_ime_util {

COMPONENT_EXPORT(UI_BASE_IME_CHROMEOS) extern const char kXkbExtensionId[];
COMPONENT_EXPORT(UI_BASE_IME_CHROMEOS) extern const char kM17nExtensionId[];
COMPONENT_EXPORT(UI_BASE_IME_CHROMEOS) extern const char kHangulExtensionId[];
COMPONENT_EXPORT(UI_BASE_IME_CHROMEOS) extern const char kMozcExtensionId[];
COMPONENT_EXPORT(UI_BASE_IME_CHROMEOS) extern const char kT13nExtensionId[];
COMPONENT_EXPORT(UI_BASE_IME_CHROMEOS)
extern const char kChinesePinyinExtensionId[];
COMPONENT_EXPORT(UI_BASE_IME_CHROMEOS)
extern const char kChineseZhuyinExtensionId[];
COMPONENT_EXPORT(UI_BASE_IME_CHROMEOS)
extern const char kChineseCangjieExtensionId[];

// Extension id, path (relative to |chrome::DIR_RESOURCES|) and IME engine
// id for the builtin-in Braille IME extension.
COMPONENT_EXPORT(UI_BASE_IME_CHROMEOS)
extern const char kBrailleImeExtensionId[];
COMPONENT_EXPORT(UI_BASE_IME_CHROMEOS)
extern const char kBrailleImeExtensionPath[];
COMPONENT_EXPORT(UI_BASE_IME_CHROMEOS) extern const char kBrailleImeEngineId[];

// The fake language name used for ARC IMEs.
COMPONENT_EXPORT(UI_BASE_IME_CHROMEOS) extern const char kArcImeLanguage[];

// Returns InputMethodID for |engine_id| in |extension_id| of extension IME.
// This function does not check |extension_id| is installed extension IME nor
// |engine_id| is really a member of |extension_id|.
std::string COMPONENT_EXPORT(UI_BASE_IME_CHROMEOS)
    GetInputMethodID(const std::string& extension_id,
                     const std::string& engine_id);

// Returns InputMethodID for |engine_id| in |extension_id| of component
// extension IME, This function does not check |extension_id| is component one
// nor |engine_id| is really a member of |extension_id|.
std::string COMPONENT_EXPORT(UI_BASE_IME_CHROMEOS)
    GetComponentInputMethodID(const std::string& extension_id,
                              const std::string& engine_id);

// Returns InputMethodID for |engine_id| in |extension_id| of ARC IME.
// This function does not check |extension_id| is one for ARC IME nor
// |engine_id| is really an installed ARC IME.
std::string COMPONENT_EXPORT(UI_BASE_IME_CHROMEOS)
    GetArcInputMethodID(const std::string& extension_id,
                        const std::string& engine_id);

// Returns extension ID if |input_method_id| is extension IME ID or component
// extension IME ID. Otherwise returns an empty string ("").
std::string COMPONENT_EXPORT(UI_BASE_IME_CHROMEOS)
    GetExtensionIDFromInputMethodID(const std::string& input_method_id);

// Returns InputMethodID from engine id (e.g. xkb:fr:fra), or returns itself if
// the |engine_id| is not a known engine id.
// The caller must make sure the |engine_id| is from system input methods
// instead of 3rd party input methods.
std::string COMPONENT_EXPORT(UI_BASE_IME_CHROMEOS)
    GetInputMethodIDByEngineID(const std::string& engine_id);

// Returns true if |input_method_id| is extension IME ID. This function does not
// check |input_method_id| is installed extension IME.
bool COMPONENT_EXPORT(UI_BASE_IME_CHROMEOS)
    IsExtensionIME(const std::string& input_method_id);

// Returns true if |input_method_id| is component extension IME ID. This
// function does not check |input_method_id| is really allowlisted one or not.
// If you want to check |input_method_id| is allowlisted component extension
// IME, please use ComponentExtensionIMEManager::Isallowlisted instead.
bool COMPONENT_EXPORT(UI_BASE_IME_CHROMEOS)
    IsComponentExtensionIME(const std::string& input_method_id);

// Returns true if |input_method_id| is a Arc IME ID. This function does not
// check |input_method_id| is really a installed Arc IME.
bool COMPONENT_EXPORT(UI_BASE_IME_CHROMEOS)
    IsArcIME(const std::string& input_method_id);

// Returns true if the |input_method_id| is the extension based xkb keyboard,
// otherwise returns false.
bool COMPONENT_EXPORT(UI_BASE_IME_CHROMEOS)
    IsKeyboardLayoutExtension(const std::string& input_method_id);

// Returns input method component id from the extension-based InputMethodID
// for component IME extensions. This function does not check that
// |input_method_id| is installed.
std::string COMPONENT_EXPORT(UI_BASE_IME_CHROMEOS)
    GetComponentIDByInputMethodID(const std::string& input_method_id);

// Returns true if |input_method_id| refers to a CrOS 1P experimental
// multilingual input method.
bool COMPONENT_EXPORT(UI_BASE_IME_CHROMEOS)
    IsExperimentalMultilingual(const std::string& input_method_id);

}  // namespace extension_ime_util
}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove when moved to ash.
namespace ash {
namespace extension_ime_util {
using ::chromeos::extension_ime_util::GetArcInputMethodID;
using ::chromeos::extension_ime_util::GetComponentIDByInputMethodID;
using ::chromeos::extension_ime_util::GetComponentInputMethodID;
using ::chromeos::extension_ime_util::GetExtensionIDFromInputMethodID;
using ::chromeos::extension_ime_util::GetInputMethodID;
using ::chromeos::extension_ime_util::GetInputMethodIDByEngineID;
using ::chromeos::extension_ime_util::IsArcIME;
using ::chromeos::extension_ime_util::IsComponentExtensionIME;
using ::chromeos::extension_ime_util::IsExperimentalMultilingual;
using ::chromeos::extension_ime_util::IsExtensionIME;
using ::chromeos::extension_ime_util::kBrailleImeEngineId;
using ::chromeos::extension_ime_util::kBrailleImeExtensionId;
using ::chromeos::extension_ime_util::kChineseCangjieExtensionId;
using ::chromeos::extension_ime_util::kChinesePinyinExtensionId;
using ::chromeos::extension_ime_util::kChineseZhuyinExtensionId;
using ::chromeos::extension_ime_util::kHangulExtensionId;
using ::chromeos::extension_ime_util::kM17nExtensionId;
using ::chromeos::extension_ime_util::kMozcExtensionId;
using ::chromeos::extension_ime_util::kT13nExtensionId;
using ::chromeos::extension_ime_util::kXkbExtensionId;
}  // namespace extension_ime_util
}  // namespace ash

#endif  // UI_BASE_IME_CHROMEOS_EXTENSION_IME_UTIL_H_
