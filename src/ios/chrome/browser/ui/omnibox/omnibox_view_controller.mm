// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/omnibox_view_controller.h"

#include "base/bind.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/sys_string_conversions.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/open_from_clipboard/clipboard_recent_content.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/commands/load_query_commands.h"
#import "ios/chrome/browser/ui/commands/omnibox_commands.h"
#import "ios/chrome/browser/ui/default_promo/default_browser_utils.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_constants.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_container_view.h"
#include "ios/chrome/browser/ui/omnibox/omnibox_text_change_delegate.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_text_field_delegate.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_constants.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#include "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/pointer_interaction_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::UserMetricsAction;

namespace {

const CGFloat kClearButtonSize = 28.0f;

}  // namespace

@interface OmniboxViewController () <OmniboxTextFieldDelegate,
                                     UIScribbleInteractionDelegate> {
  // Weak, acts as a delegate
  OmniboxTextChangeDelegate* _textChangeDelegate;
}

// Override of UIViewController's view with a different type.
@property(nonatomic, strong) OmniboxContainerView* view;

// Whether the default search engine supports search-by-image. This controls the
// edit menu option to do an image search.
@property(nonatomic, assign) BOOL searchByImageEnabled;

@property(nonatomic, assign) BOOL incognito;

// YES if we are already forwarding an OnDidChange() message to the edit view.
// Needed to prevent infinite recursion.
// TODO(crbug.com/1015413): There must be a better way.
@property(nonatomic, assign) BOOL forwardingOnDidChange;

// YES if this text field is currently processing a user-initiated event,
// such as typing in the omnibox or pressing the clear button.  Used to
// distinguish between calls to textDidChange that are triggered by the user
// typing vs by calls to setText.
@property(nonatomic, assign) BOOL processingUserEvent;

// A flag that is set whenever any input or copy/paste event happened in the
// omnibox while it was focused. Used to count event "user focuses the omnibox
// to view the complete URL and immediately defocuses it".
@property(nonatomic, assign) BOOL omniboxInteractedWhileFocused;

// Tracks editing status, because only the omnibox that is in edit mode can
// get an edit menu.
@property(nonatomic, assign) BOOL isTextfieldEditing;

// Is YES while fixing display of edit menu (below omnibox).
@property(nonatomic, assign) BOOL showingEditMenu;

// Stores whether the clipboard currently stores copied content.
@property(nonatomic, assign) BOOL hasCopiedContent;
// Stores the current content type in the clipboard. This is only valid if
// |hasCopiedContent| is YES.
@property(nonatomic, assign) ClipboardContentType copiedContentType;
// Stores whether the cached clipboard state is currently being updated. See
// |-updateCachedClipboardState| for more information.
@property(nonatomic, assign) BOOL isUpdatingCachedClipboardState;

@end

@implementation OmniboxViewController
@dynamic view;

- (instancetype)initWithIncognito:(BOOL)isIncognito {
  self = [super init];
  if (self) {
    _incognito = isIncognito;
  }
  return self;
}

#pragma mark - UIViewController

- (void)loadView {
  UIColor* textColor = [UIColor colorNamed:kTextPrimaryColor];
  UIColor* textFieldTintColor = [UIColor colorNamed:kBlueColor];
  UIColor* iconTintColor;
  iconTintColor = [UIColor colorNamed:kToolbarButtonColor];

  self.view = [[OmniboxContainerView alloc] initWithFrame:CGRectZero
                                                textColor:textColor
                                            textFieldTint:textFieldTintColor
                                                 iconTint:iconTintColor];
  self.view.incognito = self.incognito;

  self.textField.delegate = self;

  SetA11yLabelAndUiAutomationName(self.textField, IDS_ACCNAME_LOCATION,
                                  @"Address");

  [self.textField
      addInteraction:[[UIScribbleInteraction alloc] initWithDelegate:self]];
}

- (void)viewDidLoad {
  [super viewDidLoad];

  // Add Paste and Go option to the editing menu
  RegisterEditMenuItem([[UIMenuItem alloc]
      initWithTitle:l10n_util::GetNSString(IDS_IOS_SEARCH_COPIED_IMAGE)
             action:@selector(searchCopiedImage:)]);
  RegisterEditMenuItem([[UIMenuItem alloc]
      initWithTitle:l10n_util::GetNSString(IDS_IOS_VISIT_COPIED_LINK)
             action:@selector(visitCopiedLink:)]);
  RegisterEditMenuItem([[UIMenuItem alloc]
      initWithTitle:l10n_util::GetNSString(IDS_IOS_SEARCH_COPIED_TEXT)
             action:@selector(searchCopiedText:)]);

  self.textField.placeholderTextColor = [self placeholderAndClearButtonColor];
  self.textField.placeholder = l10n_util::GetNSString(IDS_OMNIBOX_EMPTY_HINT);
  [self setupClearButton];

  [NSNotificationCenter.defaultCenter
      addObserver:self
         selector:@selector(textInputModeDidChange)
             name:UITextInputCurrentInputModeDidChangeNotification
           object:nil];
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];

  [NSNotificationCenter.defaultCenter
      addObserver:self
         selector:@selector(pasteboardDidChange:)
             name:UIPasteboardChangedNotification
           object:nil];

  // The pasteboard changed notification doesn't fire if the clipboard changes
  // while the app is in the background, so update the state whenever the app
  // becomes active.
  [NSNotificationCenter.defaultCenter
      addObserver:self
         selector:@selector(applicationDidBecomeActive:)
             name:UIApplicationDidBecomeActiveNotification
           object:nil];
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];

  [self.view attachLayoutGuides];
}

- (void)viewWillDisappear:(BOOL)animated {
  [super viewWillDisappear:animated];
  self.textField.selectedTextRange =
      [self.textField textRangeFromPosition:self.textField.beginningOfDocument
                                 toPosition:self.textField.beginningOfDocument];

  [NSNotificationCenter.defaultCenter
      removeObserver:self
                name:UIPasteboardChangedNotification
              object:nil];

  // The pasteboard changed notification doesn't fire if the clipboard changes
  // while the app is in the background, so update the state whenever the app
  // becomes active.
  [NSNotificationCenter.defaultCenter
      removeObserver:self
                name:UIApplicationDidBecomeActiveNotification
              object:nil];
}

#pragma mark - properties

- (void)setTextChangeDelegate:(OmniboxTextChangeDelegate*)textChangeDelegate {
  _textChangeDelegate = textChangeDelegate;
}

- (void)setIsTextfieldEditing:(BOOL)owns {
  if (_isTextfieldEditing == owns) {
    return;
  }
  if (owns) {
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(menuControllerWillShow:)
               name:UIMenuControllerWillShowMenuNotification
             object:nil];
  } else {
    [[NSNotificationCenter defaultCenter]
        removeObserver:self
                  name:UIMenuControllerWillShowMenuNotification
                object:nil];
  }
  _isTextfieldEditing = owns;
}

#pragma mark - public methods

- (OmniboxTextFieldIOS*)textField {
  return self.view.textField;
}

- (void)prepareOmniboxForScribble {
  [self.textField exitPreEditState];
  [self.textField setText:[[NSAttributedString alloc] initWithString:@""]
           userTextLength:0];
  self.textField.placeholder = nil;
}

- (void)cleanupOmniboxAfterScribble {
  self.textField.placeholder = l10n_util::GetNSString(IDS_OMNIBOX_EMPTY_HINT);
}

#pragma mark - OmniboxTextFieldDelegate

- (BOOL)textField:(UITextField*)textField
    shouldChangeCharactersInRange:(NSRange)range
                replacementString:(NSString*)newText {
  if (!_textChangeDelegate) {
    // This can happen when the view controller is still alive but the model is
    // already deconstructed on shutdown.
    return YES;
  }
  self.processingUserEvent = _textChangeDelegate->OnWillChange(range, newText);
  return self.processingUserEvent;
}

- (void)textFieldDidChange:(id)sender {
  // If the text is empty, update the leading image.
  if (self.textField.text.length == 0) {
    [self.view setLeadingImage:self.emptyTextLeadingImage];
  }

  [self updateClearButtonVisibility];
  self.semanticContentAttribute = [self.textField bestSemanticContentAttribute];

  if (self.forwardingOnDidChange) {
    return;
  }

  // Reset the changed flag.
  self.omniboxInteractedWhileFocused = YES;

  BOOL savedProcessingUserEvent = self.processingUserEvent;
  self.processingUserEvent = NO;
  self.forwardingOnDidChange = YES;
  if (!_textChangeDelegate) {
    // This can happen when the view controller is still alive but the model is
    // already deconstructed on shutdown.
    return;
  }
  _textChangeDelegate->OnDidChange(savedProcessingUserEvent);
  self.forwardingOnDidChange = NO;
}

// Delegate method for UITextField, called when user presses the "go" button.
- (BOOL)textFieldShouldReturn:(UITextField*)textField {
  if (!_textChangeDelegate) {
    // This can happen when the view controller is still alive but the model is
    // already deconstructed on shutdown.
    return YES;
  }
  _textChangeDelegate->OnAccept();
  return NO;
}

// Always update the text field colors when we start editing.  It's possible
// for this method to be called when we are already editing (popup focus
// change).  In this case, OnDidBeginEditing will be called multiple times.
// If that becomes an issue a boolean should be added to track editing state.
- (void)textFieldDidBeginEditing:(UITextField*)textField {
  [self updateCachedClipboardState];

  // Update the clear button state.
  [self updateClearButtonVisibility];
  [self.view setLeadingImage:self.textField.text.length
                                 ? self.defaultLeadingImage
                                 : self.emptyTextLeadingImage];

  self.semanticContentAttribute = [self.textField bestSemanticContentAttribute];
  self.isTextfieldEditing = YES;

  self.omniboxInteractedWhileFocused = NO;
  if (!_textChangeDelegate) {
    // This can happen when the view controller is still alive but the model is
    // already deconstructed on shutdown.
    return;
  }
  _textChangeDelegate->OnDidBeginEditing();
}

- (BOOL)textFieldShouldEndEditing:(UITextField*)textField {
  if (!_textChangeDelegate) {
    // This can happen when the view controller is still alive but the model is
    // already deconstructed on shutdown.
    return YES;
  }
  _textChangeDelegate->OnWillEndEditing();

  return YES;
}

// Record the metrics as needed.
- (void)textFieldDidEndEditing:(UITextField*)textField
                        reason:(UITextFieldDidEndEditingReason)reason {
  self.isTextfieldEditing = NO;

  if (!self.omniboxInteractedWhileFocused) {
    RecordAction(
        UserMetricsAction("Mobile_FocusedDefocusedOmnibox_WithNoAction"));
  }
}

- (BOOL)textFieldShouldClear:(UITextField*)textField {
  if (!_textChangeDelegate) {
    // This can happen when the view controller is still alive but the model is
    // already deconstructed on shutdown.
    return YES;
  }
  _textChangeDelegate->ClearText();
  self.processingUserEvent = YES;
  return YES;
}

- (void)onCopy {
  self.omniboxInteractedWhileFocused = YES;
  if (!_textChangeDelegate) {
    // This can happen when the view controller is still alive but the model is
    // already deconstructed on shutdown.
    return;
  }
  _textChangeDelegate->OnCopy();
}

- (void)willPaste {
  if (!_textChangeDelegate) {
    // This can happen when the view controller is still alive but the model is
    // already deconstructed on shutdown.
    return;
  }
  _textChangeDelegate->WillPaste();
}

- (void)onDeleteBackward {
  if (!_textChangeDelegate) {
    // This can happen when the view controller is still alive but the model is
    // already deconstructed on shutdown.
    return;
  }
  _textChangeDelegate->OnDeleteBackward();
}

#pragma mark - OmniboxConsumer

- (void)updateAutocompleteIcon:(UIImage*)icon {
  [self.view setLeadingImage:icon];
}

- (void)updateSearchByImageSupported:(BOOL)searchByImageSupported {
  self.searchByImageEnabled = searchByImageSupported;
}

#pragma mark - EditViewAnimatee

- (void)setLeadingIconFaded:(BOOL)faded {
  CATransition* transition = [CATransition animation];
  transition.duration = 0.3;
  transition.timingFunction = [CAMediaTimingFunction
      functionWithName:kCAMediaTimingFunctionEaseInEaseOut];
  transition.type = kCATransitionFade;
  [self.view.layer addAnimation:transition forKey:nil];
  if (faded) {
    [self.view setLeadingImageAlpha:0];
    [self.view setLeadingImageScale:0];
  } else {
    [self.view setLeadingImageAlpha:1];
    [self.view setLeadingImageScale:1];
  }
}

- (void)setClearButtonFaded:(BOOL)faded {
  self.textField.rightView.alpha = faded ? 0 : 1;
}

#pragma mark - LocationBarOffsetProvider

- (CGFloat)xOffsetForString:(NSString*)string {
  return [self.textField offsetForString:string];
}

#pragma mark - private

// Tint color for the textfield placeholder and the clear button.
- (UIColor*)placeholderAndClearButtonColor {
  return [UIColor colorNamed:kTextfieldPlaceholderColor];
}

#pragma mark notification callbacks

// Called on UITextInputCurrentInputModeDidChangeNotification for self.textField
- (void)textInputModeDidChange {
  // Only respond to language changes when the omnibox is first responder.
  if (![self.textField isFirstResponder]) {
    return;
  }

  [self.textField updateTextDirection];
  self.semanticContentAttribute = [self.textField bestSemanticContentAttribute];

  [self.delegate omniboxViewControllerTextInputModeDidChange:self];
}

- (void)updateCachedClipboardState {
  // Sometimes, checking the clipboard state itself causes the clipboard to
  // emit a UIPasteboardChangedNotification, leading to an infinite loop. For
  // now, just prevent re-checking the clipboard state, but hopefully this will
  // be fixed in a future iOS version (see crbug.com/1049053 for crash details).
  if (self.isUpdatingCachedClipboardState) {
    return;
  }
  self.isUpdatingCachedClipboardState = YES;
  self.hasCopiedContent = NO;
  ClipboardRecentContent* clipboardRecentContent =
      ClipboardRecentContent::GetInstance();
  std::set<ClipboardContentType> desired_types;
  desired_types.insert(ClipboardContentType::URL);
  desired_types.insert(ClipboardContentType::Text);
  desired_types.insert(ClipboardContentType::Image);
  __weak __typeof(self) weakSelf = self;
  clipboardRecentContent->HasRecentContentFromClipboard(
      desired_types,
      base::BindOnce(^(std::set<ClipboardContentType> matched_types) {
        weakSelf.hasCopiedContent = !matched_types.empty();
        if (weakSelf.searchByImageEnabled &&
            matched_types.find(ClipboardContentType::Image) !=
                matched_types.end()) {
          weakSelf.copiedContentType = ClipboardContentType::Image;
        } else if (matched_types.find(ClipboardContentType::URL) !=
                   matched_types.end()) {
          weakSelf.copiedContentType = ClipboardContentType::URL;
        } else if (matched_types.find(ClipboardContentType::Text) !=
                   matched_types.end()) {
          weakSelf.copiedContentType = ClipboardContentType::Text;
        }
        self.isUpdatingCachedClipboardState = NO;
      }));
}

- (void)menuControllerWillShow:(NSNotification*)notification {
  if (self.showingEditMenu || !self.isTextfieldEditing ||
      !self.textField.window.isKeyWindow) {
    return;
  }

  self.showingEditMenu = YES;

  // Cancel original menu opening.
  UIMenuController* menuController = [UIMenuController sharedMenuController];
  [menuController hideMenu];

  // Reset where it should open below text field and reopen it.
  menuController.arrowDirection = UIMenuControllerArrowUp;

  [menuController showMenuFromView:self.textField rect:self.textField.frame];

  self.showingEditMenu = NO;
}

- (void)pasteboardDidChange:(NSNotification*)notification {
  [self updateCachedClipboardState];
}

- (void)applicationDidBecomeActive:(NSNotification*)notification {
  [self updateCachedClipboardState];
}

#pragma mark clear button

// Omnibox uses a custom clear button. It has a custom tint and image, but
// otherwise it should act exactly like a system button. To achieve this, a
// custom button is used as the |rightView|. Textfield's setRightViewMode: is
// used to make the button invisible when the textfield is empty; the visibility
// is updated on textfield text changes and clear button presses.
- (void)setupClearButton {
  // Do not use the system clear button. Use a custom "right view" instead.
  // Note that |rightView| is an incorrect name, it's really a trailing view.
  [self.textField setClearButtonMode:UITextFieldViewModeNever];
  [self.textField setRightViewMode:UITextFieldViewModeAlways];

  UIButton* clearButton = [UIButton buttonWithType:UIButtonTypeSystem];
  clearButton.frame = CGRectMake(0, 0, kClearButtonSize, kClearButtonSize);
  [clearButton setImage:[self clearButtonIcon] forState:UIControlStateNormal];
  [clearButton addTarget:self
                  action:@selector(clearButtonPressed)
        forControlEvents:UIControlEventTouchUpInside];
  self.textField.rightView = clearButton;

  clearButton.tintColor = [self placeholderAndClearButtonColor];
  SetA11yLabelAndUiAutomationName(clearButton, IDS_IOS_ACCNAME_CLEAR_TEXT,
                                  @"Clear Text");

  clearButton.pointerInteractionEnabled = YES;
  clearButton.pointerStyleProvider =
      CreateLiftEffectCirclePointerStyleProvider();

  // Observe text changes to show the clear button when there is text and hide
  // it when the textfield is empty.
  [self.textField addTarget:self
                     action:@selector(textFieldDidChange:)
           forControlEvents:UIControlEventEditingChanged];
}

- (UIImage*)clearButtonIcon {
  UIImage* image = [[UIImage imageNamed:@"omnibox_clear_icon"]
      imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];

  return image;
}

- (void)clearButtonPressed {
  // Emulate a system button clear callback.
  BOOL shouldClear =
      [self.textField.delegate textFieldShouldClear:self.textField];
  if (shouldClear) {
    [self.textField setText:@""];
    // Calling setText: does not trigger UIControlEventEditingChanged, so update
    // the clear button visibility manually.
    [self.textField sendActionsForControlEvents:UIControlEventEditingChanged];
  }
}

// Hides the clear button if the textfield is empty; shows it otherwise.
- (void)updateClearButtonVisibility {
  BOOL hasText = self.textField.text.length > 0;
  [self.textField setRightViewMode:hasText ? UITextFieldViewModeAlways
                                           : UITextFieldViewModeNever];
}

// Handle the updates to semanticContentAttribute by passing the changes along
// to the necessary views.
- (void)setSemanticContentAttribute:
    (UISemanticContentAttribute)semanticContentAttribute {
  _semanticContentAttribute = semanticContentAttribute;

  self.view.semanticContentAttribute = self.semanticContentAttribute;
  self.textField.semanticContentAttribute = self.semanticContentAttribute;
}

#pragma mark - UIMenuItem

- (BOOL)canPerformAction:(SEL)action withSender:(id)sender {
  if (action == @selector(searchCopiedImage:) ||
      action == @selector(visitCopiedLink:) ||
      action == @selector(searchCopiedText:)) {
    if (!self.hasCopiedContent) {
      return NO;
    }
    if (self.copiedContentType == ClipboardContentType::Image) {
      return action == @selector(searchCopiedImage:);
    }
    if (self.copiedContentType == ClipboardContentType::URL) {
      return action == @selector(visitCopiedLink:);
    }
    if (self.copiedContentType == ClipboardContentType::Text) {
      return action == @selector(searchCopiedText:);
    }
    return NO;
  }
  return NO;
}

- (void)searchCopiedImage:(id)sender {
  RecordAction(
      UserMetricsAction("Mobile.OmniboxContextMenu.SearchCopiedImage"));
  self.omniboxInteractedWhileFocused = YES;
  [self.delegate omniboxViewControllerSearchCopiedImage:self];
}

- (void)visitCopiedLink:(id)sender {
  // A search using clipboard link is activity that should indicate a user
  // that would be interested in setting Chrome as the default browser.
  LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeGeneral);
  [self.delegate omniboxViewControllerUserDidVisitCopiedLink:self];
  RecordAction(UserMetricsAction("Mobile.OmniboxContextMenu.VisitCopiedLink"));
  self.omniboxInteractedWhileFocused = YES;
  ClipboardRecentContent::GetInstance()->GetRecentURLFromClipboard(
      base::BindOnce(^(absl::optional<GURL> optionalURL) {
        if (!optionalURL) {
          return;
        }
        NSString* url = base::SysUTF8ToNSString(optionalURL.value().spec());
        dispatch_async(dispatch_get_main_queue(), ^{
          [self.dispatcher loadQuery:url immediately:YES];
          [self.dispatcher cancelOmniboxEdit];
        });
      }));
}

- (void)searchCopiedText:(id)sender {
  // A search using clipboard text is activity that should indicate a user
  // that would be interested in setting Chrome as the default browser.
  LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeGeneral);
  RecordAction(UserMetricsAction("Mobile.OmniboxContextMenu.SearchCopiedText"));
  self.omniboxInteractedWhileFocused = YES;
  ClipboardRecentContent::GetInstance()->GetRecentTextFromClipboard(
      base::BindOnce(^(absl::optional<std::u16string> optionalText) {
        if (!optionalText) {
          return;
        }
        NSString* query = base::SysUTF16ToNSString(optionalText.value());
        dispatch_async(dispatch_get_main_queue(), ^{
          [self.dispatcher loadQuery:query immediately:YES];
          [self.dispatcher cancelOmniboxEdit];
        });
      }));
}

#pragma mark - UIScribbleInteractionDelegate

- (void)scribbleInteractionWillBeginWriting:(UIScribbleInteraction*)interaction
    API_AVAILABLE(ios(14.0)) {
  if (self.textField.isPreEditing) {
    [self.textField exitPreEditState];
    [self.textField setText:[[NSAttributedString alloc] initWithString:@""]
             userTextLength:0];
  }

  [self.textField clearAutocompleteText];
}

- (void)scribbleInteractionDidFinishWriting:(UIScribbleInteraction*)interaction
    API_AVAILABLE(ios(14.0)) {
  [self cleanupOmniboxAfterScribble];

  // Dismiss any inline autocomplete. The user expectation is to not have it.
  [self.textField clearAutocompleteText];
}

@end
