// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/zwp_text_input_manager.h"

#include <text-input-extension-unstable-v1-server-protocol.h>
#include <text-input-unstable-v1-server-protocol.h>
#include <wayland-server-core.h>
#include <wayland-server-protocol-core.h>
#include <xkbcommon/xkbcommon.h>

#include "base/memory/weak_ptr.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_offset_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/exo/display.h"
#include "components/exo/text_input.h"
#include "components/exo/wayland/serial_tracker.h"
#include "components/exo/wayland/server_util.h"
#include "components/exo/xkb_tracker.h"
#include "ui/base/ime/utf_offset.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/dom/keycode_converter.h"

namespace exo {
namespace wayland {

namespace {

////////////////////////////////////////////////////////////////////////////////
// text_input_v1 interface:

class WaylandTextInputDelegate : public TextInput::Delegate {
 public:
  WaylandTextInputDelegate(wl_resource* text_input,
                           const XkbTracker* xkb_tracker,
                           SerialTracker* serial_tracker)
      : text_input_(text_input),
        xkb_tracker_(xkb_tracker),
        serial_tracker_(serial_tracker) {}
  ~WaylandTextInputDelegate() override = default;

  void set_surface(wl_resource* surface) { surface_ = surface; }

  void set_extended_text_input(wl_resource* extended_text_input) {
    extended_text_input_ = extended_text_input;
  }

  bool has_extended_text_input() const { return extended_text_input_; }

  base::WeakPtr<WaylandTextInputDelegate> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  wl_client* client() { return wl_resource_get_client(text_input_); }

  // TextInput::Delegate:
  void Activated() override {
    zwp_text_input_v1_send_enter(text_input_, surface_);
    wl_client_flush(client());
  }

  void Deactivated() override {
    zwp_text_input_v1_send_leave(text_input_);
    wl_client_flush(client());
  }

  void OnVirtualKeyboardVisibilityChanged(bool is_visible) override {
    // The detailed spec of |state| is implementation dependent.
    // So, now we use the lowest bit to indicate whether keyboard is visible.
    // This behavior is consistent with ozone/wayland to support Lacros.
    zwp_text_input_v1_send_input_panel_state(text_input_,
                                             static_cast<uint32_t>(is_visible));
    wl_client_flush(client());
  }

  void SetCompositionText(const ui::CompositionText& composition) override {
    SendPreeditStyle(composition.text, composition.ime_text_spans);

    std::vector<size_t> offsets = {composition.selection.start()};
    const std::string utf8 =
        base::UTF16ToUTF8AndAdjustOffsets(composition.text, &offsets);

    if (offsets[0] != std::string::npos)
      zwp_text_input_v1_send_preedit_cursor(text_input_, offsets[0]);

    zwp_text_input_v1_send_preedit_string(
        text_input_,
        serial_tracker_->GetNextSerial(SerialTracker::EventType::OTHER_EVENT),
        utf8.c_str(), utf8.c_str());

    wl_client_flush(client());
  }

  void Commit(const std::u16string& text) override {
    zwp_text_input_v1_send_commit_string(
        text_input_,
        serial_tracker_->GetNextSerial(SerialTracker::EventType::OTHER_EVENT),
        base::UTF16ToUTF8(text).c_str());
    wl_client_flush(client());
  }

  void SetCursor(base::StringPiece16 surrounding_text,
                 const gfx::Range& selection) override {
    std::vector<size_t> offsets{selection.start(), selection.end()};
    base::UTF16ToUTF8AndAdjustOffsets(surrounding_text, &offsets);
    zwp_text_input_v1_send_cursor_position(text_input_,
                                           static_cast<uint32_t>(offsets[1]),
                                           static_cast<uint32_t>(offsets[0]));
  }

  void DeleteSurroundingText(base::StringPiece16 surrounding_text,
                             const gfx::Range& range) override {
    std::vector<size_t> offsets{range.GetMin(), range.GetMax()};
    base::UTF16ToUTF8AndAdjustOffsets(surrounding_text, &offsets);
    // Currently, the arguments are conflicting with spec.
    // However, the only client, Lacros, also interprets wrongly in the same
    // way so just fixing here could cause visible regression.
    // TODO(crbug.com/1227590): Fix the behavior with versioning.
    zwp_text_input_v1_send_delete_surrounding_text(
        text_input_, static_cast<uint32_t>(offsets[0]),
        static_cast<uint32_t>(offsets[1] - offsets[0]));
  }

  void SendKey(const ui::KeyEvent& event) override {
    uint32_t keysym = xkb_tracker_->GetKeysym(
        ui::KeycodeConverter::DomCodeToNativeKeycode(event.code()));
    bool pressed = (event.type() == ui::ET_KEY_PRESSED);
    // TODO(mukai): consolidate the definition of this modifiers_mask with other
    // similar ones in components/exo/keyboard.cc or arc_ime_service.cc.
    constexpr uint32_t modifiers_mask =
        ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN |
        ui::EF_COMMAND_DOWN | ui::EF_ALTGR_DOWN | ui::EF_MOD3_DOWN;
    // 1-bit shifts to adjust the bitpattern for the modifiers; see also
    // WaylandTextInputDelegate::SendModifiers().
    uint32_t modifiers = (event.flags() & modifiers_mask) >> 1;

    zwp_text_input_v1_send_keysym(
        text_input_, TimeTicksToMilliseconds(event.time_stamp()),
        serial_tracker_->GetNextSerial(SerialTracker::EventType::OTHER_EVENT),
        keysym,
        pressed ? WL_KEYBOARD_KEY_STATE_PRESSED
                : WL_KEYBOARD_KEY_STATE_RELEASED,
        modifiers);
    wl_client_flush(client());
  }

  void OnTextDirectionChanged(base::i18n::TextDirection direction) override {
    uint32_t wayland_direction = ZWP_TEXT_INPUT_V1_TEXT_DIRECTION_AUTO;
    switch (direction) {
      case base::i18n::RIGHT_TO_LEFT:
        wayland_direction = ZWP_TEXT_INPUT_V1_TEXT_DIRECTION_LTR;
        break;
      case base::i18n::LEFT_TO_RIGHT:
        wayland_direction = ZWP_TEXT_INPUT_V1_TEXT_DIRECTION_RTL;
        break;
      case base::i18n::UNKNOWN_DIRECTION:
        LOG(ERROR) << "Unrecognized direction: " << direction;
    }

    zwp_text_input_v1_send_text_direction(
        text_input_,
        serial_tracker_->GetNextSerial(SerialTracker::EventType::OTHER_EVENT),
        wayland_direction);
  }

  void SetCompositionFromExistingText(
      base::StringPiece16 surrounding_text,
      const gfx::Range& cursor,
      const gfx::Range& range,
      const std::vector<ui::ImeTextSpan>& ui_ime_text_spans) override {
    if (!extended_text_input_)
      return;

    uint32_t begin = range.GetMin();
    uint32_t end = range.GetMax();
    SendPreeditStyle(surrounding_text.substr(begin, range.length()),
                     ui_ime_text_spans);

    std::vector<size_t> offsets = {begin, end, cursor.end()};
    base::UTF16ToUTF8AndAdjustOffsets(surrounding_text, &offsets);
    int32_t index =
        static_cast<int32_t>(offsets[0]) - static_cast<int32_t>(offsets[2]);
    uint32_t length = static_cast<uint32_t>(offsets[1] - offsets[0]);
    zcr_extended_text_input_v1_send_set_preedit_region(extended_text_input_,
                                                       index, length);
    wl_client_flush(client());
  }

  void SendPreeditStyle(base::StringPiece16 text,
                        const std::vector<ui::ImeTextSpan>& spans) {
    if (spans.empty())
      return;

    // Convert all offsets from UTF16 to UTF8.
    std::vector<size_t> offsets;
    offsets.reserve(spans.size() * 2);
    for (const auto& span : spans) {
      auto minmax = std::minmax(span.start_offset, span.end_offset);
      offsets.push_back(minmax.first);
      offsets.push_back(minmax.second);
    }
    base::UTF16ToUTF8AndAdjustOffsets(text, &offsets);

    for (size_t i = 0; i < spans.size(); ++i) {
      if (offsets[i * 2] == std::string::npos ||
          offsets[i * 2 + 1] == std::string::npos) {
        // Invalid span is specified.
        continue;
      }
      const auto& span = spans[i];
      const uint32_t begin = offsets[i * 2];
      const uint32_t end = offsets[i * 2 + 1];

      uint32_t style = ZWP_TEXT_INPUT_V1_PREEDIT_STYLE_DEFAULT;
      switch (span.type) {
        case ui::ImeTextSpan::Type::kComposition:
          if (span.thickness == ui::ImeTextSpan::Thickness::kThick) {
            style = ZWP_TEXT_INPUT_V1_PREEDIT_STYLE_HIGHLIGHT;
          } else {
            style = ZWP_TEXT_INPUT_V1_PREEDIT_STYLE_UNDERLINE;
          }
          break;
        case ui::ImeTextSpan::Type::kSuggestion:
          style = ZWP_TEXT_INPUT_V1_PREEDIT_STYLE_SELECTION;
          break;
        case ui::ImeTextSpan::Type::kMisspellingSuggestion:
        case ui::ImeTextSpan::Type::kAutocorrect:
        case ui::ImeTextSpan::Type::kGrammarSuggestion:
          style = ZWP_TEXT_INPUT_V1_PREEDIT_STYLE_INCORRECT;
          break;
      }
      zwp_text_input_v1_send_preedit_styling(text_input_, begin, end - begin,
                                             style);
    }
  }

  wl_resource* text_input_;
  wl_resource* extended_text_input_ = nullptr;
  wl_resource* surface_ = nullptr;

  // Owned by Seat, which is updated before calling the callbacks of this
  // class.
  const XkbTracker* const xkb_tracker_;

  // Owned by Server, which always outlives this delegate.
  SerialTracker* const serial_tracker_;

  base::WeakPtrFactory<WaylandTextInputDelegate> weak_factory_{this};
};

// Holds WeakPtr to WaylandTextInputDelegate, and the lifetime of this class's
// instance is tied to zcr_extended_text_input connection.
// If text_input connection is destroyed earlier than extended_text_input,
// then delegate_ will return nullptr automatically.
class WaylandExtendedTextInput {
 public:
  explicit WaylandExtendedTextInput(
      base::WeakPtr<WaylandTextInputDelegate> delegate)
      : delegate_(delegate) {}
  WaylandExtendedTextInput(const WaylandExtendedTextInput&) = delete;
  WaylandExtendedTextInput& operator=(const WaylandExtendedTextInput&) = delete;
  ~WaylandExtendedTextInput() {
    if (delegate_)
      delegate_->set_extended_text_input(nullptr);
  }

 private:
  base::WeakPtr<WaylandTextInputDelegate> delegate_;
};

void text_input_activate(wl_client* client,
                         wl_resource* resource,
                         wl_resource* seat,
                         wl_resource* surface_resource) {
  TextInput* text_input = GetUserDataAs<TextInput>(resource);
  Surface* surface = GetUserDataAs<Surface>(surface_resource);
  static_cast<WaylandTextInputDelegate*>(text_input->delegate())
      ->set_surface(surface_resource);
  text_input->Activate(surface);

  // Sending modifiers.
  constexpr const char* kModifierNames[] = {
      XKB_MOD_NAME_SHIFT,
      XKB_MOD_NAME_CTRL,
      XKB_MOD_NAME_ALT,
      XKB_MOD_NAME_LOGO,
      "Mod5",
      "Mod3",
  };
  wl_array modifiers;
  wl_array_init(&modifiers);
  for (const char* modifier : kModifierNames) {
    char* p =
        static_cast<char*>(wl_array_add(&modifiers, ::strlen(modifier) + 1));
    ::strcpy(p, modifier);
  }
  zwp_text_input_v1_send_modifiers_map(resource, &modifiers);
  wl_array_release(&modifiers);
}

void text_input_deactivate(wl_client* client,
                           wl_resource* resource,
                           wl_resource* seat) {
  TextInput* text_input = GetUserDataAs<TextInput>(resource);
  text_input->Deactivate();
}

void text_input_show_input_panel(wl_client* client, wl_resource* resource) {
  GetUserDataAs<TextInput>(resource)->ShowVirtualKeyboardIfEnabled();
}

void text_input_hide_input_panel(wl_client* client, wl_resource* resource) {
  GetUserDataAs<TextInput>(resource)->HideVirtualKeyboard();
}

void text_input_reset(wl_client* client, wl_resource* resource) {
  GetUserDataAs<TextInput>(resource)->Reset();
}

void text_input_set_surrounding_text(wl_client* client,
                                     wl_resource* resource,
                                     const char* text,
                                     uint32_t cursor,
                                     uint32_t anchor) {
  TextInput* text_input = GetUserDataAs<TextInput>(resource);
  // TODO(crbug.com/1227590): Selection range should keep cursor/anchor
  // relationship.
  auto minmax = std::minmax(cursor, anchor);
  std::vector<size_t> offsets{minmax.first, minmax.second};
  std::u16string u16_text = base::UTF8ToUTF16AndAdjustOffsets(text, &offsets);
  if (offsets[0] == std::u16string::npos || offsets[1] == std::u16string::npos)
    return;
  text_input->SetSurroundingText(u16_text, gfx::Range(offsets[0], offsets[1]));
}

void text_input_set_content_type(wl_client* client,
                                 wl_resource* resource,
                                 uint32_t hint,
                                 uint32_t purpose) {
  TextInput* text_input = GetUserDataAs<TextInput>(resource);
  ui::TextInputType type = ui::TEXT_INPUT_TYPE_TEXT;
  ui::TextInputMode mode = ui::TEXT_INPUT_MODE_DEFAULT;
  int flags = ui::TEXT_INPUT_FLAG_NONE;
  bool should_do_learning = true;
  if (hint & ZWP_TEXT_INPUT_V1_CONTENT_HINT_AUTO_COMPLETION)
    flags |= ui::TEXT_INPUT_FLAG_AUTOCOMPLETE_ON;
  if (hint & ZWP_TEXT_INPUT_V1_CONTENT_HINT_AUTO_CORRECTION)
    flags |= ui::TEXT_INPUT_FLAG_AUTOCORRECT_ON;
  if (hint & ZWP_TEXT_INPUT_V1_CONTENT_HINT_AUTO_CAPITALIZATION)
    flags |= ui::TEXT_INPUT_FLAG_AUTOCAPITALIZE_SENTENCES;
  if (hint & ZWP_TEXT_INPUT_V1_CONTENT_HINT_LOWERCASE)
    flags |= ui::TEXT_INPUT_FLAG_AUTOCAPITALIZE_NONE;
  if (hint & ZWP_TEXT_INPUT_V1_CONTENT_HINT_UPPERCASE)
    flags |= ui::TEXT_INPUT_FLAG_AUTOCAPITALIZE_CHARACTERS;
  if (hint & ZWP_TEXT_INPUT_V1_CONTENT_HINT_TITLECASE)
    flags |= ui::TEXT_INPUT_FLAG_AUTOCAPITALIZE_WORDS;
  if (hint & ZWP_TEXT_INPUT_V1_CONTENT_HINT_HIDDEN_TEXT) {
    flags |= ui::TEXT_INPUT_FLAG_AUTOCOMPLETE_OFF |
             ui::TEXT_INPUT_FLAG_AUTOCORRECT_OFF;
  }
  if (hint & ZWP_TEXT_INPUT_V1_CONTENT_HINT_SENSITIVE_DATA)
    should_do_learning = false;
  // Unused hints: LATIN, MULTILINE.

  switch (purpose) {
    case ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_DIGITS:
      mode = ui::TEXT_INPUT_MODE_DECIMAL;
      type = ui::TEXT_INPUT_TYPE_NUMBER;
      break;
    case ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_NUMBER:
      mode = ui::TEXT_INPUT_MODE_NUMERIC;
      type = ui::TEXT_INPUT_TYPE_NUMBER;
      break;
    case ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_PHONE:
      mode = ui::TEXT_INPUT_MODE_TEL;
      type = ui::TEXT_INPUT_TYPE_TELEPHONE;
      break;
    case ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_URL:
      mode = ui::TEXT_INPUT_MODE_URL;
      type = ui::TEXT_INPUT_TYPE_URL;
      break;
    case ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_EMAIL:
      mode = ui::TEXT_INPUT_MODE_EMAIL;
      type = ui::TEXT_INPUT_TYPE_EMAIL;
      break;
    case ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_PASSWORD:
      DCHECK(!should_do_learning);
      type = ui::TEXT_INPUT_TYPE_PASSWORD;
      break;
    case ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_DATE:
      type = ui::TEXT_INPUT_TYPE_DATE;
      break;
    case ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_TIME:
      type = ui::TEXT_INPUT_TYPE_TIME;
      break;
    case ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_DATETIME:
      type = ui::TEXT_INPUT_TYPE_DATE_TIME;
      break;
    case ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_NORMAL:
    case ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_ALPHA:
    case ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_NAME:
    case ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_TERMINAL:
      // No special type / mode are set.
      break;
  }

  text_input->SetTypeModeFlags(type, mode, flags, should_do_learning);
}

void text_input_set_cursor_rectangle(wl_client* client,
                                     wl_resource* resource,
                                     int32_t x,
                                     int32_t y,
                                     int32_t width,
                                     int32_t height) {
  GetUserDataAs<TextInput>(resource)->SetCaretBounds(
      gfx::Rect(x, y, width, height));
}

void text_input_set_preferred_language(wl_client* client,
                                       wl_resource* resource,
                                       const char* language) {
  // Nothing needs to be done.
}

void text_input_commit_state(wl_client* client,
                             wl_resource* resource,
                             uint32_t serial) {
  // Nothing needs to be done.
}

void text_input_invoke_action(wl_client* client,
                              wl_resource* resource,
                              uint32_t button,
                              uint32_t index) {
  GetUserDataAs<TextInput>(resource)->Resync();
}

constexpr struct zwp_text_input_v1_interface text_input_v1_implementation = {
    text_input_activate,
    text_input_deactivate,
    text_input_show_input_panel,
    text_input_hide_input_panel,
    text_input_reset,
    text_input_set_surrounding_text,
    text_input_set_content_type,
    text_input_set_cursor_rectangle,
    text_input_set_preferred_language,
    text_input_commit_state,
    text_input_invoke_action,
};

////////////////////////////////////////////////////////////////////////////////
// text_input_manager_v1 interface:

void text_input_manager_create_text_input(wl_client* client,
                                          wl_resource* resource,
                                          uint32_t id) {
  auto* data = GetUserDataAs<WaylandTextInputManager>(resource);

  wl_resource* text_input_resource =
      wl_resource_create(client, &zwp_text_input_v1_interface, 1, id);

  SetImplementation(
      text_input_resource, &text_input_v1_implementation,
      std::make_unique<TextInput>(std::make_unique<WaylandTextInputDelegate>(
          text_input_resource, data->xkb_tracker, data->serial_tracker)));
}

constexpr struct zwp_text_input_manager_v1_interface
    text_input_manager_implementation = {
        text_input_manager_create_text_input,
};

////////////////////////////////////////////////////////////////////////////////
// extended_text_input_v1 interface:

void extended_text_input_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

constexpr struct zcr_extended_text_input_v1_interface
    extended_text_input_implementation = {extended_text_input_destroy};

////////////////////////////////////////////////////////////////////////////////
// text_input_extension_v1 interface:

void text_input_extension_get_extended_text_input(
    wl_client* client,
    wl_resource* resource,
    uint32_t id,
    wl_resource* text_input_resource) {
  TextInput* text_input = GetUserDataAs<TextInput>(text_input_resource);
  auto* delegate =
      static_cast<WaylandTextInputDelegate*>(text_input->delegate());
  if (delegate->has_extended_text_input()) {
    wl_resource_post_error(
        resource, ZCR_TEXT_INPUT_EXTENSION_V1_ERROR_EXTENDED_TEXT_INPUT_EXISTS,
        "text_input has already been associated with a extended_text_input "
        "object");
    return;
  }

  uint32_t version = wl_resource_get_version(resource);
  wl_resource* extended_text_input_resource = wl_resource_create(
      client, &zcr_extended_text_input_v1_interface, version, id);

  delegate->set_extended_text_input(extended_text_input_resource);

  SetImplementation(
      extended_text_input_resource, &extended_text_input_implementation,
      std::make_unique<WaylandExtendedTextInput>(delegate->GetWeakPtr()));
}

constexpr struct zcr_text_input_extension_v1_interface
    text_input_extension_implementation = {
        text_input_extension_get_extended_text_input};

}  // namespace

void bind_text_input_manager(wl_client* client,
                             void* data,
                             uint32_t version,
                             uint32_t id) {
  wl_resource* resource =
      wl_resource_create(client, &zwp_text_input_manager_v1_interface, 1, id);
  wl_resource_set_implementation(resource, &text_input_manager_implementation,
                                 data, nullptr);
}

void bind_text_input_extension(wl_client* client,
                               void* data,
                               uint32_t version,
                               uint32_t id) {
  wl_resource* resource = wl_resource_create(
      client, &zcr_text_input_extension_v1_interface, version, id);
  wl_resource_set_implementation(resource, &text_input_extension_implementation,
                                 data, nullptr);
}

}  // namespace wayland
}  // namespace exo
