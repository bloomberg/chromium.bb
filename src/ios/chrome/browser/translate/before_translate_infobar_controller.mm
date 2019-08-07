// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/translate/before_translate_infobar_controller.h"

#include <stddef.h>
#import <UIKit/UIKit.h>

#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/strings/grit/components_strings.h"
#include "components/translate/core/browser/translate_infobar_delegate.h"
#import "ios/chrome/browser/infobars/infobar_controller+protected.h"
#include "ios/chrome/browser/infobars/infobar_controller_delegate.h"
#include "ios/chrome/browser/translate/language_selection_context.h"
#include "ios/chrome/browser/translate/language_selection_delegate.h"
#include "ios/chrome/browser/translate/language_selection_handler.h"
#include "ios/chrome/browser/translate/translate_infobar_tags.h"
#import "ios/chrome/browser/ui/infobars/confirm_infobar_view.h"
#import "ios/chrome/browser/ui/util/top_view_controller.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface BeforeTranslateInfoBarController ()<LanguageSelectionDelegate>

// Overrides superclass property.
@property(nonatomic, readonly)
    translate::TranslateInfoBarDelegate* infoBarDelegate;

@end

@implementation BeforeTranslateInfoBarController {
  // Stores whether the user is currently choosing in the UIPickerView the
  // original language, or the target language.
  TranslateInfoBarIOSTag::Tag _languageSelectionType;
  __weak ConfirmInfoBarView* _infoBarView;
}

@synthesize languageSelectionHandler = _languageSelectionHandler;
@dynamic infoBarDelegate;

#pragma mark -
#pragma mark InfoBarControllerProtocol

- (instancetype)initWithInfoBarDelegate:
    (translate::TranslateInfoBarDelegate*)infoBarDelegate {
  return [super initWithInfoBarDelegate:infoBarDelegate];
}

- (UIView*)infobarView {
  ConfirmInfoBarView* infoBarView =
      [[ConfirmInfoBarView alloc] initWithFrame:CGRectZero];
  _infoBarView = infoBarView;
  // Icon
  gfx::Image icon = self.infoBarDelegate->GetIcon();
  if (!icon.IsEmpty())
    [infoBarView addLeftIcon:icon.ToUIImage()];

  // Main text.
  [self updateInfobarLabelOnView:infoBarView];

  // Close button.
  [infoBarView addCloseButtonWithTag:TranslateInfoBarIOSTag::BEFORE_DENY
                              target:self
                              action:@selector(infoBarButtonDidPress:)];
  // Other buttons.
  NSString* buttonAccept = l10n_util::GetNSString(IDS_TRANSLATE_INFOBAR_ACCEPT);
  NSString* buttonDeny = l10n_util::GetNSString(IDS_TRANSLATE_INFOBAR_DENY);
  [infoBarView addButton1:buttonAccept
                     tag1:TranslateInfoBarIOSTag::BEFORE_ACCEPT
                  button2:buttonDeny
                     tag2:TranslateInfoBarIOSTag::BEFORE_DENY
                   target:self
                   action:@selector(infoBarButtonDidPress:)];
  return infoBarView;
}

- (void)updateInfobarLabelOnView:(ConfirmInfoBarView*)view {
  NSString* originalLanguage =
      base::SysUTF16ToNSString(self.infoBarDelegate->original_language_name());
  NSString* targetLanguage =
      base::SysUTF16ToNSString(self.infoBarDelegate->target_language_name());
  base::string16 originalLanguageWithLink =
      base::SysNSStringToUTF16([[view class]
          stringAsLink:originalLanguage
                   tag:TranslateInfoBarIOSTag::BEFORE_SOURCE_LANGUAGE]);
  base::string16 targetLanguageWithLink = base::SysNSStringToUTF16([[view class]
      stringAsLink:targetLanguage
               tag:TranslateInfoBarIOSTag::BEFORE_TARGET_LANGUAGE]);
  NSString* label =
      l10n_util::GetNSStringF(IDS_TRANSLATE_INFOBAR_BEFORE_MESSAGE_IOS,
                              originalLanguageWithLink, targetLanguageWithLink);

  __weak BeforeTranslateInfoBarController* weakSelf = self;
  [view addLabel:label
          action:^(NSUInteger tag) {
            [weakSelf infobarLinkDidPress:tag];
          }];
}

#pragma mark - Handling of User Events

- (void)infoBarButtonDidPress:(id)sender {
  if ([self shouldIgnoreUserInteraction])
    return;

  NSUInteger buttonId = base::mac::ObjCCastStrict<UIButton>(sender).tag;
  switch (buttonId) {
    case TranslateInfoBarIOSTag::BEFORE_ACCEPT:
      self.infoBarDelegate->Translate();
      break;
    case TranslateInfoBarIOSTag::BEFORE_DENY:
      self.infoBarDelegate->TranslationDeclined();
      if (self.infoBarDelegate->ShouldShowNeverTranslateShortcut())
        self.infoBarDelegate->ShowNeverTranslateInfobar();
      else
        self.delegate->RemoveInfoBar();
      break;
    default:
      NOTREACHED() << "Unexpected Translate button label";
      break;
  }
}

- (void)infobarLinkDidPress:(NSUInteger)tag {
  if ([self shouldIgnoreUserInteraction])
    return;

  _languageSelectionType = static_cast<TranslateInfoBarIOSTag::Tag>(tag);
  DCHECK(_languageSelectionType ==
             TranslateInfoBarIOSTag::BEFORE_SOURCE_LANGUAGE ||
         _languageSelectionType ==
             TranslateInfoBarIOSTag::BEFORE_TARGET_LANGUAGE);

  size_t selectedRow;
  size_t disabledRow;
  int originalLanguageIndex = -1;
  int targetLanguageIndex = -1;

  for (size_t i = 0; i < self.infoBarDelegate->num_languages(); ++i) {
    if (self.infoBarDelegate->language_code_at(i) ==
        self.infoBarDelegate->original_language_code()) {
      originalLanguageIndex = i;
    }
    if (self.infoBarDelegate->language_code_at(i) ==
        self.infoBarDelegate->target_language_code()) {
      targetLanguageIndex = i;
    }
  }
  DCHECK_GT(originalLanguageIndex, -1);
  DCHECK_GT(targetLanguageIndex, -1);

  if (_languageSelectionType ==
      TranslateInfoBarIOSTag::BEFORE_SOURCE_LANGUAGE) {
    selectedRow = originalLanguageIndex;
    disabledRow = targetLanguageIndex;
  } else {
    selectedRow = targetLanguageIndex;
    disabledRow = originalLanguageIndex;
  }
  LanguageSelectionContext* context =
      [LanguageSelectionContext contextWithLanguageData:self.infoBarDelegate
                                           initialIndex:selectedRow
                                       unavailableIndex:disabledRow];
  DCHECK(self.languageSelectionHandler);
  [self.languageSelectionHandler showLanguageSelectorWithContext:context
                                                        delegate:self];
}

#pragma mark - LanguageSelectionDelegate

- (void)languageSelectorSelectedLanguage:(std::string)languageCode {
  if ([self shouldIgnoreUserInteraction])
    return;

  if (_languageSelectionType ==
          TranslateInfoBarIOSTag::BEFORE_SOURCE_LANGUAGE &&
      languageCode != self.infoBarDelegate->target_language_code()) {
    self.infoBarDelegate->UpdateOriginalLanguage(languageCode);
  }
  if (_languageSelectionType ==
          TranslateInfoBarIOSTag::BEFORE_TARGET_LANGUAGE &&
      languageCode != self.infoBarDelegate->original_language_code()) {
    self.infoBarDelegate->UpdateTargetLanguage(languageCode);
  }
  [self updateInfobarLabelOnView:_infoBarView];
}

- (void)languageSelectorClosedWithoutSelection {
  // No-op in this implementation, but (for example) metrics for this state
  // might be added.
}

@end
