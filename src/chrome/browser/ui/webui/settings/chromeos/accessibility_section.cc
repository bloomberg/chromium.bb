// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/accessibility_section.h"

#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/ash_pref_names.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "chrome/browser/accessibility/accessibility_state_utils.h"
#include "chrome/browser/ui/webui/settings/accessibility_main_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/accessibility_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/search/search_tag_registry.h"
#include "chrome/browser/ui/webui/settings/font_handler.h"
#include "chrome/browser/ui/webui/settings/shared_settings_localized_strings_provider.h"
#include "chrome/browser/ui/webui/settings/tts_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/content_features.h"
#include "media/base/media_switches.h"
#include "ui/accessibility/accessibility_switches.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"

namespace chromeos {
namespace settings {
namespace {

const std::vector<SearchConcept>& GetA11ySearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_A11Y_ALWAYS_SHOW_OPTIONS,
       mojom::kAccessibilitySectionPath,
       mojom::SearchResultIcon::kA11y,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kA11yQuickSettings},
       {IDS_OS_SETTINGS_TAG_A11Y_ALWAYS_SHOW_OPTIONS_ALT1,
        SearchConcept::kAltTagEnd}},
      {IDS_OS_SETTINGS_TAG_A11Y_STICKY_KEYS,
       mojom::kManageAccessibilitySubpagePath,
       mojom::SearchResultIcon::kA11y,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kStickyKeys}},
      {IDS_OS_SETTINGS_TAG_A11Y_LARGE_CURSOR,
       mojom::kManageAccessibilitySubpagePath,
       mojom::SearchResultIcon::kA11y,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kLargeCursor}},
      {IDS_OS_SETTINGS_TAG_A11Y,
       mojom::kAccessibilitySectionPath,
       mojom::SearchResultIcon::kA11y,
       mojom::SearchResultDefaultRank::kHigh,
       mojom::SearchResultType::kSection,
       {.section = mojom::Section::kAccessibility},
       {IDS_OS_SETTINGS_TAG_A11Y_ALT1, SearchConcept::kAltTagEnd}},
      {IDS_OS_SETTINGS_TAG_A11Y_DOCKED_MAGNIFIER,
       mojom::kManageAccessibilitySubpagePath,
       mojom::SearchResultIcon::kA11y,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kDockedMagnifier}},
      {IDS_OS_SETTINGS_TAG_A11y_CHROMEVOX,
       mojom::kManageAccessibilitySubpagePath,
       mojom::SearchResultIcon::kA11y,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kChromeVox},
       {IDS_OS_SETTINGS_TAG_A11y_CHROMEVOX_ALT1, SearchConcept::kAltTagEnd}},
      {IDS_OS_SETTINGS_TAG_A11Y_TABLET_NAVIGATION_BUTTONS,
       mojom::kManageAccessibilitySubpagePath,
       mojom::SearchResultIcon::kA11y,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kTabletNavigationButtons}},
      {IDS_OS_SETTINGS_TAG_A11Y_MONO_AUDIO,
       mojom::kManageAccessibilitySubpagePath,
       mojom::SearchResultIcon::kA11y,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kMonoAudio},
       {IDS_OS_SETTINGS_TAG_A11Y_MONO_AUDIO_ALT1, SearchConcept::kAltTagEnd}},
      {IDS_OS_SETTINGS_TAG_A11Y_TEXT_TO_SPEECH,
       mojom::kManageAccessibilitySubpagePath,
       mojom::SearchResultIcon::kA11y,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSubpage,
       {.subpage = mojom::Subpage::kManageAccessibility}},
      {IDS_OS_SETTINGS_TAG_A11Y_CAPTIONS,
       mojom::kCaptionsSubpagePath,
       mojom::SearchResultIcon::kA11y,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSubpage,
       {.subpage = mojom::Subpage::kCaptions}},
      {IDS_OS_SETTINGS_TAG_A11Y_SPEECH_ENGINES,
       mojom::kTextToSpeechSubpagePath,
       mojom::SearchResultIcon::kA11y,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kTextToSpeechEngines}},
      {IDS_OS_SETTINGS_TAG_A11Y_HIGHLIGHT_CURSOR,
       mojom::kManageAccessibilitySubpagePath,
       mojom::SearchResultIcon::kA11y,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kHighlightCursorWhileMoving}},
      {IDS_OS_SETTINGS_TAG_A11Y_MANAGE,
       mojom::kManageAccessibilitySubpagePath,
       mojom::SearchResultIcon::kA11y,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSubpage,
       {.subpage = mojom::Subpage::kManageAccessibility},
       {IDS_OS_SETTINGS_TAG_A11Y_MANAGE_ALT1, SearchConcept::kAltTagEnd}},
      {IDS_OS_SETTINGS_TAG_A11Y_ON_SCREEN_KEYBOARD,
       mojom::kManageAccessibilitySubpagePath,
       mojom::SearchResultIcon::kA11y,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kOnScreenKeyboard}},
      {IDS_OS_SETTINGS_TAG_A11Y_HIGHLIGHT_TEXT_CARET,
       mojom::kManageAccessibilitySubpagePath,
       mojom::SearchResultIcon::kA11y,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kHighlightTextCaret}},
      {IDS_OS_SETTINGS_TAG_A11Y_DICTATION,
       mojom::kManageAccessibilitySubpagePath,
       mojom::SearchResultIcon::kA11y,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kDictation},
       {IDS_OS_SETTINGS_TAG_A11Y_DICTATION_ALT1, SearchConcept::kAltTagEnd}},
      {IDS_OS_SETTINGS_TAG_A11Y_HIGH_CONTRAST,
       mojom::kManageAccessibilitySubpagePath,
       mojom::SearchResultIcon::kA11y,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kHighContrastMode},
       {IDS_OS_SETTINGS_TAG_A11Y_HIGH_CONTRAST_ALT1,
        SearchConcept::kAltTagEnd}},
      {IDS_OS_SETTINGS_TAG_A11Y_HIGHLIGHT_KEYBOARD_FOCUS,
       mojom::kManageAccessibilitySubpagePath,
       mojom::SearchResultIcon::kA11y,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kSpeakToType}},
      {IDS_OS_SETTINGS_TAG_A11Y_STARTUP_SOUND,
       mojom::kManageAccessibilitySubpagePath,
       mojom::SearchResultIcon::kA11y,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kStartupSound}},
      {IDS_OS_SETTINGS_TAG_A11Y_AUTOMATICALLY_CLICK,
       mojom::kManageAccessibilitySubpagePath,
       mojom::SearchResultIcon::kA11y,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kAutoClickWhenCursorStops}},
      {IDS_OS_SETTINGS_TAG_A11Y_SELECT_TO_SPEAK,
       mojom::kManageAccessibilitySubpagePath,
       mojom::SearchResultIcon::kA11y,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kSelectToSpeak}},
      {IDS_OS_SETTINGS_TAG_A11Y_SPEECH_PITCH,
       mojom::kTextToSpeechSubpagePath,
       mojom::SearchResultIcon::kA11y,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kTextToSpeechPitch}},
      {IDS_OS_SETTINGS_TAG_A11Y_SPEECH_RATE,
       mojom::kTextToSpeechSubpagePath,
       mojom::SearchResultIcon::kA11y,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kTextToSpeechRate}},
      {IDS_OS_SETTINGS_TAG_A11Y_SPEECH_VOLUME,
       mojom::kTextToSpeechSubpagePath,
       mojom::SearchResultIcon::kA11y,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kTextToSpeechVolume}},
      {IDS_OS_SETTINGS_TAG_A11Y_FULLSCREEN_MAGNIFIER,
       mojom::kManageAccessibilitySubpagePath,
       mojom::SearchResultIcon::kA11y,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kFullscreenMagnifier}},
      {IDS_OS_SETTINGS_TAG_A11Y_SPEECH_VOICE_PREVIEW,
       mojom::kTextToSpeechSubpagePath,
       mojom::SearchResultIcon::kA11y,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kTextToSpeechVoice}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetA11ySwitchAccessSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_A11Y_ENABLE_SWITCH_ACCESS,
       mojom::kManageAccessibilitySubpagePath,
       mojom::SearchResultIcon::kA11y,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kEnableSwitchAccess}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetA11ySwitchAccessOnSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_A11Y_SWITCH_ACCESS_ASSIGNMENT,
       mojom::kSwitchAccessOptionsSubpagePath,
       mojom::SearchResultIcon::kA11y,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kSwitchActionAssignment}},
      {IDS_OS_SETTINGS_TAG_A11Y_SWITCH_ACCESS,
       mojom::kSwitchAccessOptionsSubpagePath,
       mojom::SearchResultIcon::kA11y,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSubpage,
       {.subpage = mojom::Subpage::kSwitchAccessOptions}},
      {IDS_OS_SETTINGS_TAG_A11Y_SWITCH_ACCESS_AUTO_SCAN,
       mojom::kSwitchAccessOptionsSubpagePath,
       mojom::SearchResultIcon::kA11y,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kSwitchActionAutoScan}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetA11ySwitchAccessKeyboardSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_A11Y_SWITCH_ACCESS_AUTO_SCAN_KEYBOARD,
       mojom::kSwitchAccessOptionsSubpagePath,
       mojom::SearchResultIcon::kA11y,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kSwitchActionAutoScanKeyboard}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetA11yLabelsSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_A11Y_LABELS_FROM_GOOGLE,
       mojom::kAccessibilitySectionPath,
       mojom::SearchResultIcon::kA11y,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kGetImageDescriptionsFromGoogle}},
  });
  return *tags;
}

bool AreExperimentalA11yLabelsAllowed() {
  return base::FeatureList::IsEnabled(
      ::features::kExperimentalAccessibilityLabels);
}

bool IsSwitchAccessAllowed() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      ::switches::kEnableExperimentalAccessibilitySwitchAccess);
}

bool IsSwitchAccessTextAllowed() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      ::switches::kEnableExperimentalAccessibilitySwitchAccessText);
}

}  // namespace

AccessibilitySection::AccessibilitySection(
    Profile* profile,
    SearchTagRegistry* search_tag_registry,
    PrefService* pref_service)
    : OsSettingsSection(profile, search_tag_registry),
      pref_service_(pref_service) {
  registry()->AddSearchTags(GetA11ySearchConcepts());

  pref_change_registrar_.Init(pref_service_);
  pref_change_registrar_.Add(
      ash::prefs::kAccessibilitySwitchAccessEnabled,
      base::BindRepeating(&AccessibilitySection::UpdateSearchTags,
                          base::Unretained(this)));
  pref_change_registrar_.Add(
      ash::prefs::kAccessibilitySwitchAccessAutoScanEnabled,
      base::BindRepeating(&AccessibilitySection::UpdateSearchTags,
                          base::Unretained(this)));
  UpdateSearchTags();
}

AccessibilitySection::~AccessibilitySection() = default;

void AccessibilitySection::AddLoadTimeData(
    content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"a11yPageTitle", IDS_SETTINGS_ACCESSIBILITY},
      {"a11yWebStore", IDS_SETTINGS_ACCESSIBILITY_WEB_STORE},
      {"moreFeaturesLinkDescription",
       IDS_SETTINGS_MORE_FEATURES_LINK_DESCRIPTION},
      {"accessibleImageLabelsTitle",
       IDS_SETTINGS_ACCESSIBLE_IMAGE_LABELS_TITLE},
      {"accessibleImageLabelsSubtitle",
       IDS_SETTINGS_ACCESSIBLE_IMAGE_LABELS_SUBTITLE},
      {"settingsSliderRoleDescription",
       IDS_SETTINGS_SLIDER_MIN_MAX_ARIA_ROLE_DESCRIPTION},
      {"manageAccessibilityFeatures",
       IDS_SETTINGS_ACCESSIBILITY_MANAGE_ACCESSIBILITY_FEATURES},
      {"optionsInMenuLabel", IDS_SETTINGS_OPTIONS_IN_MENU_LABEL},
      {"largeMouseCursorLabel", IDS_SETTINGS_LARGE_MOUSE_CURSOR_LABEL},
      {"largeMouseCursorSizeLabel", IDS_SETTINGS_LARGE_MOUSE_CURSOR_SIZE_LABEL},
      {"largeMouseCursorSizeDefaultLabel",
       IDS_SETTINGS_LARGE_MOUSE_CURSOR_SIZE_DEFAULT_LABEL},
      {"largeMouseCursorSizeLargeLabel",
       IDS_SETTINGS_LARGE_MOUSE_CURSOR_SIZE_LARGE_LABEL},
      {"highContrastLabel", IDS_SETTINGS_HIGH_CONTRAST_LABEL},
      {"stickyKeysLabel", IDS_SETTINGS_STICKY_KEYS_LABEL},
      {"chromeVoxLabel", IDS_SETTINGS_CHROMEVOX_LABEL},
      {"chromeVoxOptionsLabel", IDS_SETTINGS_CHROMEVOX_OPTIONS_LABEL},
      {"screenMagnifierLabel", IDS_SETTINGS_SCREEN_MAGNIFIER_LABEL},
      {"screenMagnifierZoomLabel", IDS_SETTINGS_SCREEN_MAGNIFIER_ZOOM_LABEL},
      {"dockedMagnifierLabel", IDS_SETTINGS_DOCKED_MAGNIFIER_LABEL},
      {"dockedMagnifierZoomLabel", IDS_SETTINGS_DOCKED_MAGNIFIER_ZOOM_LABEL},
      {"screenMagnifierZoom2x", IDS_SETTINGS_SCREEN_MAGNIFIER_ZOOM_2_X},
      {"screenMagnifierZoom4x", IDS_SETTINGS_SCREEN_MAGNIFIER_ZOOM_4_X},
      {"screenMagnifierZoom6x", IDS_SETTINGS_SCREEN_MAGNIFIER_ZOOM_6_X},
      {"screenMagnifierZoom8x", IDS_SETTINGS_SCREEN_MAGNIFIER_ZOOM_8_X},
      {"screenMagnifierZoom10x", IDS_SETTINGS_SCREEN_MAGNIFIER_ZOOM_10_X},
      {"screenMagnifierZoom12x", IDS_SETTINGS_SCREEN_MAGNIFIER_ZOOM_12_X},
      {"screenMagnifierZoom14x", IDS_SETTINGS_SCREEN_MAGNIFIER_ZOOM_14_X},
      {"screenMagnifierZoom16x", IDS_SETTINGS_SCREEN_MAGNIFIER_ZOOM_16_X},
      {"screenMagnifierZoom18x", IDS_SETTINGS_SCREEN_MAGNIFIER_ZOOM_18_X},
      {"screenMagnifierZoom20x", IDS_SETTINGS_SCREEN_MAGNIFIER_ZOOM_20_X},
      {"tapDraggingLabel", IDS_SETTINGS_TAP_DRAGGING_LABEL},
      {"clickOnStopLabel", IDS_SETTINGS_CLICK_ON_STOP_LABEL},
      {"delayBeforeClickLabel", IDS_SETTINGS_DELAY_BEFORE_CLICK_LABEL},
      {"delayBeforeClickExtremelyShort",
       IDS_SETTINGS_DELAY_BEFORE_CLICK_EXTREMELY_SHORT},
      {"delayBeforeClickVeryShort", IDS_SETTINGS_DELAY_BEFORE_CLICK_VERY_SHORT},
      {"delayBeforeClickShort", IDS_SETTINGS_DELAY_BEFORE_CLICK_SHORT},
      {"delayBeforeClickLong", IDS_SETTINGS_DELAY_BEFORE_CLICK_LONG},
      {"delayBeforeClickVeryLong", IDS_SETTINGS_DELAY_BEFORE_CLICK_VERY_LONG},
      {"autoclickRevertToLeftClick",
       IDS_SETTINGS_AUTOCLICK_REVERT_TO_LEFT_CLICK},
      {"autoclickStabilizeCursorPosition",
       IDS_SETTINGS_AUTOCLICK_STABILIZE_CURSOR_POSITION},
      {"autoclickMovementThresholdLabel",
       IDS_SETTINGS_AUTOCLICK_MOVEMENT_THRESHOLD_LABEL},
      {"autoclickMovementThresholdExtraSmall",
       IDS_SETTINGS_AUTOCLICK_MOVEMENT_THRESHOLD_EXTRA_SMALL},
      {"autoclickMovementThresholdSmall",
       IDS_SETTINGS_AUTOCLICK_MOVEMENT_THRESHOLD_SMALL},
      {"autoclickMovementThresholdDefault",
       IDS_SETTINGS_AUTOCLICK_MOVEMENT_THRESHOLD_DEFAULT},
      {"autoclickMovementThresholdLarge",
       IDS_SETTINGS_AUTOCLICK_MOVEMENT_THRESHOLD_LARGE},
      {"autoclickMovementThresholdExtraLarge",
       IDS_SETTINGS_AUTOCLICK_MOVEMENT_THRESHOLD_EXTRA_LARGE},
      {"dictationDescription",
       IDS_SETTINGS_ACCESSIBILITY_DICTATION_DESCRIPTION},
      {"dictationLabel", IDS_SETTINGS_ACCESSIBILITY_DICTATION_LABEL},
      {"onScreenKeyboardLabel", IDS_SETTINGS_ON_SCREEN_KEYBOARD_LABEL},
      {"monoAudioLabel", IDS_SETTINGS_MONO_AUDIO_LABEL},
      {"startupSoundLabel", IDS_SETTINGS_STARTUP_SOUND_LABEL},
      {"a11yExplanation", IDS_SETTINGS_ACCESSIBILITY_EXPLANATION},
      {"caretHighlightLabel",
       IDS_SETTINGS_ACCESSIBILITY_CARET_HIGHLIGHT_DESCRIPTION},
      {"cursorHighlightLabel",
       IDS_SETTINGS_ACCESSIBILITY_CURSOR_HIGHLIGHT_DESCRIPTION},
      {"focusHighlightLabel",
       IDS_SETTINGS_ACCESSIBILITY_FOCUS_HIGHLIGHT_DESCRIPTION},
      {"selectToSpeakTitle", IDS_SETTINGS_ACCESSIBILITY_SELECT_TO_SPEAK_TITLE},
      {"selectToSpeakDisabledDescription",
       IDS_SETTINGS_ACCESSIBILITY_SELECT_TO_SPEAK_DISABLED_DESCRIPTION},
      {"selectToSpeakDescription",
       IDS_SETTINGS_ACCESSIBILITY_SELECT_TO_SPEAK_DESCRIPTION},
      {"selectToSpeakDescriptionWithoutKeyboard",
       IDS_SETTINGS_ACCESSIBILITY_SELECT_TO_SPEAK_DESCRIPTION_WITHOUT_KEYBOARD},
      {"selectToSpeakOptionsLabel",
       IDS_SETTINGS_ACCESSIBILITY_SELECT_TO_SPEAK_OPTIONS_LABEL},
      {"switchAccessLabel",
       IDS_SETTINGS_ACCESSIBILITY_SWITCH_ACCESS_DESCRIPTION},
      {"switchAccessOptionsLabel",
       IDS_SETTINGS_ACCESSIBILITY_SWITCH_ACCESS_OPTIONS_LABEL},
      {"manageSwitchAccessSettings",
       IDS_SETTINGS_MANAGE_SWITCH_ACCESS_SETTINGS},
      {"switchAssignmentHeading", IDS_SETTINGS_SWITCH_ASSIGNMENT_HEADING},
      {"switchAssignOptionNone", IDS_SETTINGS_SWITCH_ASSIGN_OPTION_NONE},
      {"switchAssignOptionSpace", IDS_SETTINGS_SWITCH_ASSIGN_OPTION_SPACE},
      {"switchAssignOptionEnter", IDS_SETTINGS_SWITCH_ASSIGN_OPTION_ENTER},
      {"assignSelectSwitchLabel", IDS_SETTINGS_ASSIGN_SELECT_SWITCH_LABEL},
      {"assignNextSwitchLabel", IDS_SETTINGS_ASSIGN_NEXT_SWITCH_LABEL},
      {"assignPreviousSwitchLabel", IDS_SETTINGS_ASSIGN_PREVIOUS_SWITCH_LABEL},
      {"switchAccessAutoScanHeading",
       IDS_SETTINGS_SWITCH_ACCESS_AUTO_SCAN_HEADING},
      {"switchAccessAutoScanLabel", IDS_SETTINGS_SWITCH_ACCESS_AUTO_SCAN_LABEL},
      {"switchAccessAutoScanSpeedLabel",
       IDS_SETTINGS_SWITCH_ACCESS_AUTO_SCAN_SPEED_LABEL},
      {"switchAccessAutoScanKeyboardSpeedLabel",
       IDS_SETTINGS_SWITCH_ACCESS_AUTO_SCAN_KEYBOARD_SPEED_LABEL},
      {"durationInSeconds", IDS_SETTINGS_DURATION_IN_SECONDS},
      {"manageAccessibilityFeatures",
       IDS_SETTINGS_ACCESSIBILITY_MANAGE_ACCESSIBILITY_FEATURES},
      {"textToSpeechHeading",
       IDS_SETTINGS_ACCESSIBILITY_TEXT_TO_SPEECH_HEADING},
      {"displayHeading", IDS_SETTINGS_ACCESSIBILITY_DISPLAY_HEADING},
      {"displaySettingsTitle",
       IDS_SETTINGS_ACCESSIBILITY_DISPLAY_SETTINGS_TITLE},
      {"displaySettingsDescription",
       IDS_SETTINGS_ACCESSIBILITY_DISPLAY_SETTINGS_DESCRIPTION},
      {"appearanceSettingsTitle",
       IDS_SETTINGS_ACCESSIBILITY_APPEARANCE_SETTINGS_TITLE},
      {"appearanceSettingsDescription",
       IDS_SETTINGS_ACCESSIBILITY_APPEARANCE_SETTINGS_DESCRIPTION},
      {"keyboardAndTextInputHeading",
       IDS_SETTINGS_ACCESSIBILITY_KEYBOARD_AND_TEXT_INPUT_HEADING},
      {"keyboardSettingsTitle",
       IDS_SETTINGS_ACCESSIBILITY_KEYBOARD_SETTINGS_TITLE},
      {"keyboardSettingsDescription",
       IDS_SETTINGS_ACCESSIBILITY_KEYBOARD_SETTINGS_DESCRIPTION},
      {"mouseAndTouchpadHeading",
       IDS_SETTINGS_ACCESSIBILITY_MOUSE_AND_TOUCHPAD_HEADING},
      {"mouseSettingsTitle", IDS_SETTINGS_ACCESSIBILITY_MOUSE_SETTINGS_TITLE},
      {"mouseSettingsDescription",
       IDS_SETTINGS_ACCESSIBILITY_MOUSE_SETTINGS_DESCRIPTION},
      {"audioAndCaptionsHeading",
       IDS_SETTINGS_ACCESSIBILITY_AUDIO_AND_CAPTIONS_HEADING},
      {"additionalFeaturesTitle",
       IDS_SETTINGS_ACCESSIBILITY_ADDITIONAL_FEATURES_TITLE},
      {"manageTtsSettings", IDS_SETTINGS_MANAGE_TTS_SETTINGS},
      {"ttsSettingsLinkDescription", IDS_SETTINGS_TTS_LINK_DESCRIPTION},
      {"textToSpeechVoices", IDS_SETTINGS_TEXT_TO_SPEECH_VOICES},
      {"textToSpeechNoVoicesMessage",
       IDS_SETTINGS_TEXT_TO_SPEECH_NO_VOICES_MESSAGE},
      {"textToSpeechMoreLanguages", IDS_SETTINGS_TEXT_TO_SPEECH_MORE_LANGUAGES},
      {"textToSpeechProperties", IDS_SETTINGS_TEXT_TO_SPEECH_PROPERTIES},
      {"textToSpeechRate", IDS_SETTINGS_TEXT_TO_SPEECH_RATE},
      {"textToSpeechRateMinimumLabel",
       IDS_SETTINGS_TEXT_TO_SPEECH_RATE_MINIMUM_LABEL},
      {"textToSpeechRateMaximumLabel",
       IDS_SETTINGS_TEXT_TO_SPEECH_RATE_MAXIMUM_LABEL},
      {"textToSpeechPitch", IDS_SETTINGS_TEXT_TO_SPEECH_PITCH},
      {"textToSpeechPitchMinimumLabel",
       IDS_SETTINGS_TEXT_TO_SPEECH_PITCH_MINIMUM_LABEL},
      {"textToSpeechPitchMaximumLabel",
       IDS_SETTINGS_TEXT_TO_SPEECH_PITCH_MAXIMUM_LABEL},
      {"textToSpeechVolume", IDS_SETTINGS_TEXT_TO_SPEECH_VOLUME},
      {"textToSpeechVolumeMinimumLabel",
       IDS_SETTINGS_TEXT_TO_SPEECH_VOLUME_MINIMUM_LABEL},
      {"textToSpeechVolumeMaximumLabel",
       IDS_SETTINGS_TEXT_TO_SPEECH_VOLUME_MAXIMUM_LABEL},
      {"percentage", IDS_SETTINGS_PERCENTAGE},
      {"defaultPercentage", IDS_SETTINGS_DEFAULT_PERCENTAGE},
      {"textToSpeechPreviewHeading",
       IDS_SETTINGS_TEXT_TO_SPEECH_PREVIEW_HEADING},
      {"textToSpeechPreviewInputLabel",
       IDS_SETTINGS_TEXT_TO_SPEECH_PREVIEW_INPUT_LABEL},
      {"textToSpeechPreviewInput", IDS_SETTINGS_TEXT_TO_SPEECH_PREVIEW_INPUT},
      {"textToSpeechPreviewVoice", IDS_SETTINGS_TEXT_TO_SPEECH_PREVIEW_VOICE},
      {"textToSpeechPreviewPlay", IDS_SETTINGS_TEXT_TO_SPEECH_PREVIEW_PLAY},
      {"textToSpeechEngines", IDS_SETTINGS_TEXT_TO_SPEECH_ENGINES},
      {"tabletModeShelfNavigationButtonsSettingLabel",
       IDS_SETTINGS_A11Y_TABLET_MODE_SHELF_BUTTONS_LABEL},
      {"tabletModeShelfNavigationButtonsSettingDescription",
       IDS_SETTINGS_A11Y_TABLET_MODE_SHELF_BUTTONS_DESCRIPTION},
  };
  AddLocalizedStringsBulk(html_source, kLocalizedStrings);

  html_source->AddString("a11yLearnMoreUrl",
                         chrome::kChromeAccessibilityHelpURL);

  html_source->AddBoolean("showExperimentalAccessibilitySwitchAccess",
                          IsSwitchAccessAllowed());
  html_source->AddBoolean(
      "showExperimentalAccessibilitySwitchAccessImprovedTextInput",
      IsSwitchAccessTextAllowed());

  html_source->AddBoolean("showExperimentalA11yLabels",
                          AreExperimentalA11yLabelsAllowed());

  html_source->AddBoolean(
      "showTabletModeShelfNavigationButtonsSettings",
      ash::features::IsHideShelfControlsInTabletModeEnabled());

  html_source->AddString("tabletModeShelfNavigationButtonsLearnMoreUrl",
                         chrome::kTabletModeGesturesLearnMoreURL);

  html_source->AddBoolean("enableLiveCaption",
                          base::FeatureList::IsEnabled(media::kLiveCaption));

  ::settings::AddCaptionSubpageStrings(html_source);
}

void AccessibilitySection::AddHandlers(content::WebUI* web_ui) {
  web_ui->AddMessageHandler(
      std::make_unique<::settings::AccessibilityMainHandler>());
  web_ui->AddMessageHandler(std::make_unique<AccessibilityHandler>(profile()));
  web_ui->AddMessageHandler(std::make_unique<::settings::TtsHandler>());
  web_ui->AddMessageHandler(
      std::make_unique<::settings::FontHandler>(profile()));
}

void AccessibilitySection::UpdateSearchTags() {
  if (accessibility_state_utils::IsScreenReaderEnabled() &&
      AreExperimentalA11yLabelsAllowed()) {
    registry()->AddSearchTags(GetA11yLabelsSearchConcepts());
  } else {
    registry()->RemoveSearchTags(GetA11yLabelsSearchConcepts());
  }

  registry()->RemoveSearchTags(GetA11ySwitchAccessSearchConcepts());
  registry()->RemoveSearchTags(GetA11ySwitchAccessOnSearchConcepts());
  registry()->RemoveSearchTags(GetA11ySwitchAccessKeyboardSearchConcepts());

  if (!IsSwitchAccessAllowed())
    return;

  registry()->AddSearchTags(GetA11ySwitchAccessSearchConcepts());

  if (!pref_service_->GetBoolean(
          ash::prefs::kAccessibilitySwitchAccessEnabled)) {
    return;
  }

  registry()->AddSearchTags(GetA11ySwitchAccessOnSearchConcepts());

  if (pref_service_->GetBoolean(
          ash::prefs::kAccessibilitySwitchAccessAutoScanEnabled)) {
    registry()->AddSearchTags(GetA11ySwitchAccessKeyboardSearchConcepts());
  }
}

}  // namespace settings
}  // namespace chromeos
