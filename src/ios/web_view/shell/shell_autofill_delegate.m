// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/shell/shell_autofill_delegate.h"

#import <UIKit/UIKit.h>

#import "ios/web_view/shell/shell_risk_data_loader.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ShellAutofillDelegate () <CWVCreditCardVerifierDelegate>

// Autofill controller.
@property(nonatomic, strong) CWVAutofillController* autofillController;

// Risk data loader.
@property(nonatomic, strong) ShellRiskDataLoader* riskDataLoader;

// Returns an action for a suggestion.
- (UIAlertAction*)actionForSuggestion:(CWVAutofillSuggestion*)suggestion;

@end

@implementation ShellAutofillDelegate

@synthesize autofillController = _autofillController;
@synthesize riskDataLoader = _riskDataLoader;

- (instancetype)init {
  self = [super init];
  if (self) {
    _riskDataLoader = [[ShellRiskDataLoader alloc] init];
  }
  return self;
}

#pragma mark - CWVAutofillControllerDelegate methods

- (void)autofillController:(CWVAutofillController*)autofillController
    didFocusOnFieldWithIdentifier:(NSString*)fieldIdentifier
                        fieldType:(NSString*)fieldType
                         formName:(NSString*)formName
                          frameID:(NSString*)frameID
                            value:(NSString*)value {
  _autofillController = autofillController;

  __weak ShellAutofillDelegate* weakSelf = self;
  id completionHandler = ^(NSArray<CWVAutofillSuggestion*>* suggestions) {
    ShellAutofillDelegate* strongSelf = weakSelf;
    if (!suggestions.count || !strongSelf) {
      return;
    }

    UIAlertController* alertController = [UIAlertController
        alertControllerWithTitle:@"Pick a suggestion"
                         message:nil
                  preferredStyle:UIAlertControllerStyleActionSheet];
    UIAlertAction* cancelAction =
        [UIAlertAction actionWithTitle:@"Cancel"
                                 style:UIAlertActionStyleCancel
                               handler:nil];
    [alertController addAction:cancelAction];
    for (CWVAutofillSuggestion* suggestion in suggestions) {
      [alertController addAction:[self actionForSuggestion:suggestion]];
    }
    UIAlertAction* clearAction = [UIAlertAction
        actionWithTitle:@"Clear"
                  style:UIAlertActionStyleDefault
                handler:^(UIAlertAction* _Nonnull action) {
                  [autofillController clearFormWithName:formName
                                        fieldIdentifier:fieldIdentifier
                                                frameID:frameID
                                      completionHandler:nil];
                }];
    [alertController addAction:clearAction];

    [UIApplication.sharedApplication.keyWindow.rootViewController
        presentViewController:alertController
                     animated:YES
                   completion:nil];
  };
  [autofillController fetchSuggestionsForFormWithName:formName
                                      fieldIdentifier:fieldIdentifier
                                            fieldType:fieldType
                                              frameID:frameID
                                    completionHandler:completionHandler];
}

- (void)autofillController:(CWVAutofillController*)autofillController
    didInputInFieldWithIdentifier:(NSString*)fieldIdentifier
                        fieldType:(NSString*)fieldType
                         formName:(NSString*)formName
                            value:(NSString*)value {
  // Not implemented.
}

- (void)autofillController:(CWVAutofillController*)autofillController
    didBlurOnFieldWithIdentifier:(NSString*)fieldIdentifier
                       fieldType:(NSString*)fieldType
                        formName:(NSString*)formName
                           value:(NSString*)value {
  // Not implemented.
}

- (void)autofillController:(CWVAutofillController*)autofillController
     didSubmitFormWithName:(NSString*)formName
             userInitiated:(BOOL)userInitiated
               isMainFrame:(BOOL)isMainFrame {
  // Not implemented.
}

- (void)autofillControllerDidInsertFormElements:
    (CWVAutofillController*)autofillController {
  // Not implemented.
}

- (void)autofillController:(CWVAutofillController*)autofillController
    decidePolicyForLocalStorageOfCreditCard:(CWVCreditCard*)creditCard
                            decisionHandler:
                                (void (^)(CWVStoragePolicy))decisionHandler {
  NSString* cardSummary = [NSString
      stringWithFormat:@"%@ %@ %@/%@", creditCard.cardHolderFullName,
                       creditCard.cardNumber, creditCard.expirationMonth,
                       creditCard.expirationYear];
  UIAlertController* alertController = [UIAlertController
      alertControllerWithTitle:@"Update Password"
                       message:cardSummary
                preferredStyle:UIAlertControllerStyleActionSheet];
  UIAlertAction* allowAction =
      [UIAlertAction actionWithTitle:@"Allow"
                               style:UIAlertActionStyleDefault
                             handler:^(UIAlertAction* _Nonnull action) {
                               decisionHandler(CWVStoragePolicyAllow);
                             }];
  UIAlertAction* cancelAction =
      [UIAlertAction actionWithTitle:@"Cancel"
                               style:UIAlertActionStyleCancel
                             handler:^(UIAlertAction* _Nonnull action) {
                               decisionHandler(CWVStoragePolicyReject);
                             }];
  [alertController addAction:allowAction];
  [alertController addAction:cancelAction];

  [UIApplication.sharedApplication.keyWindow.rootViewController
      presentViewController:alertController
                   animated:YES
                 completion:nil];
}

- (void)autofillController:(CWVAutofillController*)autofillController
    decidePasswordSavingPolicyForUsername:(NSString*)userName
                          decisionHandler:(void (^)(CWVPasswordUserDecision))
                                              decisionHandler {
  UIAlertController* alertController = [UIAlertController
      alertControllerWithTitle:@"Save Password"
                       message:@"Do you want to save your password on "
                               @"this site?"
                preferredStyle:UIAlertControllerStyleActionSheet];

  UIAlertAction* noAction = [UIAlertAction
      actionWithTitle:@"Not this time"
                style:UIAlertActionStyleCancel
              handler:^(UIAlertAction* _Nonnull action) {
                decisionHandler(CWVPasswordUserDecisionNotThisTime);
              }];
  [alertController addAction:noAction];

  UIAlertAction* neverAction =
      [UIAlertAction actionWithTitle:@"Never"
                               style:UIAlertActionStyleDefault
                             handler:^(UIAlertAction* _Nonnull action) {
                               decisionHandler(CWVPasswordUserDecisionNever);
                             }];
  [alertController addAction:neverAction];

  UIAlertAction* yesAction =
      [UIAlertAction actionWithTitle:@"Save"
                               style:UIAlertActionStyleDefault
                             handler:^(UIAlertAction* _Nonnull action) {
                               decisionHandler(CWVPasswordUserDecisionYes);
                             }];
  [alertController addAction:yesAction];

  [UIApplication.sharedApplication.keyWindow.rootViewController
      presentViewController:alertController
                   animated:YES
                 completion:nil];
}

- (void)autofillController:(CWVAutofillController*)autofillController
    decidePasswordUpdatingPolicyForUsername:(NSString*)userName
                            decisionHandler:(void (^)(CWVPasswordUserDecision))
                                                decisionHandler {
  NSString* message =
      [NSString stringWithFormat:@"Do you want to update your password "
                                 @"for %@ on this site?",
                                 userName];
  UIAlertController* alertController = [UIAlertController
      alertControllerWithTitle:@"Update Password"
                       message:message
                preferredStyle:UIAlertControllerStyleActionSheet];

  UIAlertAction* noAction = [UIAlertAction
      actionWithTitle:@"Not this time"
                style:UIAlertActionStyleCancel
              handler:^(UIAlertAction* _Nonnull action) {
                decisionHandler(CWVPasswordUserDecisionNotThisTime);
              }];
  [alertController addAction:noAction];

  UIAlertAction* yesAction =
      [UIAlertAction actionWithTitle:@"Update"
                               style:UIAlertActionStyleDefault
                             handler:^(UIAlertAction* _Nonnull action) {
                               decisionHandler(CWVPasswordUserDecisionYes);
                             }];
  [alertController addAction:yesAction];

  [UIApplication.sharedApplication.keyWindow.rootViewController
      presentViewController:alertController
                   animated:YES
                 completion:nil];
}

- (void)autofillController:(CWVAutofillController*)autofillController
    verifyCreditCardWithVerifier:(CWVCreditCardVerifier*)verifier {
  [UIApplication.sharedApplication.keyWindow endEditing:YES];

  UIAlertController* alertController =
      [UIAlertController alertControllerWithTitle:@"Verify Card"
                                          message:@"Enter CVC"
                                   preferredStyle:UIAlertControllerStyleAlert];

  UIAlertAction* submit = [UIAlertAction
      actionWithTitle:@"Confirm"
                style:UIAlertActionStyleDefault
              handler:^(UIAlertAction* action) {
                UITextField* textField = alertController.textFields.firstObject;
                NSString* CVC = textField.text;
                [verifier verifyWithCVC:CVC
                        expirationMonth:nil
                         expirationYear:nil
                           storeLocally:NO
                             dataSource:self.riskDataLoader
                               delegate:self];
              }];

  [alertController addAction:submit];

  [alertController
      addTextFieldWithConfigurationHandler:^(UITextField* textField) {
        textField.placeholder = @"CVC";
        textField.keyboardType = UIKeyboardTypeNumberPad;
      }];

  [UIApplication.sharedApplication.keyWindow.rootViewController
      presentViewController:alertController
                   animated:YES
                 completion:nil];
}

#pragma mark - CWVCreditCardVerifierDelegate

- (void)creditCardVerifier:(CWVCreditCardVerifier*)creditCardVerifier
    didFinishVerificationWithError:(nullable NSError*)error {
  if (error) {
    UIAlertController* alertController = [UIAlertController
        alertControllerWithTitle:@"Verification Error"
                         message:error.localizedDescription
                  preferredStyle:UIAlertControllerStyleAlert];
    UIAlertAction* action =
        [UIAlertAction actionWithTitle:@"OK"
                                 style:UIAlertActionStyleDefault
                               handler:nil];
    [alertController addAction:action];
    [UIApplication.sharedApplication.keyWindow.rootViewController
        presentViewController:alertController
                     animated:YES
                   completion:nil];
  }
}

#pragma mark - Private Methods

- (UIAlertAction*)actionForSuggestion:(CWVAutofillSuggestion*)suggestion {
  NSString* title =
      [NSString stringWithFormat:@"%@ %@", suggestion.value,
                                 suggestion.displayDescription ?: @""];
  return [UIAlertAction actionWithTitle:title
                                  style:UIAlertActionStyleDefault
                                handler:^(UIAlertAction* _Nonnull action) {
                                  [_autofillController fillSuggestion:suggestion
                                                    completionHandler:nil];
                                  [UIApplication.sharedApplication.keyWindow
                                      endEditing:YES];
                                }];
}

@end
