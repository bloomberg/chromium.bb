// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <Carbon/Carbon.h>

#include <cstdint>
#include <string>

#include "base/mac/foundation_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/memory/scoped_policy.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/dom/dom_keyboard_layout_map_base.h"
#include "ui/events/keycodes/dom/keycode_converter.h"

namespace ui {

namespace {

// Per Apple docs, the buffer length can be up to 255 but is rarely more than 4.
// https://developer.apple.com/documentation/coreservices/1390584-uckeytranslate
constexpr int kUCKeyTranslateBufferLength = 4;

class DomKeyboardLayoutMapMac : public ui::DomKeyboardLayoutMapBase {
 public:
  DomKeyboardLayoutMapMac();
  ~DomKeyboardLayoutMapMac() override;

  // ui::DomKeyboardLayoutMapBase implementation.
  uint32_t GetKeyboardLayoutCount() override;
  ui::DomKey GetDomKeyFromDomCodeForLayout(
      ui::DomCode dom_code,
      uint32_t keyboard_layout_index) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(DomKeyboardLayoutMapMac);
};

DomKeyboardLayoutMapMac::DomKeyboardLayoutMapMac() = default;

DomKeyboardLayoutMapMac::~DomKeyboardLayoutMapMac() = default;

uint32_t DomKeyboardLayoutMapMac::GetKeyboardLayoutCount() {
  return 1;
}

ui::DomKey DomKeyboardLayoutMapMac::GetDomKeyFromDomCodeForLayout(
    ui::DomCode dom_code,
    uint32_t keyboard_layout_index) {
  DCHECK_NE(dom_code, ui::DomCode::NONE);
  DCHECK_EQ(keyboard_layout_index, 0U);

  // TISCopyCurrentASCIICapableKeyboardLayoutInputSource() returns the current
  // keyboard layout if it is ASCII capable, otherwise a default ASCII layout is
  // returned (chosen by the OS and favors previously used layouts).
  base::ScopedCFTypeRef<TISInputSourceRef> input_source(
      TISCopyCurrentASCIICapableKeyboardLayoutInputSource());

  CFDataRef layout_data = static_cast<CFDataRef>(TISGetInputSourceProperty(
      input_source, kTISPropertyUnicodeKeyLayoutData));

  const UCKeyboardLayout* keyboard_layout =
      reinterpret_cast<const UCKeyboardLayout*>(CFDataGetBytePtr(layout_data));
  DCHECK(keyboard_layout);

  UInt32 dead_key_state = 0;
  UniChar char_buffer[kUCKeyTranslateBufferLength] = {0};
  UniCharCount buffer_length = kUCKeyTranslateBufferLength;

  // TODO(joedow): Refactor this method call into ui/base/cocoa and update
  // other callers for this API (UCKeyTranslate).
  uint16_t scan_code = ui::KeycodeConverter::DomCodeToNativeKeycode(dom_code);
  OSStatus ret = UCKeyTranslate(keyboard_layout, scan_code, kUCKeyActionDisplay,
                                0, LMGetKbdType(), kUCKeyTranslateNoDeadKeysBit,
                                &dead_key_state, buffer_length, &buffer_length,
                                char_buffer);

  if (ret != noErr)
    return ui::DomKey::NONE;

  if (dead_key_state)
    return ui::DomKey::DeadKeyFromCombiningCharacter(char_buffer[0]);

  return ui::DomKey::FromCharacter(char_buffer[0]);
}

}  // namespace

// static
base::flat_map<std::string, std::string> GenerateDomKeyboardLayoutMap() {
  return DomKeyboardLayoutMapMac().Generate();
}

}  // namespace ui
