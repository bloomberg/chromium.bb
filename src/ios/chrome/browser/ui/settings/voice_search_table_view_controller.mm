// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/voice_search_table_view_controller.h"

#include "base/check_op.h"
#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "components/prefs/pref_member.h"
#include "components/prefs/pref_service.h"
#import "ios/chrome/browser/ui/settings/cells/settings_switch_cell.h"
#import "ios/chrome/browser/ui/settings/cells/settings_switch_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_detail_text_item.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#include "ios/chrome/browser/voice/speech_input_locale_config.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/public/provider/chrome/browser/voice/voice_search_prefs.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierTTS = kSectionIdentifierEnumZero,
  SectionIdentifierLanguages,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeTTSEnabled = kItemTypeEnumZero,
  ItemTypeLanguagesLanguageOption,
};

}  // namespace

@interface VoiceSearchTableViewController () {
  PrefService* _prefs;  // weak
  StringPrefMember _selectedLanguage;
  BooleanPrefMember _ttsEnabled;
}
// Updates all cells to check the selected language and uncheck all the other.
- (void)markAsCheckedLanguageAtIndex:(NSUInteger)index;

// Returns YES if the current language supports TTS.
- (BOOL)currentLanguageSupportsTTS;
@end

@implementation VoiceSearchTableViewController

- (instancetype)initWithPrefs:(PrefService*)prefs {
  UITableViewStyle style = base::FeatureList::IsEnabled(kSettingsRefresh)
                               ? UITableViewStylePlain
                               : UITableViewStyleGrouped;
  self = [super initWithStyle:style];
  if (self) {
    self.title = l10n_util::GetNSString(IDS_IOS_VOICE_SEARCH_SETTING_TITLE);
    _prefs = prefs;
    _selectedLanguage.Init(prefs::kVoiceSearchLocale, _prefs);
    _ttsEnabled.Init(prefs::kVoiceSearchTTS, _prefs);
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.tableView.rowHeight = UITableViewAutomaticDimension;

  [self loadModel];
}

#pragma mark - ChromeTableViewController

- (void)loadModel {
  [super loadModel];
  TableViewModel* model = self.tableViewModel;

  // TTS section.
  [model addSectionWithIdentifier:SectionIdentifierTTS];
  SettingsSwitchItem* tts =
      [[SettingsSwitchItem alloc] initWithType:ItemTypeTTSEnabled];
  tts.text = l10n_util::GetNSString(IDS_IOS_VOICE_SEARCH_SETTING_TTS);
  BOOL enabled = [self currentLanguageSupportsTTS];
  tts.on = enabled && _ttsEnabled.GetValue();
  tts.enabled = enabled;
  [model addItem:tts toSectionWithIdentifier:SectionIdentifierTTS];

  // Variables used to populate the languages section.
  voice::SpeechInputLocaleConfig* localeConfig =
      voice::SpeechInputLocaleConfig::GetInstance();
  const std::vector<voice::SpeechInputLocale>& locales =
      localeConfig->GetAvailableLocales();
  std::string selectedLocaleCode = _selectedLanguage.GetValue();

  // Section with the list of voice search languages.
  [model addSectionWithIdentifier:SectionIdentifierLanguages];
  // Add default locale option.  Using an empty string for the voice search
  // locale pref indicates using the default locale.
  TableViewDetailTextItem* defaultItem = [[TableViewDetailTextItem alloc]
      initWithType:ItemTypeLanguagesLanguageOption];
  defaultItem.text =
      l10n_util::GetNSStringF(IDS_IOS_VOICE_SEARCH_SETTINGS_DEFAULT_LOCALE,
                              localeConfig->GetDefaultLocale().display_name);
  defaultItem.accessoryType = selectedLocaleCode.empty()
                                  ? UITableViewCellAccessoryCheckmark
                                  : UITableViewCellAccessoryNone;
  [model addItem:defaultItem
      toSectionWithIdentifier:SectionIdentifierLanguages];
  // Add locale list.
  for (NSUInteger ii = 0; ii < locales.size(); ii++) {
    voice::SpeechInputLocale locale = locales[ii];
    NSString* languageName = base::SysUTF16ToNSString(locale.display_name);
    BOOL checked = (locale.code == selectedLocaleCode);

    TableViewDetailTextItem* languageItem = [[TableViewDetailTextItem alloc]
        initWithType:ItemTypeLanguagesLanguageOption];
    languageItem.text = languageName;
    languageItem.accessoryType = checked ? UITableViewCellAccessoryCheckmark
                                         : UITableViewCellAccessoryNone;
    [model addItem:languageItem
        toSectionWithIdentifier:SectionIdentifierLanguages];
  }
}

#pragma mark - UITableViewDelegate

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell =
      [super tableView:tableView cellForRowAtIndexPath:indexPath];
  NSInteger itemType = [self.tableViewModel itemTypeForIndexPath:indexPath];

  if (itemType == ItemTypeTTSEnabled) {
    // Have the switch send a message on UIControlEventValueChanged.
    SettingsSwitchCell* switchCell =
        base::mac::ObjCCastStrict<SettingsSwitchCell>(cell);
    switchCell.selectionStyle = UITableViewCellSelectionStyleNone;
    [switchCell.switchView addTarget:self
                              action:@selector(ttsToggled:)
                    forControlEvents:UIControlEventValueChanged];
  }

  return cell;
}

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  [super tableView:tableView didSelectRowAtIndexPath:indexPath];

  TableViewModel* model = self.tableViewModel;
  TableViewItem* item = [model itemAtIndexPath:indexPath];

  // Language options.
  if (item.type == ItemTypeLanguagesLanguageOption) {
    NSInteger index = [model indexInItemTypeForIndexPath:indexPath];
    std::string selectedLocaleCode;
    if (index > 0) {
      // Fetch selected locale code from locale list if non-default option was
      // selected.  Setting the preference to the empty string denotes using the
      // default locale.
      voice::SpeechInputLocaleConfig* localeConfig =
          voice::SpeechInputLocaleConfig::GetInstance();
      DCHECK_LT(static_cast<size_t>(index - 1),
                localeConfig->GetAvailableLocales().size());
      selectedLocaleCode = localeConfig->GetAvailableLocales()[index - 1].code;
    }
    _selectedLanguage.SetValue(selectedLocaleCode);

    // Update the UI.
    [self markAsCheckedLanguageAtIndex:index];
  }
  [tableView deselectRowAtIndexPath:indexPath animated:YES];
}

#pragma mark - Actions

- (void)ttsToggled:(id)sender {
  NSIndexPath* switchPath =
      [self.tableViewModel indexPathForItemType:ItemTypeTTSEnabled
                              sectionIdentifier:SectionIdentifierTTS];

  SettingsSwitchItem* switchItem =
      base::mac::ObjCCastStrict<SettingsSwitchItem>(
          [self.tableViewModel itemAtIndexPath:switchPath]);
  SettingsSwitchCell* switchCell =
      base::mac::ObjCCastStrict<SettingsSwitchCell>(
          [self.tableView cellForRowAtIndexPath:switchPath]);

  // Update the model and the preference with the current value of the switch.
  DCHECK_EQ(switchCell.switchView, sender);
  BOOL isOn = switchCell.switchView.isOn;
  switchItem.on = isOn;
  _ttsEnabled.SetValue(isOn);
}

#pragma mark - Private methods

- (void)markAsCheckedLanguageAtIndex:(NSUInteger)index {
  // Update the collection view model with the new selected language.
  NSArray* languageItems = [self.tableViewModel
      itemsInSectionWithIdentifier:SectionIdentifierLanguages];
  NSMutableArray* modifiedItems = [NSMutableArray array];
  for (NSUInteger ii = 0; ii < [languageItems count]; ++ii) {
    UITableViewCellAccessoryType type = (ii == index)
                                            ? UITableViewCellAccessoryCheckmark
                                            : UITableViewCellAccessoryNone;

    TableViewDetailTextItem* textItem =
        base::mac::ObjCCastStrict<TableViewDetailTextItem>(
            [languageItems objectAtIndex:ii]);
    if (textItem.accessoryType != type) {
      textItem.accessoryType = type;
      [modifiedItems addObject:textItem];
    }
  }

  // Some languages do not support TTS.  Disable the switch for those
  // languages.
  NSIndexPath* switchPath =
      [self.tableViewModel indexPathForItemType:ItemTypeTTSEnabled
                              sectionIdentifier:SectionIdentifierTTS];
  SettingsSwitchCell* switchCell =
      base::mac::ObjCCastStrict<SettingsSwitchCell>(
          [self.tableView cellForRowAtIndexPath:switchPath]);

  BOOL enabled = [self currentLanguageSupportsTTS];
  BOOL on = enabled && _ttsEnabled.GetValue();

  UISwitch* switchView = switchCell.switchView;
  switchView.enabled = enabled;
  switchCell.textLabel.textColor =
      [SettingsSwitchCell defaultTextColorForState:switchView.state];
  if (on != switchView.isOn) {
    [switchView setOn:on animated:YES];
  }
  // Also update the switch item.
  SettingsSwitchItem* switchItem =
      base::mac::ObjCCastStrict<SettingsSwitchItem>(
          [self.tableViewModel itemAtIndexPath:switchPath]);
  switchItem.enabled = enabled;
  switchItem.on = on;

  [self reconfigureCellsForItems:modifiedItems];
}

- (BOOL)currentLanguageSupportsTTS {
  voice::SpeechInputLocaleConfig* localeConfig =
      voice::SpeechInputLocaleConfig::GetInstance();
  std::string localeCode = _selectedLanguage.GetValue().empty()
                               ? localeConfig->GetDefaultLocale().code
                               : _selectedLanguage.GetValue();
  return localeConfig->IsTextToSpeechEnabledForCode(localeCode);
}

@end
