// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/autofill/autofill_credit_card_edit_table_view_controller.h"

#include "base/format_macros.h"
#import "base/ios/block_types.h"
#import "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/payments/payments_service_url.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#import "components/autofill/ios/browser/credit_card_util.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/application_context.h"
#import "ios/chrome/browser/ui/autofill/autofill_ui_type.h"
#import "ios/chrome/browser/ui/autofill/autofill_ui_type_util.h"
#import "ios/chrome/browser/ui/autofill/cells/autofill_edit_item.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/settings/autofill/autofill_edit_table_view_controller+protected.h"
#import "ios/chrome/browser/ui/settings/cells/copied_to_chrome_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_edit_item_delegate.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
using ::AutofillTypeFromAutofillUIType;

NSString* const kAutofillCreditCardEditTableViewId =
    @"kAutofillCreditCardEditTableViewId";

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierFields = kSectionIdentifierEnumZero,
  SectionIdentifierCopiedToChrome,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeCardholderName = kItemTypeEnumZero,
  ItemTypeCardNumber,
  ItemTypeExpirationMonth,
  ItemTypeExpirationYear,
  ItemTypeCopiedToChrome,
  ItemTypeNickname,
};

}  // namespace

@interface AutofillCreditCardEditTableViewController () <
    TableViewTextEditItemDelegate>
@end

@implementation AutofillCreditCardEditTableViewController {
  autofill::PersonalDataManager* _personalDataManager;  // weak
  autofill::CreditCard _creditCard;
}

#pragma mark - Initialization

- (instancetype)initWithCreditCard:(const autofill::CreditCard&)creditCard
               personalDataManager:(autofill::PersonalDataManager*)dataManager {
  UITableViewStyle style = base::FeatureList::IsEnabled(kSettingsRefresh)
                               ? UITableViewStylePlain
                               : UITableViewStyleGrouped;
  self = [super initWithStyle:style];
  if (self) {
    DCHECK(dataManager);

    _personalDataManager = dataManager;
    _creditCard = creditCard;

    [self setTitle:l10n_util::GetNSString(IDS_IOS_AUTOFILL_EDIT_CREDIT_CARD)];
  }

  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  self.tableView.allowsSelectionDuringEditing = YES;
  self.tableView.accessibilityIdentifier = kAutofillCreditCardEditTableViewId;
  [self loadModel];
}

#pragma mark - SettingsRootTableViewController

- (void)editButtonPressed {
  // In the case of server cards, open the Payments editing page instead.
  if (_creditCard.record_type() == autofill::CreditCard::FULL_SERVER_CARD ||
      _creditCard.record_type() == autofill::CreditCard::MASKED_SERVER_CARD) {
    GURL paymentsURL = autofill::payments::GetManageInstrumentsUrl();
    OpenNewTabCommand* command =
        [OpenNewTabCommand commandWithURLFromChrome:paymentsURL];
    [self.dispatcher closeSettingsUIAndOpenURL:command];

    // Don't call [super editButtonPressed] because edit mode is not actually
    // entered in this case.
    return;
  }

  [super editButtonPressed];

  if (!self.tableView.editing) {
    TableViewModel* model = self.tableViewModel;
    NSInteger itemCount =
        [model numberOfItemsInSection:
                   [model sectionForSectionIdentifier:SectionIdentifierFields]];

    // Reads the values from the fields and updates the local copy of the
    // card accordingly.
    NSInteger section =
        [model sectionForSectionIdentifier:SectionIdentifierFields];
    for (NSInteger itemIndex = 0; itemIndex < itemCount; ++itemIndex) {
      NSIndexPath* path = [NSIndexPath indexPathForItem:itemIndex
                                              inSection:section];
      AutofillEditItem* item = base::mac::ObjCCastStrict<AutofillEditItem>(
          [model itemAtIndexPath:path]);
      if ([self.tableViewModel itemTypeForIndexPath:path] == ItemTypeNickname) {
        NSString* trimmedNickname = [item.textFieldValue
            stringByTrimmingCharactersInSet:
                [NSCharacterSet whitespaceAndNewlineCharacterSet]];
        _creditCard.SetNickname(base::SysNSStringToUTF16(trimmedNickname));
      } else {
        _creditCard.SetInfo(
            autofill::AutofillType(
                AutofillTypeFromAutofillUIType(item.autofillUIType)),
            base::SysNSStringToUTF16(item.textFieldValue),
            GetApplicationContext()->GetApplicationLocale());
      }
    }

    _personalDataManager->UpdateCreditCard(_creditCard);
  }

  // Reload the model.
  [self loadModel];
  // Update the cells.
  [self reconfigureCellsForItems:
            [self.tableViewModel
                itemsInSectionWithIdentifier:SectionIdentifierFields]];
}

#pragma mark - ChromeTableViewController

- (void)loadModel {
  [super loadModel];
  TableViewModel<TableViewItem*>* model = self.tableViewModel;

  BOOL isEditing = self.tableView.editing;

  NSArray<AutofillEditItem*>* editItems;
  if ([self isCardNicknameManagementEnabled]) {
    editItems = @[
      [self cardNumberItem:isEditing],
      [self expirationMonthItem:isEditing],
      [self expirationYearItem:isEditing],
      [self cardholderNameItem:isEditing],
      [self nicknameItem:isEditing],
    ];
  } else {
    editItems = @[
      [self cardholderNameItem:isEditing],
      [self cardNumberItem:isEditing],
      [self expirationMonthItem:isEditing],
      [self expirationYearItem:isEditing],
    ];
  }

  [model addSectionWithIdentifier:SectionIdentifierFields];
  for (AutofillEditItem* item in editItems) {
    [model addItem:item toSectionWithIdentifier:SectionIdentifierFields];
  }

  if (_creditCard.record_type() == autofill::CreditCard::FULL_SERVER_CARD) {
    // Add CopiedToChrome cell in its own section.
    [model addSectionWithIdentifier:SectionIdentifierCopiedToChrome];
    CopiedToChromeItem* copiedToChromeItem =
        [[CopiedToChromeItem alloc] initWithType:ItemTypeCopiedToChrome];
    [model addItem:copiedToChromeItem
        toSectionWithIdentifier:SectionIdentifierCopiedToChrome];
  }
}

#pragma mark - TableViewTextEditItemDelegate

- (void)tableViewItemDidBeginEditing:
    (TableViewTextEditItem*)tableViewTextEditItem {
  // No op.
}

- (void)tableViewItemDidChange:(TableViewTextEditItem*)tableViewTextEditItem {
  if ([tableViewTextEditItem isKindOfClass:[AutofillEditItem class]]) {
    AutofillEditItem* item = (AutofillEditItem*)tableViewTextEditItem;

    // If the user is typing in the credit card number field, update the card
    // type icon (e.g. "Visa") to reflect the number being typed.
    if (item.autofillUIType == AutofillUITypeCreditCardNumber) {
      const char* network = autofill::CreditCard::GetCardNetwork(
          base::SysNSStringToUTF16(item.textFieldValue));
      item.identifyingIcon = [self cardTypeIconFromNetwork:network];
      [self reconfigureCellsForItems:@[ item ]];
    }

    if (item.type == ItemTypeNickname) {
      NSString* trimmedText = [item.textFieldValue
          stringByTrimmingCharactersInSet:
              [NSCharacterSet whitespaceAndNewlineCharacterSet]];
      BOOL newNicknameIsValid = autofill::CreditCard::IsNicknameValid(
          base::SysNSStringToUTF16(trimmedText));
      self.navigationItem.rightBarButtonItem.enabled = newNicknameIsValid;
      [item setHasValidText:newNicknameIsValid];
      [self reconfigureCellsForItems:@[ item ]];
    }
  }
}

- (void)tableViewItemDidEndEditing:
    (TableViewTextEditItem*)tableViewTextEditItem {
  // If the table view has already been dismissed or the editing stopped
  // ignore call as the item might no longer be in the cells (crbug/1125094).
  if (!self.tableView.isEditing) {
    return;
  }

  if ([tableViewTextEditItem isKindOfClass:[AutofillEditItem class]]) {
    AutofillEditItem* item = (AutofillEditItem*)tableViewTextEditItem;
    // Reconfigure to trigger appropiate icon change.
    [self reconfigureCellsForItems:@[ item ]];
  }
}

#pragma mark - UITableViewDataSource

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell = [super tableView:tableView
                     cellForRowAtIndexPath:indexPath];
  cell.selectionStyle = UITableViewCellSelectionStyleNone;

  NSInteger itemType = [self.tableViewModel itemTypeForIndexPath:indexPath];
  TableViewTextEditCell* editCell =
      base::mac::ObjCCast<TableViewTextEditCell>(cell);
  editCell.textField.delegate = self;
  switch (itemType) {
    case ItemTypeCardholderName:
    case ItemTypeCardNumber:
    case ItemTypeExpirationMonth:
    case ItemTypeExpirationYear:
    case ItemTypeNickname:
      break;
    case ItemTypeCopiedToChrome: {
      CopiedToChromeCell* copiedToChromeCell =
          base::mac::ObjCCastStrict<CopiedToChromeCell>(cell);
      [copiedToChromeCell.button addTarget:self
                                    action:@selector(buttonTapped:)
                          forControlEvents:UIControlEventTouchUpInside];
      break;
    }
    default:
      break;
  }

  return cell;
}

- (BOOL)tableView:(UITableView*)tableView
    canEditRowAtIndexPath:(NSIndexPath*)indexPath {
  // Items in this table view are not deletable, so should not be seen as
  // editable by the table view.
  return NO;
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  if (self.tableView.editing) {
    UITableViewCell* cell = [self.tableView cellForRowAtIndexPath:indexPath];
    TableViewTextEditCell* textFieldCell =
        base::mac::ObjCCastStrict<TableViewTextEditCell>(cell);
    [textFieldCell.textField becomeFirstResponder];
  }
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (BOOL)presentationControllerShouldDismiss:
    (UIPresentationController*)presentationController {
  return !self.tableView.editing;
}

#pragma mark - Actions

- (void)buttonTapped:(UIButton*)button {
  _personalDataManager->ResetFullServerCard(_creditCard.guid());

  // Reset the copy of the card data used for display immediately.
  _creditCard.set_record_type(autofill::CreditCard::MASKED_SERVER_CARD);
  _creditCard.SetNumber(_creditCard.LastFourDigits());
  [self reloadData];
}

#pragma mark - Helper Methods

- (UIImage*)cardTypeIconFromNetwork:(const char*)network {
  if (network != autofill::kGenericCard) {
    int resourceID =
        autofill::data_util::GetPaymentRequestData(network).icon_resource_id;
    // Return the card issuer network icon.
    return NativeImage(resourceID);
  } else {
    return nil;
  }
}

- (AutofillEditItem*)cardholderNameItem:(bool)isEditing {
  AutofillEditItem* cardholderNameItem =
      [[AutofillEditItem alloc] initWithType:ItemTypeCardholderName];
  cardholderNameItem.textFieldName =
      l10n_util::GetNSString(IDS_IOS_AUTOFILL_CARDHOLDER);
  cardholderNameItem.textFieldValue = autofill::GetCreditCardName(
      _creditCard, GetApplicationContext()->GetApplicationLocale());
  cardholderNameItem.textFieldEnabled = isEditing;
  cardholderNameItem.autofillUIType = AutofillUITypeCreditCardHolderFullName;
  cardholderNameItem.hideIcon = !isEditing;
  return cardholderNameItem;
}

- (AutofillEditItem*)cardNumberItem:(bool)isEditing {
  AutofillEditItem* cardNumberItem =
      [[AutofillEditItem alloc] initWithType:ItemTypeCardNumber];
  cardNumberItem.textFieldName =
      l10n_util::GetNSString(IDS_IOS_AUTOFILL_CARD_NUMBER);
  // Never show full card number for Wallet cards, even if copied locally.
  cardNumberItem.textFieldValue =
      autofill::IsCreditCardLocal(_creditCard)
          ? base::SysUTF16ToNSString(_creditCard.number())
          : base::SysUTF16ToNSString(_creditCard.NetworkAndLastFourDigits());
  cardNumberItem.textFieldEnabled = isEditing;
  cardNumberItem.autofillUIType = AutofillUITypeCreditCardNumber;
  cardNumberItem.keyboardType = UIKeyboardTypeNumberPad;
  cardNumberItem.hideIcon = !isEditing;
  cardNumberItem.delegate = self;
  // Hide credit card icon when editing.
  if (!isEditing) {
    cardNumberItem.identifyingIcon =
        [self cardTypeIconFromNetwork:_creditCard.network().c_str()];
  }
  return cardNumberItem;
}

- (AutofillEditItem*)expirationMonthItem:(bool)isEditing {
  AutofillEditItem* expirationMonthItem =
      [[AutofillEditItem alloc] initWithType:ItemTypeExpirationMonth];
  expirationMonthItem.textFieldName =
      l10n_util::GetNSString(IDS_IOS_AUTOFILL_EXP_MONTH);
  expirationMonthItem.textFieldValue =
      [NSString stringWithFormat:@"%02d", _creditCard.expiration_month()];
  expirationMonthItem.textFieldEnabled = isEditing;
  expirationMonthItem.autofillUIType = AutofillUITypeCreditCardExpMonth;
  expirationMonthItem.keyboardType = UIKeyboardTypeNumberPad;
  expirationMonthItem.hideIcon = !isEditing;
  return expirationMonthItem;
}

- (AutofillEditItem*)expirationYearItem:(bool)isEditing {
  // Expiration year.
  AutofillEditItem* expirationYearItem =
      [[AutofillEditItem alloc] initWithType:ItemTypeExpirationYear];
  expirationYearItem.textFieldName =
      l10n_util::GetNSString(IDS_IOS_AUTOFILL_EXP_YEAR);
  expirationYearItem.textFieldValue =
      [NSString stringWithFormat:@"%04d", _creditCard.expiration_year()];
  expirationYearItem.textFieldEnabled = isEditing;
  expirationYearItem.autofillUIType = AutofillUITypeCreditCardExpYear;
  expirationYearItem.keyboardType = UIKeyboardTypeNumberPad;
  expirationYearItem.returnKeyType = UIReturnKeyDone;
  expirationYearItem.hideIcon = !isEditing;
  return expirationYearItem;
}

- (AutofillEditItem*)nicknameItem:(bool)isEditing {
  AutofillEditItem* nicknameItem =
      [[AutofillEditItem alloc] initWithType:ItemTypeNickname];
  nicknameItem.textFieldName =
      l10n_util::GetNSString(IDS_IOS_AUTOFILL_NICKNAME);
  nicknameItem.textFieldValue =
      autofill::GetCreditCardNicknameString(_creditCard);
  nicknameItem.textFieldPlaceholder =
      l10n_util::GetNSString(IDS_IOS_AUTOFILL_DIALOG_PLACEHOLDER_NICKNAME);
  nicknameItem.textFieldEnabled = isEditing;
  nicknameItem.keyboardType = UIKeyboardTypeDefault;
  nicknameItem.hideIcon = !isEditing;
  nicknameItem.delegate = self;
  return nicknameItem;
}

// Returns whether card nickname managment feature is enabled.
- (BOOL)isCardNicknameManagementEnabled {
  return base::FeatureList::IsEnabled(
      autofill::features::kAutofillEnableCardNicknameManagement);
}

@end
