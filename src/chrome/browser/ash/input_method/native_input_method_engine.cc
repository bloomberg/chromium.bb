// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/native_input_method_engine.h"

#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/feature_list.h"
#include "base/i18n/i18n_constants.h"
#include "base/i18n/icu_string_conversions.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_offset_string_conversions.h"
#include "base/strings/utf_string_conversion_utils.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/input_method/assistive_suggester_client_filter.h"
#include "chrome/browser/ash/input_method/autocorrect_manager.h"
#include "chrome/browser/ash/input_method/grammar_service_client.h"
#include "chrome/browser/ash/input_method/suggestions_service_client.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/browser/ui/webui/settings/chromeos/constants/routes.mojom.h"
#include "chrome/common/pref_names.h"
#include "chromeos/services/ime/public/mojom/input_method.mojom.h"
#include "components/prefs/pref_service.h"
#include "ui/base/ime/ash/ime_bridge.h"
#include "ui/base/ime/ash/input_method_manager.h"
#include "ui/base/ime/ash/input_method_ukm.h"
#include "ui/events/keycodes/dom/keycode_converter.h"

namespace ash {
namespace input_method {

namespace {

namespace mojom = ::chromeos::ime::mojom;

// The values here should be kept in sync with
// chrome/browser/resources/settings/chromeos/os_languages_page/input_method_util.js
// Although these strings look like UI strings, they are the actual internal
// values stored inside prefs. Therefore, it is important to make sure these
// strings match the settings page exactly.
constexpr char kKoreanPrefsLayoutDubeolsik[] = "2 Set / 두벌식";
constexpr char kKoreanPrefsLayoutDubeolsikOldHangeul[] =
    "2 Set (Old Hangul) / 두벌식 (옛글)";
constexpr char kKoreanPrefsLayoutSebeolsik390[] = "3 Set (390) / 세벌식 (390)";
constexpr char kKoreanPrefsLayoutSebeolsikFinal[] =
    "3 Set (Final) / 세벌식 (최종)";
constexpr char kKoreanPrefsLayoutSebeolsikNoShift[] =
    "3 Set (No Shift) / 세벌식 (순아래)";
constexpr char kKoreanPrefsLayoutSebeolsikOldHangeul[] =
    "3 Set (Old Hangul) / 세벌식 (옛글)";

// Returns the current input context. This may change during the session, even
// if the IME engine does not change.
ui::IMEInputContextHandlerInterface* GetInputContext() {
  return ui::IMEBridge::Get()->GetInputContextHandler();
}

// TODO(https://crbug/1166082): Use a separate InputMethodEngine for rule-based.
bool ShouldRouteToRuleBasedEngine(const std::string& engine_id) {
  return base::StartsWith(engine_id, "vkd_", base::CompareCase::SENSITIVE);
}

IMECandidateWindowHandlerInterface* GetCandidateWindowHandler() {
  return ui::IMEBridge::Get()->GetCandidateWindowHandler();
}

bool IsFstEngine(const std::string& engine_id) {
  return base::StartsWith(engine_id, "xkb:", base::CompareCase::SENSITIVE);
}

bool IsKoreanEngine(const std::string& engine_id) {
  return base::StartsWith(engine_id, "ko-", base::CompareCase::SENSITIVE);
}

bool IsChineseEngine(const std::string& engine_id) {
  return engine_id == "zh-t-i0-pinyin" || engine_id == "zh-hant-t-i0-pinyin" ||
         engine_id == "zh-hant-t-i0-cangjie-1987" ||
         engine_id == "zh-hant-t-i0-cangjie-1987-x-m0-simplified" ||
         engine_id == "yue-hant-t-i0-und" || engine_id == "zh-t-i0-wubi-1986" ||
         engine_id == "zh-hant-t-i0-array-1992" ||
         engine_id == "zh-hant-t-i0-dayi-1988" ||
         engine_id == "zh-hant-t-i0-und";
}

bool IsUsEnglishEngine(const std::string& engine_id) {
  return engine_id == "xkb:us::eng";
}

bool ShouldRouteToNativeMojoEngine(const std::string& engine_id) {
  // To avoid handling tricky cases where the user types with both the virtual
  // and the physical keyboard, only run the native code path if the virtual
  // keyboard is disabled. Otherwise, just let the extension handle any physical
  // key events.
  if (ChromeKeyboardControllerClient::Get()->GetKeyboardEnabled()) {
    return false;
  }

  return (features::IsSystemChinesePhysicalTypingEnabled() &&
          IsChineseEngine(engine_id)) ||
         (features::IsSystemKoreanPhysicalTypingEnabled() &&
          IsKoreanEngine(engine_id)) ||
         (features::IsSystemLatinPhysicalTypingEnabled() &&
          IsFstEngine(engine_id));
}

bool IsPhysicalKeyboardAutocorrectEnabled(PrefService* prefs,
                                          const std::string& engine_id) {
  // The FST Mojo engine is only needed if autocorrect is enabled.
  const base::DictionaryValue* input_method_settings =
      prefs->GetDictionary(::prefs::kLanguageInputMethodSpecificSettings);
  const base::Value* autocorrect_setting = input_method_settings->FindPath(
      engine_id + ".physicalKeyboardAutoCorrectionLevel");
  return autocorrect_setting && autocorrect_setting->GetIfInt().value_or(0) > 0;
}

bool IsPredictiveWritingEnabled(PrefService* pref_service,
                                const std::string& engine_id) {
  return (features::IsAssistiveMultiWordEnabled() &&
          pref_service->GetBoolean(prefs::kAssistPredictiveWritingEnabled) &&
          IsUsEnglishEngine(engine_id));
}

std::string NormalizeRuleBasedEngineId(const std::string engine_id) {
  // For legacy reasons, |engine_id| starts with "vkd_" in the input method
  // manifest, but the InputEngineManager expects the prefix "m17n:".
  // TODO(https://crbug.com/1012490): Migrate to m17n prefix and remove this.
  if (base::StartsWith(engine_id, "vkd_", base::CompareCase::SENSITIVE)) {
    return "m17n:" + engine_id.substr(4);
  }
  return engine_id;
}

mojom::ModifierStatePtr ModifierStateFromEvent(const ui::KeyEvent& event) {
  auto modifier_state = mojom::ModifierState::New();
  modifier_state->alt = event.flags() & ui::EF_ALT_DOWN;
  modifier_state->alt_graph = event.flags() & ui::EF_ALTGR_DOWN;
  modifier_state->caps_lock = event.flags() & ui::EF_CAPS_LOCK_ON;
  modifier_state->control = event.flags() & ui::EF_CONTROL_DOWN;
  modifier_state->shift = event.flags() & ui::EF_SHIFT_DOWN;
  return modifier_state;
}

mojom::InputFieldType TextInputTypeToMojoType(ui::TextInputType type) {
  using mojom::InputFieldType;
  switch (type) {
    case ui::TEXT_INPUT_TYPE_PASSWORD:
      return InputFieldType::kPassword;
    case ui::TEXT_INPUT_TYPE_SEARCH:
      return InputFieldType::kSearch;
    case ui::TEXT_INPUT_TYPE_EMAIL:
      return InputFieldType::kEmail;
    case ui::TEXT_INPUT_TYPE_TELEPHONE:
      return InputFieldType::kTelephone;
    case ui::TEXT_INPUT_TYPE_URL:
      return InputFieldType::kURL;
    case ui::TEXT_INPUT_TYPE_NUMBER:
      return InputFieldType::kNumber;
    case ui::TEXT_INPUT_TYPE_NULL:
      return InputFieldType::kNoIME;
    case ui::TEXT_INPUT_TYPE_TEXT:
      return InputFieldType::kText;
    default:
      return InputFieldType::kText;
  }
}

mojom::AutocorrectMode AutocorrectFlagsToMojoType(int flags) {
  if ((flags & ui::TEXT_INPUT_FLAG_AUTOCORRECT_OFF) ||
      (flags & ui::TEXT_INPUT_FLAG_SPELLCHECK_OFF)) {
    return mojom::AutocorrectMode::kDisabled;
  }
  return mojom::AutocorrectMode::kEnabled;
}

enum class ImeServiceEvent {
  kUnknown = 0,
  kInitSuccess = 1,
  kInitFailed = 2,
  kActivateImeSuccess = 3,
  kActivateImeFailed = 4,
  kServiceDisconnected = 5,
  kMaxValue = kServiceDisconnected
};

void LogEvent(ImeServiceEvent event) {
  UMA_HISTOGRAM_ENUMERATION("InputMethod.Mojo.Extension.Event", event);
}

// Not using a EnumTraits here because the mapping is not 1:1.
mojom::DomCode DomCodeToMojom(const ui::DomCode code) {
  switch (code) {
    case ui::DomCode::BACKQUOTE:
      return mojom::DomCode::kBackquote;
    case ui::DomCode::BACKSLASH:
      return mojom::DomCode::kBackslash;
    case ui::DomCode::BRACKET_LEFT:
      return mojom::DomCode::kBracketLeft;
    case ui::DomCode::BRACKET_RIGHT:
      return mojom::DomCode::kBracketRight;
    case ui::DomCode::COMMA:
      return mojom::DomCode::kComma;
    case ui::DomCode::DIGIT0:
      return mojom::DomCode::kDigit0;
    case ui::DomCode::DIGIT1:
      return mojom::DomCode::kDigit1;
    case ui::DomCode::DIGIT2:
      return mojom::DomCode::kDigit2;
    case ui::DomCode::DIGIT3:
      return mojom::DomCode::kDigit3;
    case ui::DomCode::DIGIT4:
      return mojom::DomCode::kDigit4;
    case ui::DomCode::DIGIT5:
      return mojom::DomCode::kDigit5;
    case ui::DomCode::DIGIT6:
      return mojom::DomCode::kDigit6;
    case ui::DomCode::DIGIT7:
      return mojom::DomCode::kDigit7;
    case ui::DomCode::DIGIT8:
      return mojom::DomCode::kDigit8;
    case ui::DomCode::DIGIT9:
      return mojom::DomCode::kDigit9;
    case ui::DomCode::EQUAL:
      return mojom::DomCode::kEqual;
    case ui::DomCode::INTL_BACKSLASH:
      return mojom::DomCode::kIntlBackslash;
    case ui::DomCode::INTL_RO:
      return mojom::DomCode::kIntlRo;
    case ui::DomCode::INTL_YEN:
      return mojom::DomCode::kIntlYen;
    case ui::DomCode::US_A:
      return mojom::DomCode::kKeyA;
    case ui::DomCode::US_B:
      return mojom::DomCode::kKeyB;
    case ui::DomCode::US_C:
      return mojom::DomCode::kKeyC;
    case ui::DomCode::US_D:
      return mojom::DomCode::kKeyD;
    case ui::DomCode::US_E:
      return mojom::DomCode::kKeyE;
    case ui::DomCode::US_F:
      return mojom::DomCode::kKeyF;
    case ui::DomCode::US_G:
      return mojom::DomCode::kKeyG;
    case ui::DomCode::US_H:
      return mojom::DomCode::kKeyH;
    case ui::DomCode::US_I:
      return mojom::DomCode::kKeyI;
    case ui::DomCode::US_J:
      return mojom::DomCode::kKeyJ;
    case ui::DomCode::US_K:
      return mojom::DomCode::kKeyK;
    case ui::DomCode::US_L:
      return mojom::DomCode::kKeyL;
    case ui::DomCode::US_M:
      return mojom::DomCode::kKeyM;
    case ui::DomCode::US_N:
      return mojom::DomCode::kKeyN;
    case ui::DomCode::US_O:
      return mojom::DomCode::kKeyO;
    case ui::DomCode::US_P:
      return mojom::DomCode::kKeyP;
    case ui::DomCode::US_Q:
      return mojom::DomCode::kKeyQ;
    case ui::DomCode::US_R:
      return mojom::DomCode::kKeyR;
    case ui::DomCode::US_S:
      return mojom::DomCode::kKeyS;
    case ui::DomCode::US_T:
      return mojom::DomCode::kKeyT;
    case ui::DomCode::US_U:
      return mojom::DomCode::kKeyU;
    case ui::DomCode::US_V:
      return mojom::DomCode::kKeyV;
    case ui::DomCode::US_W:
      return mojom::DomCode::kKeyW;
    case ui::DomCode::US_X:
      return mojom::DomCode::kKeyX;
    case ui::DomCode::US_Y:
      return mojom::DomCode::kKeyY;
    case ui::DomCode::US_Z:
      return mojom::DomCode::kKeyZ;
    case ui::DomCode::MINUS:
      return mojom::DomCode::kMinus;
    case ui::DomCode::PERIOD:
      return mojom::DomCode::kPeriod;
    case ui::DomCode::QUOTE:
      return mojom::DomCode::kQuote;
    case ui::DomCode::SEMICOLON:
      return mojom::DomCode::kSemicolon;
    case ui::DomCode::SLASH:
      return mojom::DomCode::kSlash;
    case ui::DomCode::BACKSPACE:
      return mojom::DomCode::kBackspace;
    case ui::DomCode::ENTER:
      return mojom::DomCode::kEnter;
    case ui::DomCode::SPACE:
      return mojom::DomCode::kSpace;
    case ui::DomCode::ALT_LEFT:
      return mojom::DomCode::kAltLeft;
    case ui::DomCode::ALT_RIGHT:
      return mojom::DomCode::kAltRight;
    case ui::DomCode::SHIFT_LEFT:
      return mojom::DomCode::kShiftLeft;
    case ui::DomCode::SHIFT_RIGHT:
      return mojom::DomCode::kShiftRight;
    case ui::DomCode::CONTROL_LEFT:
      return mojom::DomCode::kControlLeft;
    case ui::DomCode::CONTROL_RIGHT:
      return mojom::DomCode::kControlRight;
    case ui::DomCode::CAPS_LOCK:
      return mojom::DomCode::kCapsLock;
    default:
      return mojom::DomCode::kOther;
  }
}

// Not using an EnumTraits here because the mapping is not 1:1.
absl::optional<mojom::NamedDomKey> NamedDomKeyToMojom(
    const ui::DomKey::Base& key) {
  switch (key) {
    case ui::DomKey::ALT:
      return mojom::NamedDomKey::kAlt;
    case ui::DomKey::ALT_GRAPH:
      return mojom::NamedDomKey::kAltGraph;
    case ui::DomKey::CAPS_LOCK:
      return mojom::NamedDomKey::kCapsLock;
    case ui::DomKey::CONTROL:
      return mojom::NamedDomKey::kControl;
    case ui::DomKey::SHIFT:
      return mojom::NamedDomKey::kShift;
    case ui::DomKey::ENTER:
      return mojom::NamedDomKey::kEnter;
    case ui::DomKey::BACKSPACE:
      return mojom::NamedDomKey::kBackspace;
    case ui::DomKey::ESCAPE:
      return mojom::NamedDomKey::kEscape;
    case ui::DomKey::HANGUL_MODE:
      return mojom::NamedDomKey::kHangeulMode;
    case ui::DomKey::HANJA_MODE:
      return mojom::NamedDomKey::kHanjaMode;
    case ui::DomKey::ARROW_DOWN:
      return mojom::NamedDomKey::kArrowDown;
    case ui::DomKey::ARROW_LEFT:
      return mojom::NamedDomKey::kArrowLeft;
    case ui::DomKey::ARROW_RIGHT:
      return mojom::NamedDomKey::kArrowRight;
    case ui::DomKey::ARROW_UP:
      return mojom::NamedDomKey::kArrowUp;
    case ui::DomKey::PAGE_DOWN:
      return mojom::NamedDomKey::kPageDown;
    case ui::DomKey::PAGE_UP:
      return mojom::NamedDomKey::kPageUp;
    default:
      return absl::nullopt;
  }
}

// Returns nullptr if it's not convertible.
// Not using a UnionTraits here because the mapping is not 1:1.
mojom::DomKeyPtr DomKeyToMojom(const ui::DomKey& key) {
  // `IsCharacter` may return true for named keys like Enter because they have a
  // Unicode representation. Hence, try to convert the key into a named key
  // first before trying to convert it to a character key.
  if (ui::KeycodeConverter::IsDomKeyNamed(key)) {
    absl::optional<mojom::NamedDomKey> named_key = NamedDomKeyToMojom(key);
    return named_key ? mojom::DomKey::NewNamedKey(*named_key) : nullptr;
  }
  if (key.IsCharacter()) {
    return mojom::DomKey::NewCodepoint(key.ToCharacter());
  }
  return nullptr;
}

// Returns nullptr if it's not convertible.
// Not using a StructTraits here because the mapping is not 1:1.
mojom::PhysicalKeyEventPtr CreatePhysicalKeyEventFromKeyEvent(
    const ui::KeyEvent& event) {
  mojom::DomKeyPtr key = DomKeyToMojom(event.GetDomKey());
  if (!key) {
    return nullptr;
  }

  return mojom::PhysicalKeyEvent::New(
      event.type() == ui::ET_KEY_PRESSED ? mojom::KeyEventType::kKeyDown
                                         : mojom::KeyEventType::kKeyUp,
      std::move(key), DomCodeToMojom(event.code()),
      ModifierStateFromEvent(event));
}

uint32_t Utf8ToCodepoint(const std::string& str) {
  int32_t index = 0;
  uint32_t codepoint = 0;
  base::ReadUnicodeCharacter(str.data(), str.length(), &index, &codepoint);

  // Should only contain a single codepoint.
  DCHECK_EQ(index, str.length() - 1);
  return codepoint;
}

uint32_t Utf16ToCodepoint(const std::u16string& str) {
  int32_t index = 0;
  uint32_t codepoint = 0;
  base::ReadUnicodeCharacter(str.data(), str.length(), &index, &codepoint);

  // Should only contain a single codepoint.
  DCHECK_EQ(index, str.length() - 1);
  return codepoint;
}

ui::ImeTextSpan::Thickness GetCompositionSpanThickness(
    const mojom::CompositionSpanStyle& style) {
  switch (style) {
    case mojom::CompositionSpanStyle::kNone:
      return ui::ImeTextSpan::Thickness::kNone;
    case mojom::CompositionSpanStyle::kDefault:
      return ui::ImeTextSpan::Thickness::kThin;
  }
}

// Not using a StructTraits here because the mapping is not 1:1.
ui::ImeTextSpan CompositionSpanToImeTextSpan(
    const mojom::CompositionSpan& span) {
  return ui::ImeTextSpan(ui::ImeTextSpan::Type::kComposition, span.start,
                         span.end, GetCompositionSpanThickness(span.style),
                         span.style == mojom::CompositionSpanStyle::kNone
                             ? ui::ImeTextSpan::UnderlineStyle::kNone
                             : ui::ImeTextSpan::UnderlineStyle::kSolid);
}

mojom::KoreanLayout KoreanLayoutToMojom(const std::string& layout) {
  if (layout == kKoreanPrefsLayoutDubeolsik)
    return mojom::KoreanLayout::kDubeolsik;
  if (layout == kKoreanPrefsLayoutDubeolsikOldHangeul)
    return mojom::KoreanLayout::kDubeolsikOldHangeul;
  if (layout == kKoreanPrefsLayoutSebeolsik390)
    return mojom::KoreanLayout::kSebeolsik390;
  if (layout == kKoreanPrefsLayoutSebeolsikFinal)
    return mojom::KoreanLayout::kSebeolsikFinal;
  if (layout == kKoreanPrefsLayoutSebeolsikNoShift)
    return mojom::KoreanLayout::kSebeolsikNoShift;
  if (layout == kKoreanPrefsLayoutSebeolsikOldHangeul)
    return mojom::KoreanLayout::kSebeolsikOldHangeul;
  return mojom::KoreanLayout::kDubeolsik;
}

mojom::InputMethodSettingsPtr CreateSettingsFromPrefs(
    const std::string& engine_id,
    PrefService* prefs) {
  if (!prefs) {
    return nullptr;
  }

  const base::DictionaryValue* prefs_settings =
      prefs->GetDictionary(::prefs::kLanguageInputMethodSpecificSettings);

  // TODO(b/151884011): Extend this to other input methods like Pinyin.
  if (IsFstEngine(engine_id)) {
    auto latin_settings = mojom::LatinSettings::New();
    latin_settings->autocorrect =
        prefs_settings
            ->FindIntPath(engine_id + ".physicalKeyboardAutoCorrectionLevel")
            .value_or(0) > 0;
    latin_settings->predictive_writing =
        IsPredictiveWritingEnabled(prefs, engine_id);
    return mojom::InputMethodSettings::NewLatinSettings(
        std::move(latin_settings));
  } else if (IsKoreanEngine(engine_id)) {
    auto korean_settings = mojom::KoreanSettings::New();
    korean_settings->input_multiple_syllables =
        !prefs_settings->FindBoolPath(engine_id + ".koreanEnableSyllableInput")
             .value_or(true);
    const std::string* prefs_layout =
        prefs_settings->FindStringPath(engine_id + ".koreanKeyboardLayout");
    korean_settings->layout = prefs_layout ? KoreanLayoutToMojom(*prefs_layout)
                                           : mojom::KoreanLayout::kDubeolsik;
    return mojom::InputMethodSettings::NewKoreanSettings(
        std::move(korean_settings));
  }

  return nullptr;
}

void OnConnected(bool bound) {
  LogEvent(bound ? ImeServiceEvent::kActivateImeSuccess
                 : ImeServiceEvent::kActivateImeFailed);
}

void OnError(base::Time start) {
  LOG(ERROR) << "IME Service connection error";

  // If the Mojo pipe disconnection happens in 1000 ms after the service
  // is initialized, we consider it as a failure. Normally it's caused
  // by the Mojo service itself or misconfigured on Chrome OS.
  if (base::Time::Now() - start < base::Milliseconds(1000)) {
    LogEvent(ImeServiceEvent::kInitFailed);
  } else {
    LogEvent(ImeServiceEvent::kServiceDisconnected);
  }
}

}  // namespace

NativeInputMethodEngine::NativeInputMethodEngine() = default;

NativeInputMethodEngine::~NativeInputMethodEngine() = default;

void NativeInputMethodEngine::Initialize(
    std::unique_ptr<InputMethodEngineBase::Observer> observer,
    const char* extension_id,
    Profile* profile) {
  // TODO(crbug/1141231): refactor the mix of unique and raw ptr here.
  std::unique_ptr<AssistiveSuggester> assistive_suggester =
      std::make_unique<AssistiveSuggester>(
          this, profile, std::make_unique<AssistiveSuggesterClientFilter>());
  assistive_suggester_ = assistive_suggester.get();
  std::unique_ptr<AutocorrectManager> autocorrect_manager =
      std::make_unique<AutocorrectManager>(this);
  autocorrect_manager_ = autocorrect_manager.get();

  auto suggestions_service_client =
      features::IsAssistiveMultiWordEnabled()
          ? std::make_unique<SuggestionsServiceClient>()
          : nullptr;

  auto suggestions_collector =
      features::IsAssistiveMultiWordEnabled()
          ? std::make_unique<SuggestionsCollector>(
                assistive_suggester_, std::move(suggestions_service_client))
          : nullptr;

  chrome_keyboard_controller_client_observer_.Observe(
      ChromeKeyboardControllerClient::Get());

  // Wrap the given observer in our observer that will decide whether to call
  // Mojo directly or forward to the extension.
  auto native_observer = std::make_unique<NativeInputMethodEngine::ImeObserver>(
      profile->GetPrefs(), std::move(observer), std::move(assistive_suggester),
      std::move(autocorrect_manager), std::move(suggestions_collector),
      std::make_unique<GrammarManager>(
          profile, std::make_unique<GrammarServiceClient>(), this));
  InputMethodEngine::Initialize(std::move(native_observer), extension_id,
                                profile);
}

void NativeInputMethodEngine::CandidateClicked(uint32_t index) {
  // The parent implementation will try to convert `index` into a candidate ID.
  // The native Mojo engine doesn't use candidate IDs, so we just treat `index`
  // as the ID, without doing a mapping.
  if (ShouldRouteToNativeMojoEngine(GetActiveComponentId())) {
    GetNativeObserver()->OnCandidateClicked(
        GetActiveComponentId(), index,
        InputMethodEngineBase::MOUSE_BUTTON_LEFT);
  } else {
    InputMethodEngine::CandidateClicked(index);
  }
}

void NativeInputMethodEngine::OnKeyboardEnabledChanged(bool enabled) {
  // Re-activate the engine whenever the virtual keyboard is enabled or disabled
  // so that the native or extension state is reset correctly.
  Enable(GetActiveComponentId());
}

void NativeInputMethodEngine::OnProfileWillBeDestroyed(Profile* profile) {
  InputMethodEngine::OnProfileWillBeDestroyed(profile);
  GetNativeObserver()->OnProfileWillBeDestroyed();
}

void NativeInputMethodEngine::FlushForTesting() {
  GetNativeObserver()->FlushForTesting();
}

bool NativeInputMethodEngine::IsConnectedForTesting() const {
  return GetNativeObserver()->IsConnectedForTesting();
}

void NativeInputMethodEngine::OnAutocorrect(
    const std::u16string& typed_word,
    const std::u16string& corrected_word,
    int start_index) {
  autocorrect_manager_->HandleAutocorrect(
      gfx::Range(start_index, start_index + corrected_word.length()),
      typed_word, corrected_word);
}

NativeInputMethodEngine::ImeObserver*
NativeInputMethodEngine::GetNativeObserver() const {
  return static_cast<ImeObserver*>(observer_.get());
}

NativeInputMethodEngine::ImeObserver::ImeObserver(
    PrefService* prefs,
    std::unique_ptr<InputMethodEngineBase::Observer> ime_base_observer,
    std::unique_ptr<AssistiveSuggester> assistive_suggester,
    std::unique_ptr<AutocorrectManager> autocorrect_manager,
    std::unique_ptr<SuggestionsCollector> suggestions_collector,
    std::unique_ptr<GrammarManager> grammar_manager)
    : prefs_(prefs),
      ime_base_observer_(std::move(ime_base_observer)),
      assistive_suggester_(std::move(assistive_suggester)),
      autocorrect_manager_(std::move(autocorrect_manager)),
      suggestions_collector_(std::move(suggestions_collector)),
      grammar_manager_(std::move(grammar_manager)) {}

NativeInputMethodEngine::ImeObserver::~ImeObserver() = default;

void NativeInputMethodEngine::ImeObserver::OnActivate(
    const std::string& engine_id) {
  // TODO(b/181077907): Always launch the IME service and let IME service decide
  // whether it should shutdown or not.
  if (IsFstEngine(engine_id) && ShouldRouteToNativeMojoEngine(engine_id) &&
      !IsPhysicalKeyboardAutocorrectEnabled(prefs_, engine_id) &&
      !IsPredictiveWritingEnabled(prefs_, engine_id)) {
    remote_manager_.reset();
    input_method_.reset();
    host_receiver_.reset();
    return;
  }

  if (ShouldRouteToRuleBasedEngine(engine_id)) {
    if (!remote_manager_.is_bound()) {
      auto* ime_manager = InputMethodManager::Get();
      ime_manager->ConnectInputEngineManager(
          remote_manager_.BindNewPipeAndPassReceiver());
      remote_manager_.set_disconnect_handler(
          base::BindOnce(&OnError, base::Time::Now()));
      LogEvent(ImeServiceEvent::kInitSuccess);
    }

    const auto new_engine_id = NormalizeRuleBasedEngineId(engine_id);

    // Deactivate any existing engine.
    input_method_.reset();
    host_receiver_.reset();

    remote_manager_->ConnectToInputMethod(
        new_engine_id, input_method_.BindNewPipeAndPassReceiver(),
        host_receiver_.BindNewPipeAndPassRemote(),
        base::BindOnce(&OnConnected));

    // Notify the virtual keyboard extension that the IME has changed.
    ime_base_observer_->OnActivate(engine_id);
  } else if (ShouldRouteToNativeMojoEngine(engine_id)) {
    if (!remote_manager_.is_bound()) {
      auto* ime_manager = InputMethodManager::Get();
      ime_manager->ConnectInputEngineManager(
          remote_manager_.BindNewPipeAndPassReceiver());
      remote_manager_.set_disconnect_handler(
          base::BindOnce(&OnError, base::Time::Now()));
      LogEvent(ImeServiceEvent::kInitSuccess);
    }

    // Deactivate any existing engine.
    input_method_.reset();
    host_receiver_.reset();

    remote_manager_->ConnectToInputMethod(
        engine_id, input_method_.BindNewPipeAndPassReceiver(),
        host_receiver_.BindNewPipeAndPassRemote(),
        base::BindOnce(&OnConnected));
  } else {
    // Release the IME service.
    // TODO(b/147709499): A better way to cleanup all.
    remote_manager_.reset();
    input_method_.reset();
    host_receiver_.reset();

    ime_base_observer_->OnActivate(engine_id);
  }
}

void NativeInputMethodEngine::ImeObserver::OnFocus(
    const std::string& engine_id,
    int context_id,
    const IMEEngineHandlerInterface::InputContext& context) {
  if (assistive_suggester_->IsAssistiveFeatureEnabled()) {
    assistive_suggester_->OnFocus(context_id);
  }
  autocorrect_manager_->OnFocus(context_id);
  if (grammar_manager_->IsOnDeviceGrammarEnabled()) {
    grammar_manager_->OnFocus(context_id, context.flags);
  }
  if (ShouldRouteToNativeMojoEngine(engine_id)) {
    if (input_method_.is_bound()) {
      input_method_->OnFocus(mojom::InputFieldInfo::New(
                                 TextInputTypeToMojoType(context.type),
                                 AutocorrectFlagsToMojoType(context.flags),
                                 context.should_do_learning
                                     ? mojom::PersonalizationMode::kEnabled
                                     : mojom::PersonalizationMode::kDisabled),
                             CreateSettingsFromPrefs(engine_id, prefs_));

      // TODO(b/202224495): Send the surrounding text as part of InputFieldInfo.
      SendSurroundingTextToNativeMojoEngine(last_surrounding_text_);
    }
  } else {
    ime_base_observer_->OnFocus(engine_id, context_id, context);
  }
}

void NativeInputMethodEngine::ImeObserver::OnBlur(const std::string& engine_id,
                                                  int context_id) {
  if (assistive_suggester_->IsAssistiveFeatureEnabled())
    assistive_suggester_->OnBlur();

  if (ShouldRouteToNativeMojoEngine(engine_id)) {
    if (input_method_.is_bound()) {
      input_method_->OnBlur();
    }
  } else {
    ime_base_observer_->OnBlur(engine_id, context_id);
  }
}

void NativeInputMethodEngine::ImeObserver::OnKeyEvent(
    const std::string& engine_id,
    const ui::KeyEvent& event,
    ui::IMEEngineHandlerInterface::KeyEventDoneCallback callback) {
  if (assistive_suggester_->IsAssistiveFeatureEnabled()) {
    if (assistive_suggester_->OnKeyEvent(event)) {
      std::move(callback).Run(true);
      return;
    }
  }
  if (autocorrect_manager_->OnKeyEvent(event)) {
    std::move(callback).Run(true);
    return;
  }
  if (grammar_manager_->IsOnDeviceGrammarEnabled() &&
      grammar_manager_->OnKeyEvent(event)) {
    std::move(callback).Run(true);
    return;
  }

  if (ShouldRouteToRuleBasedEngine(engine_id) ||
      ShouldRouteToNativeMojoEngine(engine_id)) {
    if (input_method_.is_bound()) {
      // CharacterComposer only takes KEY_PRESSED events.
      const bool filtered = event.type() == ui::ET_KEY_PRESSED &&
                            character_composer_.FilterKeyPress(event);

      // Don't send dead keys to the system IME. Dead keys should be handled at
      // the OS level and not exposed to IMEs.
      if (event.GetDomKey().IsDeadKey()) {
        std::move(callback).Run(true);
        return;
      }

      mojom::PhysicalKeyEventPtr key_event =
          CreatePhysicalKeyEventFromKeyEvent(event);
      if (!key_event) {
        std::move(callback).Run(false);
        return;
      }
      if (filtered) {
        // TODO(b/174612548): Transform the corresponding KEY_RELEASED event to
        // use the composed character as well.
        key_event->key = mojom::DomKey::NewCodepoint(
            Utf16ToCodepoint(character_composer_.composed_character()));
      }

      input_method_->ProcessKeyEvent(
          std::move(key_event),
          base::BindOnce(
              [](ui::IMEEngineHandlerInterface::KeyEventDoneCallback
                     original_callback,
                 mojom::KeyEventResult result) {
                std::move(original_callback)
                    .Run(result == mojom::KeyEventResult::kConsumedByIme);
              },
              std::move(callback)));
    } else {
      std::move(callback).Run(false);
    }
  } else {
    ime_base_observer_->OnKeyEvent(engine_id, event, std::move(callback));
  }
}

void NativeInputMethodEngine::ImeObserver::OnReset(
    const std::string& engine_id) {
  if (ShouldRouteToNativeMojoEngine(engine_id) ||
      ShouldRouteToRuleBasedEngine(engine_id)) {
    if (input_method_.is_bound()) {
      input_method_->OnCompositionCanceledBySystem();
    }
  } else {
    ime_base_observer_->OnReset(engine_id);
  }
}

void NativeInputMethodEngine::ImeObserver::OnDeactivated(
    const std::string& engine_id) {
  if (ShouldRouteToRuleBasedEngine(engine_id)) {
    input_method_.reset();
  }
  ime_base_observer_->OnDeactivated(engine_id);
}

void NativeInputMethodEngine::ImeObserver::OnCompositionBoundsChanged(
    const std::vector<gfx::Rect>& bounds) {
  ime_base_observer_->OnCompositionBoundsChanged(bounds);
}

void NativeInputMethodEngine::ImeObserver::OnSurroundingTextChanged(
    const std::string& engine_id,
    const std::u16string& text,
    int cursor_pos,
    int anchor_pos,
    int offset_pos) {
  DCHECK_GE(cursor_pos, 0);
  DCHECK_GE(anchor_pos, 0);

  last_surrounding_text_ = SurroundingText{.text = text,
                                           .cursor_pos = cursor_pos,
                                           .anchor_pos = anchor_pos,
                                           .offset_pos = offset_pos};

  assistive_suggester_->RecordAssistiveMatchMetrics(text, cursor_pos,
                                                    anchor_pos);
  if (assistive_suggester_->IsAssistiveFeatureEnabled()) {
    assistive_suggester_->OnSurroundingTextChanged(text, cursor_pos,
                                                   anchor_pos);
  }
  autocorrect_manager_->OnSurroundingTextChanged(text, cursor_pos, anchor_pos);
  if (grammar_manager_->IsOnDeviceGrammarEnabled()) {
    grammar_manager_->OnSurroundingTextChanged(text, cursor_pos, anchor_pos);
  }
  if (ShouldRouteToNativeMojoEngine(engine_id)) {
    if (input_method_.is_bound()) {
      SendSurroundingTextToNativeMojoEngine(last_surrounding_text_);
    }
  } else {
    ime_base_observer_->OnSurroundingTextChanged(engine_id, text, cursor_pos,
                                                 anchor_pos, offset_pos);
  }
}

void NativeInputMethodEngine::ImeObserver::OnCandidateClicked(
    const std::string& component_id,
    int candidate_id,
    InputMethodEngineBase::MouseButtonEvent button) {
  if (ShouldRouteToNativeMojoEngine(component_id)) {
    if (input_method_.is_bound()) {
      input_method_->OnCandidateSelected(candidate_id);
    }
  } else {
    ime_base_observer_->OnCandidateClicked(component_id, candidate_id, button);
  }
}

void NativeInputMethodEngine::ImeObserver::OnAssistiveWindowButtonClicked(
    const ui::ime::AssistiveWindowButton& button) {
  switch (button.id) {
    case ui::ime::ButtonId::kSmartInputsSettingLink:
      base::RecordAction(base::UserMetricsAction(
          "ChromeOS.Settings.SmartInputs.PersonalInfoSuggestions.Open"));
      // TODO(crbug/1101689): Add subpath for personal info suggestions
      // settings.
      chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
          ProfileManager::GetActiveUserProfile(),
          chromeos::settings::mojom::kSmartInputsSubpagePath);
      break;
    case ui::ime::ButtonId::kLearnMore:
      if (button.window_type ==
          ui::ime::AssistiveWindowType::kEmojiSuggestion) {
        base::RecordAction(base::UserMetricsAction(
            "ChromeOS.Settings.SmartInputs.EmojiSuggestions.Open"));
        // TODO(crbug/1101689): Add subpath for emoji suggestions settings.
        chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
            ProfileManager::GetActiveUserProfile(),
            chromeos::settings::mojom::kSmartInputsSubpagePath);
      }
      break;
    case ui::ime::ButtonId::kSuggestion:
      if (assistive_suggester_->IsAssistiveFeatureEnabled()) {
        assistive_suggester_->AcceptSuggestion(button.index);
      }
      if (grammar_manager_->IsOnDeviceGrammarEnabled()) {
        grammar_manager_->AcceptSuggestion();
      }
      break;
    case ui::ime::ButtonId::kUndo:
      autocorrect_manager_->UndoAutocorrect();
      break;
    case ui::ime::ButtonId::kIgnoreSuggestion:
      if (grammar_manager_->IsOnDeviceGrammarEnabled()) {
        grammar_manager_->IgnoreSuggestion();
      }
      break;
    case ui::ime::ButtonId::kAddToDictionary:
    case ui::ime::ButtonId::kNone:
      ime_base_observer_->OnAssistiveWindowButtonClicked(button);
      break;
  }
}

void NativeInputMethodEngine::ImeObserver::OnMenuItemActivated(
    const std::string& component_id,
    const std::string& menu_id) {
  ime_base_observer_->OnMenuItemActivated(component_id, menu_id);
}

void NativeInputMethodEngine::ImeObserver::OnScreenProjectionChanged(
    bool is_projected) {
  ime_base_observer_->OnScreenProjectionChanged(is_projected);
}

void NativeInputMethodEngine::ImeObserver::OnSuggestionsGathered(
    RequestSuggestionsCallback callback,
    mojom::SuggestionsResponsePtr response) {
  std::move(callback).Run(std::move(response));
}

void NativeInputMethodEngine::ImeObserver::OnSuggestionsChanged(
    const std::vector<std::string>& suggestions) {
  ime_base_observer_->OnSuggestionsChanged(suggestions);
}

void NativeInputMethodEngine::ImeObserver::OnInputMethodOptionsChanged(
    const std::string& engine_id) {
  ime_base_observer_->OnInputMethodOptionsChanged(engine_id);
}

void NativeInputMethodEngine::ImeObserver::CommitText(
    const std::u16string& text,
    mojom::CommitTextCursorBehavior cursor_behavior) {
  GetInputContext()->CommitText(
      text,
      cursor_behavior == mojom::CommitTextCursorBehavior::kMoveCursorBeforeText
          ? ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorBeforeText
          : ui::TextInputClient::InsertTextCursorBehavior::
                kMoveCursorAfterText);
}

void NativeInputMethodEngine::ImeObserver::SetComposition(
    const std::u16string& text,
    std::vector<mojom::CompositionSpanPtr> spans) {
  ui::CompositionText composition;
  composition.text = text;

  composition.ime_text_spans.reserve(spans.size());
  for (const auto& span : spans) {
    composition.ime_text_spans.push_back(CompositionSpanToImeTextSpan(*span));
  }

  GetInputContext()->UpdateCompositionText(
      std::move(composition), /*cursor_pos=*/composition.text.length(),
      /*visible=*/true);
}

void NativeInputMethodEngine::ImeObserver::SetCompositionRange(
    uint32_t start_index,
    uint32_t end_index) {
  const auto ordered_range = std::minmax(start_index, end_index);
  // TODO(b/151884011): Turn on underlining for composition-based languages.
  GetInputContext()->SetComposingRange(
      ordered_range.first, ordered_range.second,
      {ui::ImeTextSpan(
          ui::ImeTextSpan::Type::kComposition, /*start_offset=*/0,
          /*end_offset=*/ordered_range.second - ordered_range.first,
          ui::ImeTextSpan::Thickness::kNone,
          ui::ImeTextSpan::UnderlineStyle::kNone)});
}

void NativeInputMethodEngine::ImeObserver::FinishComposition() {
  GetInputContext()->ConfirmCompositionText(/*reset_engine=*/false,
                                            /*keep_selection=*/true);
}

void NativeInputMethodEngine::ImeObserver::DeleteSurroundingText(
    uint32_t num_before_cursor,
    uint32_t num_after_cursor) {
  GetInputContext()->DeleteSurroundingText(
      /*offset=*/-static_cast<int>(num_before_cursor),
      /*length=*/num_before_cursor + num_after_cursor);
}

void NativeInputMethodEngine::ImeObserver::HandleAutocorrect(
    mojom::AutocorrectSpanPtr autocorrect_span) {
  autocorrect_manager_->HandleAutocorrect(autocorrect_span->autocorrect_range,
                                          autocorrect_span->original_text,
                                          autocorrect_span->current_text);
}

void NativeInputMethodEngine::ImeObserver::RequestSuggestions(
    mojom::SuggestionsRequestPtr request,
    RequestSuggestionsCallback callback) {
  suggestions_collector_->GatherSuggestions(
      std::move(request),
      base::BindOnce(
          &NativeInputMethodEngine::ImeObserver::OnSuggestionsGathered,
          base::Unretained(this), std::move(callback)));
}

void NativeInputMethodEngine::ImeObserver::DisplaySuggestions(
    const std::vector<ime::TextSuggestion>& suggestions) {
  assistive_suggester_->OnExternalSuggestionsUpdated(suggestions);
}

void NativeInputMethodEngine::ImeObserver::UpdateCandidatesWindow(
    chromeos::ime::mojom::CandidatesWindowPtr window) {
  if (!GetCandidateWindowHandler()) {
    return;
  }

  ui::CandidateWindow candidate_window;
  if (!window) {
    GetCandidateWindowHandler()->UpdateLookupTable(candidate_window,
                                                   /*visible=*/false);
    return;
  }

  for (const auto& candidate : window->candidates) {
    ui::CandidateWindow::Entry entry;
    entry.value = base::UTF8ToUTF16(candidate->text);
    entry.label = base::UTF8ToUTF16(candidate->label.value_or(""));
    entry.annotation = base::UTF8ToUTF16(candidate->annotation.value_or(""));
    candidate_window.mutable_candidates()->push_back(entry);
  }

  ui::CandidateWindow::CandidateWindowProperty property;
  property.cursor_position = window->highlighted_candidate;
  property.page_size = window->candidates.size();
  property.is_vertical = true;
  candidate_window.SetProperty(property);

  GetCandidateWindowHandler()->UpdateLookupTable(candidate_window,
                                                 /*visible=*/true);
}

void NativeInputMethodEngine::ImeObserver::RecordUkm(mojom::UkmEntryPtr entry) {
  if (entry->is_non_compliant_api()) {
    ui::RecordUkmNonCompliantApi(
        GetInputContext()->GetClientSourceForMetrics(),
        entry->get_non_compliant_api()->non_compliant_operation);
  }
}

void NativeInputMethodEngine::ImeObserver::ReportKoreanAction(
    mojom::KoreanAction action) {
  UMA_HISTOGRAM_ENUMERATION("InputMethod.PhysicalKeyboard.Korean.Action",
                            action);
}

void NativeInputMethodEngine::ImeObserver::ReportKoreanSettings(
    mojom::KoreanSettingsPtr settings) {
  UMA_HISTOGRAM_BOOLEAN("InputMethod.PhysicalKeyboard.Korean.MultipleSyllables",
                        settings->input_multiple_syllables);
  UMA_HISTOGRAM_ENUMERATION("InputMethod.PhysicalKeyboard.Korean.Layout",
                            settings->layout);
}

void NativeInputMethodEngine::ImeObserver::FlushForTesting() {
  remote_manager_.FlushForTesting();
  if (host_receiver_.is_bound())
    host_receiver_.FlushForTesting();
  if (input_method_.is_bound())
    input_method_.FlushForTesting();
}

void NativeInputMethodEngine::ImeObserver::OnProfileWillBeDestroyed() {
  prefs_ = nullptr;
}

void NativeInputMethodEngine::ImeObserver::
    SendSurroundingTextToNativeMojoEngine(
        const SurroundingText& surrounding_text) {
  std::vector<size_t> selection_indices = {
      static_cast<size_t>(surrounding_text.anchor_pos),
      static_cast<size_t>(surrounding_text.cursor_pos)};
  std::string utf8_text = base::UTF16ToUTF8AndAdjustOffsets(
      surrounding_text.text, &selection_indices);

  auto selection = mojom::SelectionRange::New();
  selection->anchor = selection_indices[0];
  selection->focus = selection_indices[1];

  input_method_->OnSurroundingTextChanged(
      std::move(utf8_text), surrounding_text.offset_pos, std::move(selection));
}

void NativeInputMethodEngine::OnInputMethodOptionsChanged() {
  if (ShouldRouteToNativeMojoEngine(GetActiveComponentId())) {
    Enable(GetActiveComponentId());
  } else {
    InputMethodEngine::OnInputMethodOptionsChanged();
  }
}

}  // namespace input_method
}  // namespace ash
