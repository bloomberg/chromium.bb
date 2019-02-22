// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/translate/after_translate_infobar_controller.h"

#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/strings/grit/components_strings.h"
#include "components/translate/core/browser/translate_infobar_delegate.h"
#import "ios/chrome/browser/infobars/infobar_controller+protected.h"
#include "ios/chrome/browser/infobars/infobar_controller_delegate.h"
#include "ios/chrome/browser/translate/translate_infobar_tags.h"
#import "ios/chrome/browser/ui/infobars/confirm_infobar_view.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
enum AlwaysTranslateSwitchState {
  ALWAYS_TRANSLATE_SWITCH_NOT_CHANGED,
  ALWAYS_TRANSLATE_SWITCH_SET_TO_ENABLED,
  ALWAYS_TRANSLATE_SWITCH_SET_TO_DISABLED,
};
}  // namespace

@interface AfterTranslateInfoBarController () {
  AlwaysTranslateSwitchState _alwaysTranslateSwitchState;
}

// Overrides superclass property.
@property(nonatomic, readonly)
    translate::TranslateInfoBarDelegate* infoBarDelegate;

@end

@implementation AfterTranslateInfoBarController

@dynamic infoBarDelegate;

#pragma mark -
#pragma mark InfoBarControllerProtocol

- (instancetype)initWithInfoBarDelegate:
    (translate::TranslateInfoBarDelegate*)infoBarDelegate {
  return [super initWithInfoBarDelegate:infoBarDelegate];
}

- (UIView<InfoBarViewSizing>*)viewForFrame:(CGRect)frame {
  ConfirmInfoBarView* infoBarView =
      [[ConfirmInfoBarView alloc] initWithFrame:frame];
  // Icon
  gfx::Image icon = self.infoBarDelegate->GetIcon();
  if (!icon.IsEmpty())
    [infoBarView addLeftIcon:icon.ToUIImage()];
  // Main text.
  const bool autodeterminedSourceLanguage =
      self.infoBarDelegate->original_language_code() ==
      translate::kUnknownLanguageCode;
  bool swappedLanguageButtons;
  std::vector<base::string16> strings;
  translate::TranslateInfoBarDelegate::GetAfterTranslateStrings(
      &strings, &swappedLanguageButtons, autodeterminedSourceLanguage);
  DCHECK_EQ(autodeterminedSourceLanguage ? 2U : 3U, strings.size());
  NSString* label1 = base::SysUTF16ToNSString(strings[0]);
  NSString* label2 = base::SysUTF16ToNSString(strings[1]);
  NSString* label3 = autodeterminedSourceLanguage
                         ? @""
                         : base::SysUTF16ToNSString(strings[2]);
  base::string16 stdOriginal = self.infoBarDelegate->original_language_name();
  NSString* original = base::SysUTF16ToNSString(stdOriginal);
  NSString* target =
      base::SysUTF16ToNSString(self.infoBarDelegate->target_language_name());
  NSString* label =
      [[NSString alloc] initWithFormat:@"%@ %@ %@%@ %@.", label1, original,
                                       label2, label3, target];
  [infoBarView addLabel:label];
  // Close button.
  [infoBarView addCloseButtonWithTag:TranslateInfoBarIOSTag::CLOSE
                              target:self
                              action:@selector(infoBarButtonDidPress:)];
  // Other buttons.
  NSString* buttonRevert = l10n_util::GetNSString(IDS_TRANSLATE_INFOBAR_REVERT);
  NSString* buttonOptions = l10n_util::GetNSString(IDS_DONE);
  [infoBarView addButton1:buttonOptions
                     tag1:TranslateInfoBarIOSTag::AFTER_DONE
                  button2:buttonRevert
                     tag2:TranslateInfoBarIOSTag::AFTER_REVERT
                   target:self
                   action:@selector(infoBarButtonDidPress:)];
  // Always translate switch.
  _alwaysTranslateSwitchState = ALWAYS_TRANSLATE_SWITCH_NOT_CHANGED;
  if (self.infoBarDelegate->ShouldShowAlwaysTranslateShortcut()) {
    base::string16 alwaysTranslate = l10n_util::GetStringFUTF16(
        IDS_TRANSLATE_INFOBAR_ALWAYS_TRANSLATE, stdOriginal);
    const BOOL switchValue = self.infoBarDelegate->ShouldAlwaysTranslate();
    [infoBarView
        addSwitchWithLabel:base::SysUTF16ToNSString(alwaysTranslate)
                      isOn:switchValue
                       tag:TranslateInfoBarIOSTag::ALWAYS_TRANSLATE_SWITCH
                    target:self
                    action:@selector(infoBarSwitchDidPress:)];
  }
  return infoBarView;
}

#pragma mark - Handling of User Events

- (void)infoBarButtonDidPress:(id)sender {
  if ([self shouldIgnoreUserInteraction])
    return;

  NSUInteger buttonId = base::mac::ObjCCastStrict<UIButton>(sender).tag;
  switch (buttonId) {
    case TranslateInfoBarIOSTag::CLOSE:
      self.infoBarDelegate->InfoBarDismissed();
      self.delegate->RemoveInfoBar();
      break;
    case TranslateInfoBarIOSTag::AFTER_DONE:
      [self saveAlwaysTranslateState];
      self.infoBarDelegate->InfoBarDismissed();
      self.delegate->RemoveInfoBar();
      break;
    case TranslateInfoBarIOSTag::AFTER_REVERT:
      self.infoBarDelegate->RevertTranslation();
      break;
    default:
      NOTREACHED() << "Unexpected Translate button label";
      break;
  }
}

- (void)infoBarSwitchDidPress:(id)sender {
  DCHECK_EQ(TranslateInfoBarIOSTag::ALWAYS_TRANSLATE_SWITCH, [sender tag]);
  DCHECK([sender respondsToSelector:@selector(isOn)]);
  _alwaysTranslateSwitchState = [sender isOn]
                                    ? ALWAYS_TRANSLATE_SWITCH_SET_TO_ENABLED
                                    : ALWAYS_TRANSLATE_SWITCH_SET_TO_DISABLED;
}

#pragma mark - Private methods

- (void)saveAlwaysTranslateState {
  if (_alwaysTranslateSwitchState == ALWAYS_TRANSLATE_SWITCH_NOT_CHANGED)
    return;

  const bool alwaysTranslate =
      _alwaysTranslateSwitchState == ALWAYS_TRANSLATE_SWITCH_SET_TO_ENABLED;
  if (alwaysTranslate == self.infoBarDelegate->ShouldAlwaysTranslate())
    return;

  self.infoBarDelegate->ToggleAlwaysTranslate();
}

@end
