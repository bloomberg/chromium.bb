// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility_android.h"

#include <algorithm>
#include <unordered_map>

#include "base/cxx17_backports.h"
#include "base/i18n/break_iterator.h"
#include "base/lazy_instance.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/accessibility/browser_accessibility_manager_android.h"
#include "content/public/common/content_client.h"
#include "third_party/blink/public/strings/grit/blink_strings.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/ax_assistant_structure.h"
#include "ui/accessibility/ax_node_position.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/accessibility/platform/ax_android_constants.h"
#include "ui/accessibility/platform/ax_unique_id.h"

namespace {

// These are enums from android.text.InputType in Java:
enum {
  ANDROID_TEXT_INPUTTYPE_TYPE_NULL = 0,
  ANDROID_TEXT_INPUTTYPE_TYPE_DATETIME = 0x4,
  ANDROID_TEXT_INPUTTYPE_TYPE_DATETIME_DATE = 0x14,
  ANDROID_TEXT_INPUTTYPE_TYPE_DATETIME_TIME = 0x24,
  ANDROID_TEXT_INPUTTYPE_TYPE_NUMBER = 0x2,
  ANDROID_TEXT_INPUTTYPE_TYPE_PHONE = 0x3,
  ANDROID_TEXT_INPUTTYPE_TYPE_TEXT = 0x1,
  ANDROID_TEXT_INPUTTYPE_TYPE_TEXT_URI = 0x11,
  ANDROID_TEXT_INPUTTYPE_TYPE_TEXT_WEB_EDIT_TEXT = 0xa1,
  ANDROID_TEXT_INPUTTYPE_TYPE_TEXT_WEB_EMAIL = 0xd1,
  ANDROID_TEXT_INPUTTYPE_TYPE_TEXT_WEB_PASSWORD = 0xe1
};

// These are enums from android.view.View in Java:
enum {
  ANDROID_VIEW_VIEW_ACCESSIBILITY_LIVE_REGION_NONE = 0,
  ANDROID_VIEW_VIEW_ACCESSIBILITY_LIVE_REGION_POLITE = 1,
  ANDROID_VIEW_VIEW_ACCESSIBILITY_LIVE_REGION_ASSERTIVE = 2
};

// These are enums from
// android.view.accessibility.AccessibilityNodeInfo.RangeInfo in Java:
enum { ANDROID_VIEW_ACCESSIBILITY_RANGE_TYPE_FLOAT = 1 };

}  // namespace

namespace content {

namespace {
// The minimum amount of characters that must be typed into a text field before
// AT will communicate invalid content to the user.
constexpr int kMinimumCharacterCountForInvalid = 7;
}  // namespace

// static
BrowserAccessibility* BrowserAccessibility::Create() {
  return new BrowserAccessibilityAndroid();
}

using UniqueIdMap = std::unordered_map<int32_t, BrowserAccessibilityAndroid*>;
// Map from each AXPlatformNode's unique id to its instance.
base::LazyInstance<UniqueIdMap>::Leaky g_unique_id_map =
    LAZY_INSTANCE_INITIALIZER;

// static
BrowserAccessibilityAndroid* BrowserAccessibilityAndroid::GetFromUniqueId(
    int32_t unique_id) {
  UniqueIdMap* unique_ids = g_unique_id_map.Pointer();
  auto iter = unique_ids->find(unique_id);
  if (iter != unique_ids->end())
    return iter->second;

  return nullptr;
}

BrowserAccessibilityAndroid::BrowserAccessibilityAndroid() {
  g_unique_id_map.Get()[unique_id()] = this;
}

BrowserAccessibilityAndroid::~BrowserAccessibilityAndroid() {
  if (unique_id())
    g_unique_id_map.Get().erase(unique_id());
}

void BrowserAccessibilityAndroid::OnLocationChanged() {
  auto* manager =
      static_cast<BrowserAccessibilityManagerAndroid*>(this->manager());
  manager->FireLocationChanged(this);
}

std::u16string
BrowserAccessibilityAndroid::GetLocalizedStringForImageAnnotationStatus(
    ax::mojom::ImageAnnotationStatus status) const {
  // Default to standard text, except for special case of eligible.
  if (status != ax::mojom::ImageAnnotationStatus::kEligibleForAnnotation)
    return BrowserAccessibility::GetLocalizedStringForImageAnnotationStatus(
        status);

  ContentClient* content_client = content::GetContentClient();

  int message_id = 0;

  switch (static_cast<ax::mojom::WritingDirection>(
      GetIntAttribute(ax::mojom::IntAttribute::kTextDirection))) {
    case ax::mojom::WritingDirection::kRtl:
      message_id = IDS_AX_IMAGE_ELIGIBLE_FOR_ANNOTATION_ANDROID_RTL;
      break;
    case ax::mojom::WritingDirection::kTtb:
    case ax::mojom::WritingDirection::kBtt:
    case ax::mojom::WritingDirection::kNone:
    case ax::mojom::WritingDirection::kLtr:
      message_id = IDS_AX_IMAGE_ELIGIBLE_FOR_ANNOTATION_ANDROID_LTR;
      break;
  }

  DCHECK(message_id);

  return content_client->GetLocalizedString(message_id);
}

void BrowserAccessibilityAndroid::AppendTextToString(
    std::u16string extra_text,
    std::u16string* string) const {
  if (extra_text.empty())
    return;

  if (string->empty()) {
    *string = extra_text;
    return;
  }

  *string += std::u16string(u", ") + extra_text;
}

bool BrowserAccessibilityAndroid::IsCheckable() const {
  return GetData().HasCheckedState();
}

bool BrowserAccessibilityAndroid::IsChecked() const {
  return GetData().GetCheckedState() == ax::mojom::CheckedState::kTrue;
}

bool BrowserAccessibilityAndroid::IsClickable() const {
  // If it has a custom default action verb except for
  // ax::mojom::DefaultActionVerb::kClickAncestor, it's definitely clickable.
  // ax::mojom::DefaultActionVerb::kClickAncestor is used when an element with a
  // click listener is present in its ancestry chain.
  if (HasIntAttribute(ax::mojom::IntAttribute::kDefaultActionVerb) &&
      (GetData().GetDefaultActionVerb() !=
       ax::mojom::DefaultActionVerb::kClickAncestor)) {
    return true;
  }

  if (IsHeadingLink())
    return true;

  if (!IsEnabled()) {
    // TalkBack won't announce a control as disabled unless it's also marked
    // as clickable. In other words, Talkback wants to know if the control
    // might be clickable, if it wasn't disabled.
    return ui::IsControl(GetRole());
  }

  // Skip web areas, PDFs and iframes, they're focusable but not clickable.
  if (ui::IsIframe(GetRole()) || ui::IsPlatformDocument(GetRole()))
    return false;

  // Otherwise it's clickable if it's a control.
  return ui::IsControlOnAndroid(GetRole(), IsFocusable());
}

bool BrowserAccessibilityAndroid::IsCollapsed() const {
  return HasState(ax::mojom::State::kCollapsed);
}

// TODO(dougt) Move to ax_role_properties?
bool BrowserAccessibilityAndroid::IsCollection() const {
  return (ui::IsTableLike(GetRole()) || GetRole() == ax::mojom::Role::kList ||
          GetRole() == ax::mojom::Role::kListBox ||
          GetRole() == ax::mojom::Role::kDescriptionList ||
          GetRole() == ax::mojom::Role::kDirectory ||
          GetRole() == ax::mojom::Role::kTree);
}

bool BrowserAccessibilityAndroid::IsCollectionItem() const {
  return (GetRole() == ax::mojom::Role::kCell ||
          GetRole() == ax::mojom::Role::kColumnHeader ||
          GetRole() == ax::mojom::Role::kDescriptionListTerm ||
          GetRole() == ax::mojom::Role::kListBoxOption ||
          GetRole() == ax::mojom::Role::kListItem ||
          GetRole() == ax::mojom::Role::kRowHeader ||
          GetRole() == ax::mojom::Role::kTreeItem);
}

bool BrowserAccessibilityAndroid::IsCombobox() const {
  return (GetRole() == ax::mojom::Role::kComboBoxGrouping ||
          GetRole() == ax::mojom::Role::kTextFieldWithComboBox ||
          GetRole() == ax::mojom::Role::kComboBoxMenuButton);
}

bool BrowserAccessibilityAndroid::IsComboboxControl() const {
  return (GetRole() == ax::mojom::Role::kTree ||
          GetRole() == ax::mojom::Role::kGrid ||
          GetRole() == ax::mojom::Role::kDialog ||
          GetRole() == ax::mojom::Role::kListBox);
}

bool BrowserAccessibilityAndroid::IsContentInvalid() const {
  if (HasIntAttribute(ax::mojom::IntAttribute::kInvalidState) &&
      GetData().GetInvalidState() != ax::mojom::InvalidState::kFalse) {
    // We will not report content as invalid until a certain number of
    // characters have been typed to prevent verbose announcements to the user.
    return (GetInnerText().length() > kMinimumCharacterCountForInvalid);
  }

  return false;
}

bool BrowserAccessibilityAndroid::IsDisabledDescendant() const {
  // Iterate over parents and see if any are disabled.
  BrowserAccessibilityAndroid* parent =
      static_cast<BrowserAccessibilityAndroid*>(PlatformGetParent());
  while (parent != nullptr) {
    if (!parent->IsEnabled()) {
      return true;
    }
    parent =
        static_cast<BrowserAccessibilityAndroid*>(parent->PlatformGetParent());
  }

  return false;
}

bool BrowserAccessibilityAndroid::IsDismissable() const {
  return false;  // No concept of "dismissable" on the web currently.
}

bool BrowserAccessibilityAndroid::IsEnabled() const {
  switch (GetData().GetRestriction()) {
    case ax::mojom::Restriction::kNone:
      return true;
    case ax::mojom::Restriction::kReadOnly:
    case ax::mojom::Restriction::kDisabled:
      // On Android, both Disabled and ReadOnly are treated the same.
      // For both of them, we set AccessibilityNodeInfo.IsEnabled to false
      // and we don't expose certain actions like SET_VALUE and PASTE.
      return false;
  }

  NOTREACHED();
  return true;
}

bool BrowserAccessibilityAndroid::IsExpanded() const {
  return HasState(ax::mojom::State::kExpanded);
}

bool BrowserAccessibilityAndroid::IsFocusable() const {
  // If it's an iframe element, or the root element of a child frame that isn't
  // inside a portal, only mark it as focusable if the element has an explicit
  // name. Otherwise mark it as not focusable to avoid the user landing on empty
  // container elements in the tree.
  if (ui::IsIframe(GetRole()) ||
      (ui::IsPlatformDocument(GetRole()) && PlatformGetParent() &&
       PlatformGetParent()->GetRole() != ax::mojom::Role::kPortal)) {
    return HasStringAttribute(ax::mojom::StringAttribute::kName);
  }

  return HasState(ax::mojom::State::kFocusable);
}

bool BrowserAccessibilityAndroid::IsFormDescendant() const {
  // Iterate over parents and see if any are a form.
  const BrowserAccessibility* parent = PlatformGetParent();
  while (parent != nullptr) {
    if (ui::IsForm(parent->GetRole())) {
      return true;
    }
    parent = parent->PlatformGetParent();
  }

  return false;
}

bool BrowserAccessibilityAndroid::IsHeading() const {
  BrowserAccessibilityAndroid* parent =
      static_cast<BrowserAccessibilityAndroid*>(PlatformGetParent());
  if (parent && parent->IsHeading())
    return true;

  return ui::IsHeadingOrTableHeader(GetRole());
}

bool BrowserAccessibilityAndroid::IsHierarchical() const {
  return (GetRole() == ax::mojom::Role::kTree || IsHierarchicalList());
}

bool BrowserAccessibilityAndroid::IsLink() const {
  return ui::IsLink(GetRole());
}

bool BrowserAccessibilityAndroid::IsMultiLine() const {
  return HasState(ax::mojom::State::kMultiline);
}

bool BrowserAccessibilityAndroid::IsMultiselectable() const {
  return HasState(ax::mojom::State::kMultiselectable);
}

bool BrowserAccessibilityAndroid::IsReportingCheckable() const {
  // To communicate kMixed state Checkboxes, we will rely on state description,
  // so we will not report node as checkable to avoid duplicate utterances.
  return IsCheckable() &&
         GetData().GetCheckedState() != ax::mojom::CheckedState::kMixed;
}

bool BrowserAccessibilityAndroid::IsScrollable() const {
  return GetBoolAttribute(ax::mojom::BoolAttribute::kScrollable);
}

bool BrowserAccessibilityAndroid::IsSeekControl() const {
  // Range types should have seek control options, except progress bars.
  return GetData().IsRangeValueSupported() &&
         (GetRole() != ax::mojom::Role::kProgressIndicator);
}

bool BrowserAccessibilityAndroid::IsSelected() const {
  return GetBoolAttribute(ax::mojom::BoolAttribute::kSelected);
}

bool BrowserAccessibilityAndroid::IsSlider() const {
  return GetRole() == ax::mojom::Role::kSlider;
}

bool BrowserAccessibilityAndroid::IsVisibleToUser() const {
  return !IsInvisibleOrIgnored();
}

bool BrowserAccessibilityAndroid::IsInterestingOnAndroid() const {
  // The root is not interesting if it doesn't have a title, even
  // though it's focusable.
  if (ui::IsPlatformDocument(GetRole()) && GetInnerText().empty())
    return false;

  // The root inside a portal is not interesting.
  if (ui::IsPlatformDocument(GetRole()) && PlatformGetParent() &&
      PlatformGetParent()->GetRole() == ax::mojom::Role::kPortal) {
    return false;
  }

  // Mark as uninteresting if it's hidden, even if it is focusable.
  if (IsInvisibleOrIgnored())
    return false;

  // Walk up the ancestry. A non-focusable child of a control is not
  // interesting. A child of an invisible iframe is also not interesting.
  const BrowserAccessibility* parent = PlatformGetParent();
  while (parent) {
    if (ui::IsControl(parent->GetRole()) && !IsFocusable())
      return false;

    if (parent->GetRole() == ax::mojom::Role::kIframe &&
        parent->IsInvisibleOrIgnored()) {
      return false;
    }

    parent = parent->PlatformGetParent();
  }

  // Otherwise, focusable nodes are always interesting. Note that IsFocusable()
  // already skips over things like iframes and child frames that are
  // technically focusable but shouldn't be exposed as focusable on Android.
  if (IsFocusable())
    return true;

  // If it's not focusable but has a control role, then it's interesting.
  if (ui::IsControl(GetRole()))
    return true;

  // Mark progress indicators as interesting, since they are not focusable and
  // not a control, but users should be able to swipe/navigate to them.
  if (GetRole() == ax::mojom::Role::kProgressIndicator)
    return true;

  // If we are the direct descendant of a link and have no siblings/children,
  // then we are not interesting, return false
  parent = PlatformGetParent();
  if (parent != nullptr && ui::IsLink(parent->GetRole()) &&
      parent->PlatformChildCount() == 1 && PlatformChildCount() == 0) {
    return false;
  }

  // Otherwise, the interesting nodes are leaf nodes with non-whitespace text.
  return IsLeaf() &&
         !base::ContainsOnlyChars(GetInnerText(), base::kWhitespaceUTF16);
}

bool BrowserAccessibilityAndroid::IsHeadingLink() const {
  if (!(GetRole() == ax::mojom::Role::kHeading && InternalChildCount() == 1))
    return false;

  BrowserAccessibilityAndroid* child =
      static_cast<BrowserAccessibilityAndroid*>(InternalChildrenBegin().get());
  return child->IsLink();
}

const BrowserAccessibilityAndroid*
BrowserAccessibilityAndroid::GetSoleInterestingNodeFromSubtree() const {
  if (IsInterestingOnAndroid())
    return this;

  const BrowserAccessibilityAndroid* sole_interesting_node = nullptr;
  for (const auto& child : PlatformChildren()) {
    const BrowserAccessibilityAndroid* interesting_node =
        static_cast<const BrowserAccessibilityAndroid&>(child)
            .GetSoleInterestingNodeFromSubtree();
    if (interesting_node && sole_interesting_node) {
      // If there are two interesting nodes, return nullptr.
      return nullptr;
    } else if (interesting_node) {
      sole_interesting_node = interesting_node;
    }
  }

  return sole_interesting_node;
}

bool BrowserAccessibilityAndroid::AreInlineTextBoxesLoaded() const {
  if (IsText())
    return InternalChildCount() > 0;

  // Return false if any descendant needs to load inline text boxes.
  for (auto it = InternalChildrenBegin(); it != InternalChildrenEnd(); ++it) {
    BrowserAccessibilityAndroid* child =
        static_cast<BrowserAccessibilityAndroid*>(it.get());
    if (!child->AreInlineTextBoxesLoaded())
      return false;
  }

  // Otherwise return true - either they're all loaded, or there aren't
  // any descendants that need to load inline text boxes.
  return true;
}

bool BrowserAccessibilityAndroid::CanOpenPopup() const {
  return HasIntAttribute(ax::mojom::IntAttribute::kHasPopup);
}

const char* BrowserAccessibilityAndroid::GetClassName() const {
  ax::mojom::Role role = GetRole();

  // On Android, contenteditable needs to be handled the same as any
  // other text field.
  if (IsTextField())
    role = ax::mojom::Role::kTextField;

  return ui::AXRoleToAndroidClassName(role, PlatformGetParent() != nullptr);
}

bool BrowserAccessibilityAndroid::IsChildOfLeaf() const {
  BrowserAccessibility* ancestor = InternalGetParent();

  while (ancestor) {
    if (ancestor->IsLeaf())
      return true;
    ancestor = ancestor->InternalGetParent();
  }

  return false;
}

bool BrowserAccessibilityAndroid::IsLeaf() const {
  if (BrowserAccessibility::IsLeaf())
    return true;

  // Document roots (e.g. kRootWebArea and kPdfRoot), and iframes are always
  // allowed to contain children.
  if (ui::IsIframe(GetRole()) || ui::IsPlatformDocument(GetRole()))
    return false;

  // Button, date and time controls should not expose their children to Android
  // accessibility APIs.
  switch (GetRole()) {
    case ax::mojom::Role::kButton:
    case ax::mojom::Role::kDate:
    case ax::mojom::Role::kDateTime:
    case ax::mojom::Role::kInputTime:
      return true;
    default:
      break;
  }

  // Links are never leaves.
  if (IsLink())
    return false;

  BrowserAccessibilityManagerAndroid* manager_android =
      static_cast<BrowserAccessibilityManagerAndroid*>(manager());
  if (manager_android->prune_tree_for_screen_reader()) {
    // For some nodes, we will consider children before determining if the node
    // is a leaf. For nodes with relevant children, we will return false here
    // and allow the child nodes to be set as a leaf.

    // Headings with text can drop their children (with exceptions).
    std::u16string name = GetInnerText();
    if (GetRole() == ax::mojom::Role::kHeading && !name.empty()) {
      return IsLeafConsideringChildren();
    }

    // Focusable nodes with text can drop their children (with exceptions).
    if (HasState(ax::mojom::State::kFocusable) && !name.empty()) {
      return IsLeafConsideringChildren();
    }

    // Nodes with only static text can drop their children.
    if (HasOnlyTextChildren())
      return true;
  }

  return false;
}

bool BrowserAccessibilityAndroid::IsLeafConsideringChildren() const {
  // This is called from IsLeaf, so don't call PlatformChildCount
  // from within this!

  // Check for any children that should be exposed and return false if found (by
  // returning false we are saying the parent node is NOT a leaf and this child
  // node should instead be the leaf).
  //
  // If a node has a child that meets any of these criteria, it is NOT a leaf:
  //
  //   * child is focusable, and NOT a menu option
  //   * child is a table, cell, or row
  //
  for (auto it = InternalChildrenBegin(); it != InternalChildrenEnd(); ++it) {
    BrowserAccessibility* child = it.get();

    if (child->HasState(ax::mojom::State::kFocusable) &&
        child->GetRole() != ax::mojom::Role::kMenuListOption) {
      return false;
    }

    if (child->GetRole() == ax::mojom::Role::kTable ||
        child->GetRole() == ax::mojom::Role::kCell ||
        child->GetRole() == ax::mojom::Role::kRow ||
        child->GetRole() == ax::mojom::Role::kLayoutTable ||
        child->GetRole() == ax::mojom::Role::kLayoutTableCell ||
        child->GetRole() == ax::mojom::Role::kLayoutTableRow) {
      return false;
    }

    // Check nested children and return false if any meet above criteria.
    if (!static_cast<BrowserAccessibilityAndroid*>(child)
             ->IsLeafConsideringChildren())
      return false;
  }

  // If no such children were found, return true signaling the parent node can
  // be the leaf node.
  return true;
}

// Note: this is used to compute an object's name on Android, and is exposed as
// the name field in Android dump tree tests.
// TODO(accessibility) Should it be called GetName() so that engineers not
// familiar with Android can find it more easily?
std::u16string BrowserAccessibilityAndroid::GetInnerText() const {
  if (ui::IsIframe(GetRole()))
    return std::u16string();

  // First, always return the |value| attribute if this is an
  // input field.
  std::u16string value = GetValueForControl();
  if (ShouldExposeValueAsName())
    return value;

  // For color wells, the color is stored in separate attributes.
  // Perhaps we could return color names in the future?
  if (GetRole() == ax::mojom::Role::kColorWell) {
    unsigned int color = static_cast<unsigned int>(
        GetIntAttribute(ax::mojom::IntAttribute::kColorValue));
    unsigned int red = SkColorGetR(color);
    unsigned int green = SkColorGetG(color);
    unsigned int blue = SkColorGetB(color);
    return base::UTF8ToUTF16(
        base::StringPrintf("#%02X%02X%02X", red, green, blue));
  }

  std::u16string text = GetNameAsString16();
  if (text.empty())
    text = value;

  // For almost all focusable nodes we try to get text from contents, but for
  // the root node that's redundant and often way too verbose.
  if (ui::IsPlatformDocument(GetRole()))
    return text;

  // A role="separator" is a leaf, and cannot get name from contents, even if
  // author appends text children.
  if (GetRole() == ax::mojom::Role::kSplitter)
    return text;

  // Append image description strings to the text, if not running as WebView.
  auto* manager =
      static_cast<BrowserAccessibilityManagerAndroid*>(this->manager());
  if (!manager->IsRunningAsWebView()) {
    auto status = GetData().GetImageAnnotationStatus();
    switch (status) {
      case ax::mojom::ImageAnnotationStatus::kEligibleForAnnotation:
      case ax::mojom::ImageAnnotationStatus::kAnnotationPending:
      case ax::mojom::ImageAnnotationStatus::kAnnotationEmpty:
      case ax::mojom::ImageAnnotationStatus::kAnnotationAdult:
      case ax::mojom::ImageAnnotationStatus::kAnnotationProcessFailed:
        AppendTextToString(GetLocalizedStringForImageAnnotationStatus(status),
                           &text);
        break;

      case ax::mojom::ImageAnnotationStatus::kAnnotationSucceeded:
        text =
            GetString16Attribute(ax::mojom::StringAttribute::kImageAnnotation);
        break;

      case ax::mojom::ImageAnnotationStatus::kNone:
      case ax::mojom::ImageAnnotationStatus::kWillNotAnnotateDueToScheme:
      case ax::mojom::ImageAnnotationStatus::kIneligibleForAnnotation:
      case ax::mojom::ImageAnnotationStatus::kSilentlyEligibleForAnnotation:
        break;
    }
  }

  // This is called from IsLeaf, so don't call PlatformChildCount
  // from within this!
  if (text.empty() && (HasOnlyTextChildren() ||
                       (IsFocusable() && HasOnlyTextAndImageChildren()))) {
    for (auto it = InternalChildrenBegin(); it != InternalChildrenEnd(); ++it) {
      text +=
          static_cast<BrowserAccessibilityAndroid*>(it.get())->GetInnerText();
    }
  }

  if (text.empty() &&
      (ui::IsLink(GetRole()) || ui::IsImageOrVideo(GetRole())) &&
      !HasExplicitlyEmptyName()) {
    std::u16string url = GetString16Attribute(ax::mojom::StringAttribute::kUrl);
    text = ui::AXUrlBaseText(url);
  }

  return text;
}

std::u16string BrowserAccessibilityAndroid::GetValueForControl() const {
  std::u16string value = BrowserAccessibility::GetValueForControl();

  // Optionally replace entered password text with bullet characters
  // based on a user preference.
  if (IsPasswordField()) {
    auto* manager =
        static_cast<BrowserAccessibilityManagerAndroid*>(this->manager());
    if (manager->ShouldRespectDisplayedPasswordText()) {
      // In the Chrome accessibility tree, the value of a password node is
      // unobscured. However, if ShouldRespectDisplayedPasswordText() returns
      // true we should try to expose whatever's actually visually displayed,
      // whether that's the actual password or dots or whatever. To do this
      // we rely on the password field's shadow dom.
      value = BrowserAccessibility::GetInnerText();
    } else if (!manager->ShouldExposePasswordText()) {
      value = std::u16string(value.size(), ui::kSecurePasswordBullet);
    }
  }

  return value;
}

std::u16string BrowserAccessibilityAndroid::GetHint() const {
  std::vector<std::u16string> strings;

  // If we're returning the value as the main text, the name needs to be
  // part of the hint.
  if (ShouldExposeValueAsName()) {
    std::u16string name = GetNameAsString16();
    if (!name.empty())
      strings.push_back(name);
  }

  if (GetData().GetNameFrom() != ax::mojom::NameFrom::kPlaceholder) {
    std::u16string placeholder =
        GetString16Attribute(ax::mojom::StringAttribute::kPlaceholder);
    if (!placeholder.empty())
      strings.push_back(placeholder);
  }

  std::u16string description =
      GetString16Attribute(ax::mojom::StringAttribute::kDescription);
  if (!description.empty())
    strings.push_back(description);

  return base::JoinString(strings, u" ");
}

std::u16string BrowserAccessibilityAndroid::GetDialogModalMessageText() const {
  // For a dialog/modal, first check for a name, and then a description. If
  // both are empty, fallback to a default "dialog opened." text.
  if (HasStringAttribute(ax::mojom::StringAttribute::kName)) {
    return GetString16Attribute(ax::mojom::StringAttribute::kName);
  }

  if (HasStringAttribute(ax::mojom::StringAttribute::kDescription)) {
    return GetString16Attribute(ax::mojom::StringAttribute::kDescription);
  }

  content::ContentClient* content_client = content::GetContentClient();
  return content_client->GetLocalizedString(IDS_AX_DIALOG_MODAL_OPENED);
}

std::u16string BrowserAccessibilityAndroid::GetStateDescription() const {
  std::vector<std::u16string> state_descs;

  // For multiselectable state, generate a state description. We do not set a
  // state description for pop up/<select> to prevent double utterances.
  if (IsMultiselectable() && GetRole() != ax::mojom::Role::kPopUpButton)
    state_descs.push_back(GetMultiselectableStateDescription());

  // For Checkboxes, if we are in a kMixed state, we will communicate
  // "partially checked" through the state description. This is mutually
  // exclusive with the on/off of toggle buttons below.
  if (IsCheckable() && !IsReportingCheckable()) {
    state_descs.push_back(GetCheckboxStateDescription());
  } else if (GetRole() == ax::mojom::Role::kToggleButton) {
    // For Toggle buttons, we will append "on"/"off" in the state description.
    state_descs.push_back(GetToggleButtonStateDescription());
  }

  // For list boxes, use state description to communicate child item count. We
  // will not communicate this in the case that the listbox is also
  // multiselectable and has some items selected, since the same info would be
  // communicated as "x of y selected".
  if (GetRole() == ax::mojom::Role::kListBox &&
      (!IsMultiselectable() || !GetSelectedItemCount()))
    state_descs.push_back(GetListBoxStateDescription());

  // For list box items, use state description to communicate index of item.
  if (GetRole() == ax::mojom::Role::kListBoxOption)
    state_descs.push_back(GetListBoxItemStateDescription());

  // For nodes with non-trivial aria-current values, communicate state.
  if (HasAriaCurrent())
    state_descs.push_back(GetAriaCurrentStateDescription());

  // Concatenate all state descriptions and return.
  return base::JoinString(state_descs, u" ");
}

std::u16string BrowserAccessibilityAndroid::GetMultiselectableStateDescription()
    const {
  content::ContentClient* content_client = content::GetContentClient();

  // Count the number of children and selected children.
  int child_count = 0;
  int selected_count = 0;
  for (const auto& child : PlatformChildren()) {
    child_count++;
    const BrowserAccessibilityAndroid& android_child =
        static_cast<const BrowserAccessibilityAndroid&>(child);
    if (android_child.IsSelected())
      selected_count++;
  }

  // If none are selected, return special case.
  if (!selected_count)
    return content_client->GetLocalizedString(
        IDS_AX_MULTISELECTABLE_STATE_DESCRIPTION_NONE);

  // Generate a state description of the form: "multiselectable, x of y
  // selected.".
  std::vector<std::u16string> values;
  values.push_back(base::NumberToString16(selected_count));
  values.push_back(base::NumberToString16(child_count));
  return base::ReplaceStringPlaceholders(
      content_client->GetLocalizedString(
          IDS_AX_MULTISELECTABLE_STATE_DESCRIPTION),
      values, nullptr);
}

std::u16string BrowserAccessibilityAndroid::GetToggleButtonStateDescription()
    const {
  content::ContentClient* content_client = content::GetContentClient();

  // For checked Toggle buttons, we will return "on", otherwise "off".
  if (IsChecked())
    return content_client->GetLocalizedString(IDS_AX_TOGGLE_BUTTON_ON);

  return content_client->GetLocalizedString(IDS_AX_TOGGLE_BUTTON_OFF);
}

std::u16string BrowserAccessibilityAndroid::GetCheckboxStateDescription()
    const {
  content::ContentClient* content_client = content::GetContentClient();

  return content_client->GetLocalizedString(IDS_AX_CHECKBOX_PARTIALLY_CHECKED);
}

std::u16string BrowserAccessibilityAndroid::GetListBoxStateDescription() const {
  content::ContentClient* content_client = content::GetContentClient();

  // For empty list boxes, we will return an empty string.
  int item_count = GetItemCount();
  if (!item_count)
    return std::u16string();

  // Otherwise, we will communicate "x items" as the state description.
  return base::ReplaceStringPlaceholders(
      content_client->GetLocalizedString(IDS_AX_LIST_BOX_STATE_DESCRIPTION),
      base::NumberToString16(item_count), nullptr);
}

std::u16string BrowserAccessibilityAndroid::GetListBoxItemStateDescription()
    const {
  content::ContentClient* content_client = content::GetContentClient();

  BrowserAccessibilityAndroid* parent =
      static_cast<BrowserAccessibilityAndroid*>(PlatformGetParent());

  // If we cannot find the parent collection, escape with an empty string.
  if (!parent)
    return std::u16string();

  // For list box items, we will communicate "in list, item x of y". We add
  // one (1) to our index to offset from counting at 0.
  int item_index = GetItemIndex() + 1;
  int item_count = parent->GetItemCount();

  return base::ReplaceStringPlaceholders(
      content_client->GetLocalizedString(
          IDS_AX_LIST_BOX_ITEM_STATE_DESCRIPTION),
      std::vector<std::u16string>({base::NumberToString16(item_index),
                                   base::NumberToString16(item_count)}),
      nullptr);
}

std::u16string BrowserAccessibilityAndroid::GetAriaCurrentStateDescription()
    const {
  content::ContentClient* content_client = content::GetContentClient();

  int message_id;
  switch (static_cast<ax::mojom::AriaCurrentState>(
      GetIntAttribute(ax::mojom::IntAttribute::kAriaCurrentState))) {
    case ax::mojom::AriaCurrentState::kPage:
      message_id = IDS_AX_ARIA_CURRENT_PAGE;
      break;
    case ax::mojom::AriaCurrentState::kStep:
      message_id = IDS_AX_ARIA_CURRENT_STEP;
      break;
    case ax::mojom::AriaCurrentState::kLocation:
      message_id = IDS_AX_ARIA_CURRENT_LOCATION;
      break;
    case ax::mojom::AriaCurrentState::kDate:
      message_id = IDS_AX_ARIA_CURRENT_DATE;
      break;
    case ax::mojom::AriaCurrentState::kTime:
      message_id = IDS_AX_ARIA_CURRENT_TIME;
      break;
    case ax::mojom::AriaCurrentState::kTrue:
    default:
      message_id = IDS_AX_ARIA_CURRENT_TRUE;
      break;
  }

  return content_client->GetLocalizedString(message_id);
}

std::u16string BrowserAccessibilityAndroid::GetComboboxExpandedText() const {
  content::ContentClient* content_client = content::GetContentClient();

  // We consider comboboxes of the form:
  //
  // <div role="combobox">
  //   <input type="text" aria-controls="options">
  //   <ul role="listbox" id="options">...</ul> (Can be outside <div>)
  // </div>
  //
  // Find child input node:
  const BrowserAccessibilityAndroid* input_node = nullptr;
  for (const auto& child : PlatformChildren()) {
    const BrowserAccessibilityAndroid& android_child =
        static_cast<const BrowserAccessibilityAndroid&>(child);
    if (android_child.IsTextField()) {
      input_node = &android_child;
      break;
    }
  }

  // If we have not found a child input element, consider aria 1.0 spec:
  //
  // <input type="text" role="combobox" aria-owns="options">
  // <ul role="listbox" id="options">...</ul>
  //
  // Check if |this| is the input, otherwise try our fallbacks.
  if (!input_node) {
    if (IsTextField()) {
      input_node = this;
    } else {
      return GetComboboxExpandedTextFallback();
    }
  }

  // Get the aria-controls nodes of |input_node|.
  std::vector<BrowserAccessibility*> controls =
      manager()->GetAriaControls(input_node);

  // |input_node| should control only one element, if it doesn't, try fallbacks.
  if (controls.size() != 1)
    return GetComboboxExpandedTextFallback();

  // |controlled_node| needs to be a combobox control, if not, try fallbacks.
  BrowserAccessibilityAndroid* controlled_node =
      static_cast<BrowserAccessibilityAndroid*>(controls[0]);
  if (!controlled_node->IsComboboxControl())
    return GetComboboxExpandedTextFallback();

  // For dialogs, return special case string.
  if (controlled_node->GetRole() == ax::mojom::Role::kDialog)
    return content_client->GetLocalizedString(IDS_AX_COMBOBOX_EXPANDED_DIALOG);

  // Find |controlled_node| set size, or return default string.
  if (!controlled_node->GetSetSize())
    return content_client->GetLocalizedString(
        IDS_AX_COMBOBOX_EXPANDED_AUTOCOMPLETE_DEFAULT);

  // Replace placeholder with count and return string.
  return base::ReplaceStringPlaceholders(
      content_client->GetLocalizedString(
          IDS_AX_COMBOBOX_EXPANDED_AUTOCOMPLETE_X_OPTIONS_AVAILABLE),
      base::NumberToString16(*controlled_node->GetSetSize()), nullptr);
}

std::u16string BrowserAccessibilityAndroid::GetComboboxExpandedTextFallback()
    const {
  content::ContentClient* content_client = content::GetContentClient();

  // If a combobox was of an indeterminate form, attempt any special cases here,
  // or return "expanded" as a final option.

  // Check for child nodes that are collections.
  int child_collection_count = 0;
  const BrowserAccessibilityAndroid* collection_node = nullptr;
  for (const auto& child : PlatformChildren()) {
    const auto& android_child =
        static_cast<const BrowserAccessibilityAndroid&>(child);
    if (android_child.IsCollection()) {
      child_collection_count++;
      collection_node = &android_child;
    }
  }

  // If we find none, or more than one, we will not be able to determine the
  // correct utterance, so return a default string instead.
  if (child_collection_count != 1)
    return content_client->GetLocalizedString(IDS_AX_COMBOBOX_EXPANDED);

  // Find |collection_node| set size, or return defaul string.
  if (!collection_node->GetSetSize())
    return content_client->GetLocalizedString(
        IDS_AX_COMBOBOX_EXPANDED_AUTOCOMPLETE_DEFAULT);

  // Replace placeholder with count and return string.
  return base::ReplaceStringPlaceholders(
      content_client->GetLocalizedString(
          IDS_AX_COMBOBOX_EXPANDED_AUTOCOMPLETE_X_OPTIONS_AVAILABLE),
      base::NumberToString16(*collection_node->GetSetSize()), nullptr);
}

std::string BrowserAccessibilityAndroid::GetRoleString() const {
  return ui::ToString(GetRole());
}

std::u16string BrowserAccessibilityAndroid::GetRoleDescription() const {
  content::ContentClient* content_client = content::GetContentClient();

  // As a special case, if we have a heading level return a string like
  // "heading level 1", etc. - and if the heading consists of a link,
  // append the word link as well.
  if (GetRole() == ax::mojom::Role::kHeading) {
    std::u16string role_description;
    int level = GetIntAttribute(ax::mojom::IntAttribute::kHierarchicalLevel);
    if (level >= 1 && level <= 6) {
      std::vector<std::u16string> values;
      values.push_back(base::NumberToString16(level));
      role_description = base::ReplaceStringPlaceholders(
          content_client->GetLocalizedString(IDS_AX_ROLE_HEADING_WITH_LEVEL),
          values, nullptr);
    } else {
      role_description =
          content_client->GetLocalizedString(IDS_AX_ROLE_HEADING);
    }

    if (IsHeadingLink()) {
      role_description +=
          u" " + content_client->GetLocalizedString(IDS_AX_ROLE_LINK);
    }

    return role_description;
  }

  // If this node is a link and the parent is a heading, return the role
  // description of the parent (e.g. "heading 1 link").
  if (ui::IsLink(GetRole()) && PlatformGetParent()) {
    BrowserAccessibilityAndroid* parent =
        static_cast<BrowserAccessibilityAndroid*>(PlatformGetParent());
    if (parent->IsHeadingLink())
      return parent->GetRoleDescription();
  }

  // If this node is an image, check status and potentially add unlabeled role.
  // If running inside a WebView, do not change default image role description.
  auto* manager =
      static_cast<BrowserAccessibilityManagerAndroid*>(this->manager());
  if (!manager->IsRunningAsWebView()) {
    auto status = GetData().GetImageAnnotationStatus();
    switch (status) {
      case ax::mojom::ImageAnnotationStatus::kEligibleForAnnotation:
      case ax::mojom::ImageAnnotationStatus::kAnnotationPending:
      case ax::mojom::ImageAnnotationStatus::kAnnotationEmpty:
      case ax::mojom::ImageAnnotationStatus::kAnnotationAdult:
      case ax::mojom::ImageAnnotationStatus::kAnnotationProcessFailed:
        return GetLocalizedRoleDescriptionForUnlabeledImage();

      case ax::mojom::ImageAnnotationStatus::kAnnotationSucceeded:
      case ax::mojom::ImageAnnotationStatus::kNone:
      case ax::mojom::ImageAnnotationStatus::kWillNotAnnotateDueToScheme:
      case ax::mojom::ImageAnnotationStatus::kIneligibleForAnnotation:
      case ax::mojom::ImageAnnotationStatus::kSilentlyEligibleForAnnotation:
        break;
    }
  }

  // For pop up buttons, we want to return property value specific roles.
  if (GetRole() == ax::mojom::Role::kPopUpButton) {
    switch (static_cast<ax::mojom::HasPopup>(
        GetIntAttribute(ax::mojom::IntAttribute::kHasPopup))) {
      case ax::mojom::HasPopup::kTrue:
      case ax::mojom::HasPopup::kMenu:
        return content_client->GetLocalizedString(
            IDS_AX_ROLE_POP_UP_BUTTON_MENU);
      case ax::mojom::HasPopup::kDialog:
        return content_client->GetLocalizedString(
            IDS_AX_ROLE_POP_UP_BUTTON_DIALOG);
      case ax::mojom::HasPopup::kListbox:
      case ax::mojom::HasPopup::kTree:
      case ax::mojom::HasPopup::kGrid:
        return content_client->GetLocalizedString(IDS_AX_ROLE_POP_UP_BUTTON);
      case ax::mojom::HasPopup::kFalse:
        break;
    }
  }

  int message_id = -1;
  switch (GetRole()) {
    case ax::mojom::Role::kAbbr:
      // No role description.
      break;
    case ax::mojom::Role::kAlertDialog:
      message_id = IDS_AX_ROLE_ALERT_DIALOG;
      break;
    case ax::mojom::Role::kAlert:
      message_id = IDS_AX_ROLE_ALERT;
      break;
    case ax::mojom::Role::kApplication:
      message_id = IDS_AX_ROLE_APPLICATION;
      break;
    case ax::mojom::Role::kArticle:
      message_id = IDS_AX_ROLE_ARTICLE;
      break;
    case ax::mojom::Role::kAudio:
      message_id = IDS_AX_MEDIA_AUDIO_ELEMENT;
      break;
    case ax::mojom::Role::kBanner:
      message_id = IDS_AX_ROLE_BANNER;
      break;
    case ax::mojom::Role::kBlockquote:
      message_id = IDS_AX_ROLE_BLOCKQUOTE;
      break;
    case ax::mojom::Role::kButton:
      message_id = IDS_AX_ROLE_BUTTON;
      break;
    case ax::mojom::Role::kCanvas:
      // No role description.
      break;
    case ax::mojom::Role::kCaption:
      // No role description.
      break;
    case ax::mojom::Role::kCaret:
      // No role description.
      break;
    case ax::mojom::Role::kCell:
      // No role description.
      break;
    case ax::mojom::Role::kCheckBox:
      message_id = IDS_AX_ROLE_CHECK_BOX;
      break;
    case ax::mojom::Role::kClient:
      // No role description.
      break;
    case ax::mojom::Role::kCode:
      // No role description.
      break;
    case ax::mojom::Role::kColorWell:
      message_id = IDS_AX_ROLE_COLOR_WELL;
      break;
    case ax::mojom::Role::kColumnHeader:
      message_id = IDS_AX_ROLE_COLUMN_HEADER;
      break;
    case ax::mojom::Role::kColumn:
      // No role description.
      break;
    case ax::mojom::Role::kComboBoxGrouping:
      // No role descripotion.
      break;
    case ax::mojom::Role::kComboBoxMenuButton:
      // No role descripotion.
      break;
    case ax::mojom::Role::kComment:
      message_id = IDS_AX_ROLE_COMMENT;
      break;
    case ax::mojom::Role::kComplementary:
      message_id = IDS_AX_ROLE_COMPLEMENTARY;
      break;
    case ax::mojom::Role::kContentDeletion:
      message_id = IDS_AX_ROLE_CONTENT_DELETION;
      break;
    case ax::mojom::Role::kContentInsertion:
      message_id = IDS_AX_ROLE_CONTENT_INSERTION;
      break;
    case ax::mojom::Role::kContentInfo:
      message_id = IDS_AX_ROLE_CONTENT_INFO;
      break;
    case ax::mojom::Role::kDate:
      message_id = IDS_AX_ROLE_DATE;
      break;
    case ax::mojom::Role::kDateTime: {
      std::string type;
      if (node()->GetStringAttribute(ax::mojom::StringAttribute::kInputType,
                                     &type)) {
        // Returns a specific role to better aid users on the control type
        // they are interacting with. This differs from Android text input type
        // which has a more granular mapping that determines type of keyboard
        // to display.
        if (type == "datetime-local") {
          message_id = IDS_AX_ROLE_DATE_TIME_LOCAL;
        } else if (type == "month") {
          message_id = IDS_AX_ROLE_MONTH;
        } else if (type == "week") {
          message_id = IDS_AX_ROLE_WEEK;
        } else {
          message_id = IDS_AX_ROLE_DATE_TIME;
        }
      } else {
        message_id = IDS_AX_ROLE_DATE_TIME;
      }
      break;
    }
    case ax::mojom::Role::kDefinition:
      message_id = IDS_AX_ROLE_DEFINITION;
      break;
    case ax::mojom::Role::kDescriptionListDetail:
      message_id = IDS_AX_ROLE_DEFINITION;
      break;
    case ax::mojom::Role::kDescriptionList:
      // No role description.
      break;
    case ax::mojom::Role::kDescriptionListTerm:
      // No role description.
      break;
    case ax::mojom::Role::kDesktop:
      // No role description.
      break;
    case ax::mojom::Role::kDetails:
      // No role description.
      break;
    case ax::mojom::Role::kDialog:
      message_id = IDS_AX_ROLE_DIALOG;
      break;
    case ax::mojom::Role::kDirectory:
      message_id = IDS_AX_ROLE_DIRECTORY;
      break;
    case ax::mojom::Role::kDisclosureTriangle:
      message_id = IDS_AX_ROLE_DISCLOSURE_TRIANGLE;
      break;
    case ax::mojom::Role::kDocAbstract:
      message_id = IDS_AX_ROLE_DOC_ABSTRACT;
      break;
    case ax::mojom::Role::kDocAcknowledgments:
      message_id = IDS_AX_ROLE_DOC_ACKNOWLEDGMENTS;
      break;
    case ax::mojom::Role::kDocAfterword:
      message_id = IDS_AX_ROLE_DOC_AFTERWORD;
      break;
    case ax::mojom::Role::kDocAppendix:
      message_id = IDS_AX_ROLE_DOC_APPENDIX;
      break;
    case ax::mojom::Role::kDocBackLink:
      message_id = IDS_AX_ROLE_DOC_BACKLINK;
      break;
    case ax::mojom::Role::kDocBiblioEntry:
      message_id = IDS_AX_ROLE_DOC_BIBLIO_ENTRY;
      break;
    case ax::mojom::Role::kDocBibliography:
      message_id = IDS_AX_ROLE_DOC_BIBLIOGRAPHY;
      break;
    case ax::mojom::Role::kDocBiblioRef:
      message_id = IDS_AX_ROLE_DOC_BIBLIO_REF;
      break;
    case ax::mojom::Role::kDocChapter:
      message_id = IDS_AX_ROLE_DOC_CHAPTER;
      break;
    case ax::mojom::Role::kDocColophon:
      message_id = IDS_AX_ROLE_DOC_COLOPHON;
      break;
    case ax::mojom::Role::kDocConclusion:
      message_id = IDS_AX_ROLE_DOC_CONCLUSION;
      break;
    case ax::mojom::Role::kDocCover:
      message_id = IDS_AX_ROLE_DOC_COVER;
      break;
    case ax::mojom::Role::kDocCredit:
      message_id = IDS_AX_ROLE_DOC_CREDIT;
      break;
    case ax::mojom::Role::kDocCredits:
      message_id = IDS_AX_ROLE_DOC_CREDITS;
      break;
    case ax::mojom::Role::kDocDedication:
      message_id = IDS_AX_ROLE_DOC_DEDICATION;
      break;
    case ax::mojom::Role::kDocEndnote:
      message_id = IDS_AX_ROLE_DOC_ENDNOTE;
      break;
    case ax::mojom::Role::kDocEndnotes:
      message_id = IDS_AX_ROLE_DOC_ENDNOTES;
      break;
    case ax::mojom::Role::kDocEpigraph:
      message_id = IDS_AX_ROLE_DOC_EPIGRAPH;
      break;
    case ax::mojom::Role::kDocEpilogue:
      message_id = IDS_AX_ROLE_DOC_EPILOGUE;
      break;
    case ax::mojom::Role::kDocErrata:
      message_id = IDS_AX_ROLE_DOC_ERRATA;
      break;
    case ax::mojom::Role::kDocExample:
      message_id = IDS_AX_ROLE_DOC_EXAMPLE;
      break;
    case ax::mojom::Role::kDocFootnote:
      message_id = IDS_AX_ROLE_DOC_FOOTNOTE;
      break;
    case ax::mojom::Role::kDocForeword:
      message_id = IDS_AX_ROLE_DOC_FOREWORD;
      break;
    case ax::mojom::Role::kDocGlossary:
      message_id = IDS_AX_ROLE_DOC_GLOSSARY;
      break;
    case ax::mojom::Role::kDocGlossRef:
      message_id = IDS_AX_ROLE_DOC_GLOSS_REF;
      break;
    case ax::mojom::Role::kDocIndex:
      message_id = IDS_AX_ROLE_DOC_INDEX;
      break;
    case ax::mojom::Role::kDocIntroduction:
      message_id = IDS_AX_ROLE_DOC_INTRODUCTION;
      break;
    case ax::mojom::Role::kDocNoteRef:
      message_id = IDS_AX_ROLE_DOC_NOTE_REF;
      break;
    case ax::mojom::Role::kDocNotice:
      message_id = IDS_AX_ROLE_DOC_NOTICE;
      break;
    case ax::mojom::Role::kDocPageBreak:
      message_id = IDS_AX_ROLE_DOC_PAGE_BREAK;
      break;
    case ax::mojom::Role::kDocPageFooter:
      message_id = IDS_AX_ROLE_DOC_PAGE_FOOTER;
      break;
    case ax::mojom::Role::kDocPageHeader:
      message_id = IDS_AX_ROLE_DOC_PAGE_HEADER;
      break;
    case ax::mojom::Role::kDocPageList:
      message_id = IDS_AX_ROLE_DOC_PAGE_LIST;
      break;
    case ax::mojom::Role::kDocPart:
      message_id = IDS_AX_ROLE_DOC_PART;
      break;
    case ax::mojom::Role::kDocPreface:
      message_id = IDS_AX_ROLE_DOC_PREFACE;
      break;
    case ax::mojom::Role::kDocPrologue:
      message_id = IDS_AX_ROLE_DOC_PROLOGUE;
      break;
    case ax::mojom::Role::kDocPullquote:
      message_id = IDS_AX_ROLE_DOC_PULLQUOTE;
      break;
    case ax::mojom::Role::kDocQna:
      message_id = IDS_AX_ROLE_DOC_QNA;
      break;
    case ax::mojom::Role::kDocSubtitle:
      message_id = IDS_AX_ROLE_DOC_SUBTITLE;
      break;
    case ax::mojom::Role::kDocTip:
      message_id = IDS_AX_ROLE_DOC_TIP;
      break;
    case ax::mojom::Role::kDocToc:
      message_id = IDS_AX_ROLE_DOC_TOC;
      break;
    case ax::mojom::Role::kDocument:
      message_id = IDS_AX_ROLE_DOCUMENT;
      break;
    case ax::mojom::Role::kEmbeddedObject:
      message_id = IDS_AX_ROLE_EMBEDDED_OBJECT;
      break;
    case ax::mojom::Role::kEmphasis:
      // No role description.
      break;
    case ax::mojom::Role::kFeed:
      message_id = IDS_AX_ROLE_FEED;
      break;
    case ax::mojom::Role::kFigcaption:
      // No role description.
      break;
    case ax::mojom::Role::kFigure:
      message_id = IDS_AX_ROLE_GRAPHIC;
      break;
    case ax::mojom::Role::kFooter:
      message_id = IDS_AX_ROLE_FOOTER;
      break;
    case ax::mojom::Role::kFooterAsNonLandmark:
      // No role description.
      break;
    case ax::mojom::Role::kForm:
      // No role description.
      break;
    case ax::mojom::Role::kGenericContainer:
      // No role description.
      break;
    case ax::mojom::Role::kGraphicsDocument:
      message_id = IDS_AX_ROLE_GRAPHICS_DOCUMENT;
      break;
    case ax::mojom::Role::kGraphicsObject:
      message_id = IDS_AX_ROLE_GRAPHICS_OBJECT;
      break;
    case ax::mojom::Role::kGraphicsSymbol:
      message_id = IDS_AX_ROLE_GRAPHICS_SYMBOL;
      break;
    case ax::mojom::Role::kGrid:
      message_id = IDS_AX_ROLE_TABLE;
      break;
    case ax::mojom::Role::kGroup:
      // No role description.
      break;
    case ax::mojom::Role::kHeader:
      message_id = IDS_AX_ROLE_BANNER;
      break;
    case ax::mojom::Role::kHeaderAsNonLandmark:
      // No role description.
      break;
    case ax::mojom::Role::kHeading:
      // Note that code above this switch statement handles headings with
      // a level, returning a string like "heading level 1", etc.
      message_id = IDS_AX_ROLE_HEADING;
      break;
    case ax::mojom::Role::kIframe:
      // No role description.
      break;
    case ax::mojom::Role::kIframePresentational:
      // No role description.
      break;
    case ax::mojom::Role::kImage:
      message_id = IDS_AX_ROLE_GRAPHIC;
      break;
    case ax::mojom::Role::kImeCandidate:
      // No role description.
      break;
    case ax::mojom::Role::kInlineTextBox:
      // No role description.
      break;
    case ax::mojom::Role::kInputTime:
      message_id = IDS_AX_ROLE_INPUT_TIME;
      break;
    case ax::mojom::Role::kKeyboard:
      // No role description.
      break;
    case ax::mojom::Role::kLabelText:
      // No role description.
      break;
    case ax::mojom::Role::kLayoutTable:
    case ax::mojom::Role::kLayoutTableCell:
    case ax::mojom::Role::kLayoutTableRow:
      // No role description.
      break;
    case ax::mojom::Role::kLegend:
      // No role description.
      break;
    case ax::mojom::Role::kLineBreak:
      // No role description.
      break;
    case ax::mojom::Role::kLink:
      message_id = IDS_AX_ROLE_LINK;
      break;
    case ax::mojom::Role::kList:
      // No role description.
      break;
    case ax::mojom::Role::kListBox:
      message_id = IDS_AX_ROLE_LIST_BOX;
      break;
    case ax::mojom::Role::kListBoxOption:
      // No role description.
      break;
    case ax::mojom::Role::kListGrid:
      message_id = IDS_AX_ROLE_TABLE;
      break;
    case ax::mojom::Role::kListItem:
      // No role description.
      break;
    case ax::mojom::Role::kListMarker:
      // No role description.
      break;
    case ax::mojom::Role::kLog:
      message_id = IDS_AX_ROLE_LOG;
      break;
    case ax::mojom::Role::kMain:
      message_id = IDS_AX_ROLE_MAIN_CONTENT;
      break;
    case ax::mojom::Role::kMark:
      message_id = IDS_AX_ROLE_MARK;
      break;
    case ax::mojom::Role::kMarquee:
      message_id = IDS_AX_ROLE_MARQUEE;
      break;
    case ax::mojom::Role::kMath:
      message_id = IDS_AX_ROLE_MATH;
      break;
    case ax::mojom::Role::kMenu:
      message_id = IDS_AX_ROLE_MENU;
      break;
    case ax::mojom::Role::kMenuBar:
      message_id = IDS_AX_ROLE_MENU_BAR;
      break;
    case ax::mojom::Role::kMenuItem:
      message_id = IDS_AX_ROLE_MENU_ITEM;
      break;
    case ax::mojom::Role::kMenuItemCheckBox:
      message_id = IDS_AX_ROLE_CHECK_BOX;
      break;
    case ax::mojom::Role::kMenuItemRadio:
      message_id = IDS_AX_ROLE_RADIO;
      break;
    case ax::mojom::Role::kMenuListOption:
      // No role description.
      break;
    case ax::mojom::Role::kMenuListPopup:
      // No role description.
      break;
    case ax::mojom::Role::kMeter:
      message_id = IDS_AX_ROLE_METER;
      break;
    case ax::mojom::Role::kNavigation:
      message_id = IDS_AX_ROLE_NAVIGATIONAL_LINK;
      break;
    case ax::mojom::Role::kNote:
      message_id = IDS_AX_ROLE_NOTE;
      break;
    case ax::mojom::Role::kPane:
      // No role description.
      break;
    case ax::mojom::Role::kParagraph:
      // No role description.
      break;
    case ax::mojom::Role::kPdfActionableHighlight:
      message_id = IDS_AX_ROLE_PDF_HIGHLIGHT;
      break;
    case ax::mojom::Role::kPdfRoot:
      // No role description.
      break;
    case ax::mojom::Role::kPluginObject:
      message_id = IDS_AX_ROLE_EMBEDDED_OBJECT;
      break;
    case ax::mojom::Role::kPopUpButton:
      // Note that pop up buttons are handled before this switch
      NOTREACHED();
      break;
    case ax::mojom::Role::kPortal:
      message_id = IDS_AX_ROLE_BUTTON;
      break;
    case ax::mojom::Role::kPre:
      // No role description.
      break;
    case ax::mojom::Role::kProgressIndicator:
      message_id = IDS_AX_ROLE_PROGRESS_INDICATOR;
      break;
    case ax::mojom::Role::kRadioButton:
      message_id = IDS_AX_ROLE_RADIO;
      break;
    case ax::mojom::Role::kRadioGroup:
      message_id = IDS_AX_ROLE_RADIO_GROUP;
      break;
    case ax::mojom::Role::kRegion:
      message_id = IDS_AX_ROLE_REGION;
      break;
    case ax::mojom::Role::kRootWebArea:
      // No role description.
      break;
    case ax::mojom::Role::kRow:
      // No role description.
      break;
    case ax::mojom::Role::kRowGroup:
      // No role description.
      break;
    case ax::mojom::Role::kRowHeader:
      message_id = IDS_AX_ROLE_ROW_HEADER;
      break;
    case ax::mojom::Role::kRuby:
      // No role description.
      break;
    case ax::mojom::Role::kRubyAnnotation:
      // No role description.
      break;
    case ax::mojom::Role::kSection:
      // No role description.
      break;
    case ax::mojom::Role::kSvgRoot:
      message_id = IDS_AX_ROLE_GRAPHIC;
      break;
    case ax::mojom::Role::kScrollBar:
      message_id = IDS_AX_ROLE_SCROLL_BAR;
      break;
    case ax::mojom::Role::kScrollView:
      // No role description.
      break;
    case ax::mojom::Role::kSearch:
      message_id = IDS_AX_ROLE_SEARCH;
      break;
    case ax::mojom::Role::kSearchBox:
      message_id = IDS_AX_ROLE_SEARCH_BOX;
      break;
    case ax::mojom::Role::kSlider:
      message_id = IDS_AX_ROLE_SLIDER;
      break;
    case ax::mojom::Role::kSpinButton:
      message_id = IDS_AX_ROLE_SPIN_BUTTON;
      break;
    case ax::mojom::Role::kSplitter:
      message_id = IDS_AX_ROLE_SPLITTER;
      break;
    case ax::mojom::Role::kStaticText:
      // No role description.
      break;
    case ax::mojom::Role::kStatus:
      message_id = IDS_AX_ROLE_STATUS;
      break;
    case ax::mojom::Role::kStrong:
      // No role description.
      break;
    case ax::mojom::Role::kSuggestion:
      message_id = IDS_AX_ROLE_SUGGESTION;
      break;
    case ax::mojom::Role::kSwitch:
      message_id = IDS_AX_ROLE_SWITCH;
      break;
    case ax::mojom::Role::kTabList:
      message_id = IDS_AX_ROLE_TAB_LIST;
      break;
    case ax::mojom::Role::kTabPanel:
      message_id = IDS_AX_ROLE_TAB_PANEL;
      break;
    case ax::mojom::Role::kTab:
      message_id = IDS_AX_ROLE_TAB;
      break;
    case ax::mojom::Role::kTableHeaderContainer:
      // No role description.
      break;
    case ax::mojom::Role::kTable:
      message_id = IDS_AX_ROLE_TABLE;
      break;
    case ax::mojom::Role::kTerm:
      message_id = IDS_AX_ROLE_DESCRIPTION_TERM;
      break;
    case ax::mojom::Role::kTextField:
      // No role description.
      break;
    case ax::mojom::Role::kTextFieldWithComboBox:
      // No role description.
      break;
    case ax::mojom::Role::kTime:
      // No role description.
      break;
    case ax::mojom::Role::kTimer:
      message_id = IDS_AX_ROLE_TIMER;
      break;
    case ax::mojom::Role::kTitleBar:
      // No role description.
      break;
    case ax::mojom::Role::kToggleButton:
      message_id = IDS_AX_ROLE_TOGGLE_BUTTON;
      break;
    case ax::mojom::Role::kToolbar:
      message_id = IDS_AX_ROLE_TOOLBAR;
      break;
    case ax::mojom::Role::kTreeGrid:
      message_id = IDS_AX_ROLE_TREE_GRID;
      break;
    case ax::mojom::Role::kTreeItem:
      message_id = IDS_AX_ROLE_TREE_ITEM;
      break;
    case ax::mojom::Role::kTree:
      message_id = IDS_AX_ROLE_TREE;
      break;
    case ax::mojom::Role::kUnknown:
      // No role description.
      break;
    case ax::mojom::Role::kTooltip:
      message_id = IDS_AX_ROLE_TOOLTIP;
      break;
    case ax::mojom::Role::kVideo:
      message_id = IDS_AX_MEDIA_VIDEO_ELEMENT;
      break;
    case ax::mojom::Role::kWebView:
      // No role description.
      break;
    case ax::mojom::Role::kWindow:
      // No role description.
      break;
    case ax::mojom::Role::kNone:
      // No role description.
      break;
  }

  if (message_id != -1)
    return content_client->GetLocalizedString(message_id);

  return std::u16string();
}

int BrowserAccessibilityAndroid::GetItemIndex() const {
  int index = 0;
  if (GetData().IsRangeValueSupported()) {
    // Return a percentage here for live feedback in an AccessibilityEvent.
    // The exact value is returned in RangeCurrentValue.
    float min = GetFloatAttribute(ax::mojom::FloatAttribute::kMinValueForRange);
    float max = GetFloatAttribute(ax::mojom::FloatAttribute::kMaxValueForRange);
    float value = GetFloatAttribute(ax::mojom::FloatAttribute::kValueForRange);
    if (max > min && value >= min && value <= max)
      index = static_cast<int>(((value - min)) * 100 / (max - min));
  } else {
    absl::optional<int> pos_in_set = node()->GetPosInSet();
    if (pos_in_set && *pos_in_set > 0)
      index = *pos_in_set - 1;
  }
  return index;
}

int BrowserAccessibilityAndroid::GetItemCount() const {
  int count = 0;
  if (GetData().IsRangeValueSupported()) {
    // An AccessibilityEvent can only return integer information about a
    // seek control, so we return a percentage. The real range is returned
    // in RangeMin and RangeMax.
    count = 100;
  } else {
    if (IsCollection() && node()->GetSetSize())
      count = *node()->GetSetSize();
  }
  return count;
}

int BrowserAccessibilityAndroid::GetSelectedItemCount() const {
  // Count the number of selected children.
  int selected_count = 0;
  for (const auto& child : PlatformChildren()) {
    const BrowserAccessibilityAndroid& android_child =
        static_cast<const BrowserAccessibilityAndroid&>(child);
    if (android_child.IsSelected())
      selected_count++;
  }

  return selected_count;
}

bool BrowserAccessibilityAndroid::CanScrollForward() const {
  if (IsSlider()) {
    // If it's not a native INPUT element, then increment and decrement
    // won't work.
    std::string html_tag =
        GetStringAttribute(ax::mojom::StringAttribute::kHtmlTag);
    if (html_tag != "input")
      return false;

    float value = GetFloatAttribute(ax::mojom::FloatAttribute::kValueForRange);
    float max = GetFloatAttribute(ax::mojom::FloatAttribute::kMaxValueForRange);
    return value < max;
  } else {
    return CanScrollRight() || CanScrollDown();
  }
}

bool BrowserAccessibilityAndroid::CanScrollBackward() const {
  if (IsSlider()) {
    // If it's not a native INPUT element, then increment and decrement
    // won't work.
    std::string html_tag =
        GetStringAttribute(ax::mojom::StringAttribute::kHtmlTag);
    if (html_tag != "input")
      return false;

    float value = GetFloatAttribute(ax::mojom::FloatAttribute::kValueForRange);
    float min = GetFloatAttribute(ax::mojom::FloatAttribute::kMinValueForRange);
    return value > min;
  } else {
    return CanScrollLeft() || CanScrollUp();
  }
}

bool BrowserAccessibilityAndroid::CanScrollUp() const {
  return GetScrollY() > GetMinScrollY() && IsScrollable();
}

bool BrowserAccessibilityAndroid::CanScrollDown() const {
  return GetScrollY() < GetMaxScrollY() && IsScrollable();
}

bool BrowserAccessibilityAndroid::CanScrollLeft() const {
  return GetScrollX() > GetMinScrollX() && IsScrollable();
}

bool BrowserAccessibilityAndroid::CanScrollRight() const {
  return GetScrollX() < GetMaxScrollX() && IsScrollable();
}

int BrowserAccessibilityAndroid::GetScrollX() const {
  int value = 0;
  GetIntAttribute(ax::mojom::IntAttribute::kScrollX, &value);
  return value;
}

int BrowserAccessibilityAndroid::GetScrollY() const {
  int value = 0;
  GetIntAttribute(ax::mojom::IntAttribute::kScrollY, &value);
  return value;
}

int BrowserAccessibilityAndroid::GetMinScrollX() const {
  return GetIntAttribute(ax::mojom::IntAttribute::kScrollXMin);
}

int BrowserAccessibilityAndroid::GetMinScrollY() const {
  return GetIntAttribute(ax::mojom::IntAttribute::kScrollYMin);
}

int BrowserAccessibilityAndroid::GetMaxScrollX() const {
  return GetIntAttribute(ax::mojom::IntAttribute::kScrollXMax);
}

int BrowserAccessibilityAndroid::GetMaxScrollY() const {
  return GetIntAttribute(ax::mojom::IntAttribute::kScrollYMax);
}

bool BrowserAccessibilityAndroid::Scroll(int direction,
                                         bool is_page_scroll) const {
  int x_initial = GetIntAttribute(ax::mojom::IntAttribute::kScrollX);
  int x_min = GetIntAttribute(ax::mojom::IntAttribute::kScrollXMin);
  int x_max = GetIntAttribute(ax::mojom::IntAttribute::kScrollXMax);
  int y_initial = GetIntAttribute(ax::mojom::IntAttribute::kScrollY);
  int y_min = GetIntAttribute(ax::mojom::IntAttribute::kScrollYMin);
  int y_max = GetIntAttribute(ax::mojom::IntAttribute::kScrollYMax);

  // Figure out the bounding box of the visible portion of this scrollable
  // view so we know how much to scroll by.
  gfx::Rect bounds;
  if (ui::IsPlatformDocument(GetRole()) && !PlatformGetParent()) {
    // If this is the root node, use the bounds of the view to determine how big
    // one page is.
    if (!manager()->delegate())
      return false;
    bounds = manager()->delegate()->AccessibilityGetViewBounds();
  } else if (ui::IsPlatformDocument(GetRole()) && PlatformGetParent()) {
    // If this is a web area inside of an iframe, try to use the bounds of
    // the containing element.
    BrowserAccessibility* parent = PlatformGetParent();
    while (parent && (parent->GetClippedRootFrameBoundsRect().width() == 0 ||
                      parent->GetClippedRootFrameBoundsRect().height() == 0)) {
      parent = parent->PlatformGetParent();
    }
    if (parent)
      bounds = parent->GetClippedRootFrameBoundsRect();
    else
      bounds = GetClippedRootFrameBoundsRect();
  } else {
    // Otherwise this is something like a scrollable div, just use the
    // bounds of this object itself.
    bounds = GetClippedRootFrameBoundsRect();
  }

  // Scroll by 80% of one page, or 100% for page scrolls.
  int page_x, page_y;
  if (is_page_scroll) {
    page_x = std::max(bounds.width(), 1);
    page_y = std::max(bounds.height(), 1);
  } else {
    page_x = std::max(bounds.width() * 4 / 5, 1);
    page_y = std::max(bounds.height() * 4 / 5, 1);
  }

  if (direction == FORWARD)
    direction = y_max > y_min ? DOWN : RIGHT;
  if (direction == BACKWARD)
    direction = y_max > y_min ? UP : LEFT;

  int x = x_initial;
  int y = y_initial;
  switch (direction) {
    case UP:
      if (y_initial == y_min)
        return false;
      y = base::clamp(y_initial - page_y, y_min, y_max);
      break;
    case DOWN:
      if (y_initial == y_max)
        return false;
      y = base::clamp(y_initial + page_y, y_min, y_max);
      break;
    case LEFT:
      if (x_initial == x_min)
        return false;
      x = base::clamp(x_initial - page_x, x_min, x_max);
      break;
    case RIGHT:
      if (x_initial == x_max)
        return false;
      x = base::clamp(x_initial + page_x, x_min, x_max);
      break;
    default:
      NOTREACHED();
  }

  manager()->SetScrollOffset(*this, gfx::Point(x, y));
  return true;
}

// Given arbitrary old_value_ and new_value_, we must come up with reasonable
// edit metrics. Although edits like "apple" > "apples" are typical, anything
// is possible, such as "apple" > "applesauce", "apple" > "boot", or "" >
// "supercalifragilisticexpialidocious". So we consider old_value_ to be of the
// form AXB and new_value_ to be of the form AYB, where X and Y are the pieces
// that don't match. We take the X to be the "removed" characters and Y to be
// the "added" characters.

int BrowserAccessibilityAndroid::GetTextChangeFromIndex() const {
  // This is len(A)
  return CommonPrefixLength(old_value_, new_value_);
}

int BrowserAccessibilityAndroid::GetTextChangeAddedCount() const {
  // This is len(AYB) - (len(A) + len(B)), or len(Y), the added characters.
  return new_value_.length() - CommonEndLengths(old_value_, new_value_);
}

int BrowserAccessibilityAndroid::GetTextChangeRemovedCount() const {
  // This is len(AXB) - (len(A) + len(B)), or len(X), the removed characters.
  return old_value_.length() - CommonEndLengths(old_value_, new_value_);
}

// static
size_t BrowserAccessibilityAndroid::CommonPrefixLength(const std::u16string a,
                                                       const std::u16string b) {
  size_t a_len = a.length();
  size_t b_len = b.length();
  size_t i = 0;
  while (i < a_len && i < b_len && a[i] == b[i]) {
    i++;
  }
  return i;
}

// static
size_t BrowserAccessibilityAndroid::CommonSuffixLength(const std::u16string a,
                                                       const std::u16string b) {
  size_t a_len = a.length();
  size_t b_len = b.length();
  size_t i = 0;
  while (i < a_len && i < b_len && a[a_len - i - 1] == b[b_len - i - 1]) {
    i++;
  }
  return i;
}

// TODO(nektar): Merge this function with
// |BrowserAccessibilityCocoa::computeTextEdit|.
//
// static
size_t BrowserAccessibilityAndroid::CommonEndLengths(const std::u16string a,
                                                     const std::u16string b) {
  size_t prefix_len = CommonPrefixLength(a, b);
  // Remove the matching prefix before finding the suffix. Otherwise, if
  // old_value_ is "a" and new_value_ is "aa", "a" will be double-counted as
  // both a prefix and a suffix of "aa".
  std::u16string a_body = a.substr(prefix_len, std::string::npos);
  std::u16string b_body = b.substr(prefix_len, std::string::npos);
  size_t suffix_len = CommonSuffixLength(a_body, b_body);
  return prefix_len + suffix_len;
}

std::u16string BrowserAccessibilityAndroid::GetTextChangeBeforeText() const {
  return old_value_;
}

int BrowserAccessibilityAndroid::GetSelectionStart() const {
  int sel_start = 0;
  if (IsAtomicTextField() &&
      GetIntAttribute(ax::mojom::IntAttribute::kTextSelStart, &sel_start)) {
    return sel_start;
  }
  ui::AXTree::Selection unignored_selection =
      manager()->ax_tree()->GetUnignoredSelection();
  int32_t anchor_id = unignored_selection.anchor_object_id;
  BrowserAccessibility* anchor_object = manager()->GetFromID(anchor_id);
  if (!anchor_object) {
    return 0;
  }

  AXPosition position = anchor_object->CreateTextPositionAt(
      unignored_selection.anchor_offset, unignored_selection.anchor_affinity);
  while (position->GetAnchor() && position->GetAnchor() != node())
    position = position->CreateParentPosition();

  return !position->IsNullPosition() ? position->text_offset() : 0;
}

int BrowserAccessibilityAndroid::GetSelectionEnd() const {
  int sel_end = 0;
  if (IsAtomicTextField() &&
      GetIntAttribute(ax::mojom::IntAttribute::kTextSelEnd, &sel_end)) {
    return sel_end;
  }

  ui::AXTree::Selection unignored_selection =
      manager()->ax_tree()->GetUnignoredSelection();
  int32_t focus_id = unignored_selection.focus_object_id;
  BrowserAccessibility* focus_object = manager()->GetFromID(focus_id);
  if (!focus_object)
    return 0;

  AXPosition position = focus_object->CreateTextPositionAt(
      unignored_selection.focus_offset, unignored_selection.focus_affinity);
  while (position->GetAnchor() && position->GetAnchor() != node())
    position = position->CreateParentPosition();

  return !position->IsNullPosition() ? position->text_offset() : 0;
}

int BrowserAccessibilityAndroid::GetEditableTextLength() const {
  if (IsTextField())
    return static_cast<int>(GetValueForControl().size());
  return 0;
}

int BrowserAccessibilityAndroid::AndroidInputType() const {
  std::string html_tag =
      GetStringAttribute(ax::mojom::StringAttribute::kHtmlTag);
  if (html_tag != "input")
    return ANDROID_TEXT_INPUTTYPE_TYPE_NULL;

  std::string type;
  if (!node()->GetStringAttribute(ax::mojom::StringAttribute::kInputType,
                                  &type))
    return ANDROID_TEXT_INPUTTYPE_TYPE_TEXT;

  if (type.empty() || type == "text" || type == "search")
    return ANDROID_TEXT_INPUTTYPE_TYPE_TEXT;
  else if (type == "date")
    return ANDROID_TEXT_INPUTTYPE_TYPE_DATETIME_DATE;
  else if (type == "datetime" || type == "datetime-local")
    return ANDROID_TEXT_INPUTTYPE_TYPE_DATETIME;
  else if (type == "email")
    return ANDROID_TEXT_INPUTTYPE_TYPE_TEXT_WEB_EMAIL;
  else if (type == "month")
    return ANDROID_TEXT_INPUTTYPE_TYPE_DATETIME_DATE;
  else if (type == "number")
    return ANDROID_TEXT_INPUTTYPE_TYPE_NUMBER;
  else if (type == "password")
    return ANDROID_TEXT_INPUTTYPE_TYPE_TEXT_WEB_PASSWORD;
  else if (type == "tel")
    return ANDROID_TEXT_INPUTTYPE_TYPE_PHONE;
  else if (type == "time")
    return ANDROID_TEXT_INPUTTYPE_TYPE_DATETIME_TIME;
  else if (type == "url")
    return ANDROID_TEXT_INPUTTYPE_TYPE_TEXT_URI;
  else if (type == "week")
    return ANDROID_TEXT_INPUTTYPE_TYPE_DATETIME;

  return ANDROID_TEXT_INPUTTYPE_TYPE_NULL;
}

int BrowserAccessibilityAndroid::AndroidLiveRegionType() const {
  std::string live =
      GetStringAttribute(ax::mojom::StringAttribute::kLiveStatus);
  if (live == "polite")
    return ANDROID_VIEW_VIEW_ACCESSIBILITY_LIVE_REGION_POLITE;
  else if (live == "assertive")
    return ANDROID_VIEW_VIEW_ACCESSIBILITY_LIVE_REGION_ASSERTIVE;
  return ANDROID_VIEW_VIEW_ACCESSIBILITY_LIVE_REGION_NONE;
}

int BrowserAccessibilityAndroid::AndroidRangeType() const {
  return ANDROID_VIEW_ACCESSIBILITY_RANGE_TYPE_FLOAT;
}

int BrowserAccessibilityAndroid::RowCount() const {
  if (!IsCollection())
    return 0;

  if (node()->GetSetSize())
    return *node()->GetSetSize();

  return node()->GetTableRowCount().value_or(0);
}

int BrowserAccessibilityAndroid::ColumnCount() const {
  if (IsCollection())
    return node()->GetTableColCount().value_or(0);
  return 0;
}

int BrowserAccessibilityAndroid::RowIndex() const {
  absl::optional<int> pos_in_set = node()->GetPosInSet();
  if (pos_in_set && pos_in_set > 0)
    return *pos_in_set - 1;
  return node()->GetTableCellRowIndex().value_or(0);
}

int BrowserAccessibilityAndroid::RowSpan() const {
  return node()->GetTableCellRowSpan().value_or(0);
}

int BrowserAccessibilityAndroid::ColumnIndex() const {
  return node()->GetTableCellColIndex().value_or(0);
}

int BrowserAccessibilityAndroid::ColumnSpan() const {
  return node()->GetTableCellColSpan().value_or(0);
}

float BrowserAccessibilityAndroid::RangeMin() const {
  return GetFloatAttribute(ax::mojom::FloatAttribute::kMinValueForRange);
}

float BrowserAccessibilityAndroid::RangeMax() const {
  return GetFloatAttribute(ax::mojom::FloatAttribute::kMaxValueForRange);
}

float BrowserAccessibilityAndroid::RangeCurrentValue() const {
  return GetFloatAttribute(ax::mojom::FloatAttribute::kValueForRange);
}

void BrowserAccessibilityAndroid::GetGranularityBoundaries(
    int granularity,
    std::vector<int32_t>* starts,
    std::vector<int32_t>* ends,
    int offset) {
  switch (granularity) {
    case ANDROID_ACCESSIBILITY_NODE_INFO_MOVEMENT_GRANULARITY_LINE:
      GetLineBoundaries(starts, ends, offset);
      break;
    case ANDROID_ACCESSIBILITY_NODE_INFO_MOVEMENT_GRANULARITY_WORD:
      GetWordBoundaries(starts, ends, offset);
      break;
    default:
      NOTREACHED();
  }
}

void BrowserAccessibilityAndroid::GetLineBoundaries(
    std::vector<int32_t>* line_starts,
    std::vector<int32_t>* line_ends,
    int offset) {
  // If this node has no children, treat it as all one line.
  if (GetInnerText().size() > 0 && !InternalChildCount()) {
    line_starts->push_back(offset);
    line_ends->push_back(offset + GetInnerText().size());
  }

  // If this is a static text node, get the line boundaries from the
  // inline text boxes if possible.
  if (GetRole() == ax::mojom::Role::kStaticText) {
    int last_y = 0;
    bool is_first = true;
    for (auto it = InternalChildrenBegin(); it != InternalChildrenEnd(); ++it) {
      BrowserAccessibilityAndroid* child =
          static_cast<BrowserAccessibilityAndroid*>(it.get());
      CHECK_EQ(ax::mojom::Role::kInlineTextBox, child->GetRole());
      // TODO(dmazzoni): replace this with a proper API to determine
      // if two inline text boxes are on the same line. http://crbug.com/421771
      int y = child->GetClippedRootFrameBoundsRect().y();
      if (is_first) {
        is_first = false;
        line_starts->push_back(offset);
      } else if (y != last_y) {
        line_ends->push_back(offset);
        line_starts->push_back(offset);
      }
      offset += child->GetInnerText().size();
      last_y = y;
    }
    line_ends->push_back(offset);
    return;
  }

  // Otherwise, call GetLineBoundaries recursively on the children.
  for (auto it = InternalChildrenBegin(); it != InternalChildrenEnd(); ++it) {
    BrowserAccessibilityAndroid* child =
        static_cast<BrowserAccessibilityAndroid*>(it.get());
    child->GetLineBoundaries(line_starts, line_ends, offset);
    offset += child->GetInnerText().size();
  }
}

void BrowserAccessibilityAndroid::GetWordBoundaries(
    std::vector<int32_t>* word_starts,
    std::vector<int32_t>* word_ends,
    int offset) {
  if (GetRole() == ax::mojom::Role::kInlineTextBox) {
    const std::vector<int32_t>& starts =
        GetIntListAttribute(ax::mojom::IntListAttribute::kWordStarts);
    const std::vector<int32_t>& ends =
        GetIntListAttribute(ax::mojom::IntListAttribute::kWordEnds);
    for (size_t i = 0; i < starts.size(); ++i) {
      word_starts->push_back(offset + starts[i]);
      word_ends->push_back(offset + ends[i]);
    }
    return;
  }

  std::u16string concatenated_text;
  for (auto it = InternalChildrenBegin(); it != InternalChildrenEnd(); ++it) {
    BrowserAccessibilityAndroid* child =
        static_cast<BrowserAccessibilityAndroid*>(it.get());
    std::u16string child_text = child->GetInnerText();
    concatenated_text += child->GetInnerText();
  }

  std::u16string text = GetInnerText();
  if (text.empty() || concatenated_text == text) {
    // Great - this node is just the concatenation of its children, so
    // we can get the word boundaries recursively.
    for (auto it = InternalChildrenBegin(); it != InternalChildrenEnd(); ++it) {
      BrowserAccessibilityAndroid* child =
          static_cast<BrowserAccessibilityAndroid*>(it.get());
      child->GetWordBoundaries(word_starts, word_ends, offset);
      offset += child->GetInnerText().size();
    }
  } else {
    // This node has its own accessible text that doesn't match its
    // visible text - like alt text for an image or something with an
    // aria-label, so split the text into words locally.
    base::i18n::BreakIterator iter(text, base::i18n::BreakIterator::BREAK_WORD);
    if (!iter.Init())
      return;
    while (iter.Advance()) {
      if (iter.IsWord()) {
        word_starts->push_back(iter.prev());
        word_ends->push_back(iter.pos());
      }
    }
  }
}

std::u16string BrowserAccessibilityAndroid::GetTargetUrl() const {
  if (ui::IsImageOrVideo(GetRole()) || ui::IsLink(GetRole()))
    return GetString16Attribute(ax::mojom::StringAttribute::kUrl);

  return {};
}

void BrowserAccessibilityAndroid::GetSuggestions(
    std::vector<int>* suggestion_starts,
    std::vector<int>* suggestion_ends) const {
  DCHECK(suggestion_starts);
  DCHECK(suggestion_ends);

  if (!IsTextField())
    return;

  // TODO(accessibility): using FindTextOnlyObjectsInRange or NextInTreeOrder
  // doesn't work because Android's IsLeaf
  // implementation deliberately excludes a lot of nodes. We need a version of
  // FindTextOnlyObjectsInRange and/or NextInTreeOrder that only walk
  // the internal tree.
  BrowserAccessibility* node = InternalGetFirstChild();
  int start_offset = 0;
  while (node && node != this) {
    if (node->IsText()) {
      const std::vector<int32_t>& marker_types =
          node->GetIntListAttribute(ax::mojom::IntListAttribute::kMarkerTypes);
      if (!marker_types.empty()) {
        const std::vector<int>& marker_starts = node->GetIntListAttribute(
            ax::mojom::IntListAttribute::kMarkerStarts);
        const std::vector<int>& marker_ends =
            node->GetIntListAttribute(ax::mojom::IntListAttribute::kMarkerEnds);

        for (size_t i = 0; i < marker_types.size(); ++i) {
          // On Android, both spelling errors and alternatives from dictation
          // are both encoded as suggestions.
          if (!(marker_types[i] &
                static_cast<int32_t>(ax::mojom::MarkerType::kSuggestion))) {
            continue;
          }

          int suggestion_start = start_offset + marker_starts[i];
          int suggestion_end = start_offset + marker_ends[i];
          suggestion_starts->push_back(suggestion_start);
          suggestion_ends->push_back(suggestion_end);
        }
      }
      start_offset += node->GetInnerText().length();
    }

    // Implementation of NextInTreeOrder, but walking the internal tree.
    if (node->InternalChildCount()) {
      node = node->InternalGetFirstChild();
    } else {
      while (node && node != this) {
        BrowserAccessibility* sibling = node->InternalGetNextSibling();
        if (sibling) {
          node = sibling;
          break;
        }

        node = node->InternalGetParent();
      }
    }
  }
}

bool BrowserAccessibilityAndroid::HasAriaCurrent() const {
  if (!HasIntAttribute(ax::mojom::IntAttribute::kAriaCurrentState))
    return false;

  auto current = static_cast<ax::mojom::AriaCurrentState>(
      GetIntAttribute(ax::mojom::IntAttribute::kAriaCurrentState));

  return current != ax::mojom::AriaCurrentState::kNone &&
         current != ax::mojom::AriaCurrentState::kFalse;
}

bool BrowserAccessibilityAndroid::HasNonEmptyValue() const {
  return IsTextField() && !GetValueForControl().empty();
}

bool BrowserAccessibilityAndroid::HasCharacterLocations() const {
  if (GetRole() == ax::mojom::Role::kStaticText)
    return true;

  for (auto it = InternalChildrenBegin(); it != InternalChildrenEnd(); ++it) {
    if (static_cast<BrowserAccessibilityAndroid*>(it.get())
            ->HasCharacterLocations())
      return true;
  }
  return false;
}

bool BrowserAccessibilityAndroid::HasImage() const {
  if (ui::IsImageOrVideo(GetRole()))
    return true;

  for (auto it = InternalChildrenBegin(); it != InternalChildrenEnd(); ++it) {
    if (static_cast<BrowserAccessibilityAndroid*>(it.get())->HasImage())
      return true;
  }
  return false;
}

BrowserAccessibility*
BrowserAccessibilityAndroid::PlatformGetLowestPlatformAncestor() const {
  BrowserAccessibility* current_object =
      const_cast<BrowserAccessibilityAndroid*>(this);
  BrowserAccessibility* lowest_unignored_node = current_object;
  if (lowest_unignored_node->IsIgnored())
    lowest_unignored_node = lowest_unignored_node->PlatformGetParent();
  DCHECK(!lowest_unignored_node || !lowest_unignored_node->IsIgnored())
      << "`BrowserAccessibility::PlatformGetParent()` should return either an "
         "unignored object or nullptr.";

  // `highest_leaf_node` could be nullptr.
  BrowserAccessibility* highest_leaf_node = lowest_unignored_node;
  // For the purposes of this method, a leaf node does not include leaves in the
  // internal accessibility tree, only in the platform exposed tree.
  for (BrowserAccessibility* ancestor_node = lowest_unignored_node;
       ancestor_node; ancestor_node = ancestor_node->PlatformGetParent()) {
    if (ancestor_node->IsLeaf())
      highest_leaf_node = ancestor_node;
  }
  if (highest_leaf_node)
    return highest_leaf_node;

  if (lowest_unignored_node)
    return lowest_unignored_node;
  return current_object;
}

bool BrowserAccessibilityAndroid::HasOnlyTextChildren() const {
  // This is called from IsLeaf, so don't call PlatformChildCount
  // from within this!
  for (auto it = InternalChildrenBegin(); it != InternalChildrenEnd(); ++it) {
    if (!it->IsText())
      return false;
  }
  return true;
}

bool BrowserAccessibilityAndroid::HasOnlyTextAndImageChildren() const {
  // This is called from IsLeaf, so don't call PlatformChildCount
  // from within this!
  for (auto it = InternalChildrenBegin(); it != InternalChildrenEnd(); ++it) {
    BrowserAccessibility* child = it.get();
    if (!child->IsText() && !ui::IsImageOrVideo(child->GetRole())) {
      return false;
    }
  }
  return true;
}

bool BrowserAccessibilityAndroid::ShouldExposeValueAsName() const {
  switch (GetRole()) {
    case ax::mojom::Role::kDate:
    case ax::mojom::Role::kDateTime:
      return true;
    case ax::mojom::Role::kColorWell:
      return false;
    default:
      break;
  }

  if (GetData().IsRangeValueSupported())
    return false;

  if (IsTextField())
    return true;

  if (GetRole() == ax::mojom::Role::kPopUpButton &&
      !GetValueForControl().empty()) {
    return true;
  }

  return false;
}

void BrowserAccessibilityAndroid::OnDataChanged() {
  BrowserAccessibility::OnDataChanged();

  if (IsTextField()) {
    std::u16string value = GetValueForControl();
    if (value != new_value_) {
      old_value_ = new_value_;
      new_value_ = value;
    }
  }

  auto* manager =
      static_cast<BrowserAccessibilityManagerAndroid*>(this->manager());
  manager->ClearNodeInfoCacheForGivenId(unique_id());

  // For any nodes that are the children of a leaf, we also want to invalidate
  // the cache for the ancestry chain up until the first non-leaf node.
  if (IsChildOfLeaf()) {
    BrowserAccessibilityAndroid* parent =
        static_cast<BrowserAccessibilityAndroid*>(PlatformGetParent());

    while (parent != nullptr && (parent->IsChildOfLeaf() || parent->IsLeaf())) {
      manager->ClearNodeInfoCacheForGivenId(parent->unique_id());
      parent = static_cast<BrowserAccessibilityAndroid*>(
          parent->PlatformGetParent());
    }
  }
}

int BrowserAccessibilityAndroid::CountChildrenWithRole(
    ax::mojom::Role role) const {
  int count = 0;
  for (const auto& child : PlatformChildren()) {
    if (child.GetRole() == role)
      count++;
  }
  return count;
}

std::u16string BrowserAccessibilityAndroid::GetContentInvalidErrorMessage()
    const {
  content::ContentClient* content_client = content::GetContentClient();
  int message_id = -1;

  if (!IsContentInvalid())
    return std::u16string();

  switch (GetData().GetInvalidState()) {
    case ax::mojom::InvalidState::kNone:
    case ax::mojom::InvalidState::kFalse:
      // No error message necessary
      break;

    case ax::mojom::InvalidState::kTrue:
      message_id = CONTENT_INVALID_TRUE;
      break;
    case ax::mojom::InvalidState::kOther:
      std::string ariaInvalid = GetData().GetStringAttribute(
          ax::mojom::StringAttribute::kAriaInvalidValue);
      if (ariaInvalid == "spelling")
        message_id = CONTENT_INVALID_SPELLING;
      else if (ariaInvalid == "grammar")
        message_id = CONTENT_INVALID_GRAMMAR;
      else
        message_id = CONTENT_INVALID_TRUE;
      break;
  }

  if (message_id != -1)
    return content_client->GetLocalizedString(message_id);

  return std::u16string();
}

}  // namespace content
