/*
 * Copyright (C) 2008, 2009, 2011 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "modules/accessibility/AXObject.h"

#include "SkMatrix44.h"
#include "core/InputTypeNames.h"
#include "core/css/resolver/StyleResolver.h"
#include "core/dom/AccessibleNode.h"
#include "core/dom/UserGestureIndicator.h"
#include "core/editing/EditingUtilities.h"
#include "core/editing/VisibleUnits.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/LocalFrameView.h"
#include "core/frame/Settings.h"
#include "core/html/HTMLDialogElement.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/parser/HTMLParserIdioms.h"
#include "core/layout/LayoutBoxModelObject.h"
#include "modules/accessibility/AXObjectCacheImpl.h"
#include "platform/text/PlatformLocale.h"
#include "platform/wtf/HashSet.h"
#include "platform/wtf/StdLibExtras.h"
#include "platform/wtf/text/WTFString.h"

using blink::WebLocalizedString;

namespace blink {

using namespace HTMLNames;

namespace {

struct AccessibilityRoleHashTraits : HashTraits<AccessibilityRole> {
  static const bool kEmptyValueIsZero = true;
  static AccessibilityRole EmptyValue() {
    return AccessibilityRole::kUnknownRole;
  }
};

using ARIARoleMap = HashMap<String,
                            AccessibilityRole,
                            CaseFoldingHash,
                            HashTraits<String>,
                            AccessibilityRoleHashTraits>;

struct RoleEntry {
  const char* aria_role;
  AccessibilityRole webcore_role;
};

const RoleEntry kRoles[] = {{"alert", kAlertRole},
                            {"alertdialog", kAlertDialogRole},
                            {"application", kApplicationRole},
                            {"article", kArticleRole},
                            {"banner", kBannerRole},
                            {"button", kButtonRole},
                            {"cell", kCellRole},
                            {"checkbox", kCheckBoxRole},
                            {"columnheader", kColumnHeaderRole},
                            {"combobox", kComboBoxRole},
                            {"complementary", kComplementaryRole},
                            {"contentinfo", kContentInfoRole},
                            {"definition", kDefinitionRole},
                            {"dialog", kDialogRole},
                            {"directory", kDirectoryRole},
                            {"document", kDocumentRole},
                            {"feed", kFeedRole},
                            {"figure", kFigureRole},
                            {"form", kFormRole},
                            {"grid", kGridRole},
                            {"gridcell", kCellRole},
                            {"group", kGroupRole},
                            {"heading", kHeadingRole},
                            {"img", kImageRole},
                            {"link", kLinkRole},
                            {"list", kListRole},
                            {"listbox", kListBoxRole},
                            {"listitem", kListItemRole},
                            {"log", kLogRole},
                            {"main", kMainRole},
                            {"marquee", kMarqueeRole},
                            {"math", kMathRole},
                            {"menu", kMenuRole},
                            {"menubar", kMenuBarRole},
                            {"menuitem", kMenuItemRole},
                            {"menuitemcheckbox", kMenuItemCheckBoxRole},
                            {"menuitemradio", kMenuItemRadioRole},
                            {"navigation", kNavigationRole},
                            {"none", kNoneRole},
                            {"note", kNoteRole},
                            {"option", kListBoxOptionRole},
                            {"presentation", kPresentationalRole},
                            {"progressbar", kProgressIndicatorRole},
                            {"radio", kRadioButtonRole},
                            {"radiogroup", kRadioGroupRole},
                            {"region", kRegionRole},
                            {"row", kRowRole},
                            {"rowheader", kRowHeaderRole},
                            {"scrollbar", kScrollBarRole},
                            {"search", kSearchRole},
                            {"searchbox", kSearchBoxRole},
                            {"separator", kSplitterRole},
                            {"slider", kSliderRole},
                            {"spinbutton", kSpinButtonRole},
                            {"status", kStatusRole},
                            {"switch", kSwitchRole},
                            {"tab", kTabRole},
                            {"table", kTableRole},
                            {"tablist", kTabListRole},
                            {"tabpanel", kTabPanelRole},
                            {"term", kTermRole},
                            {"text", kStaticTextRole},
                            {"textbox", kTextFieldRole},
                            {"timer", kTimerRole},
                            {"toolbar", kToolbarRole},
                            {"tooltip", kUserInterfaceTooltipRole},
                            {"tree", kTreeRole},
                            {"treegrid", kTreeGridRole},
                            {"treeitem", kTreeItemRole}};

struct InternalRoleEntry {
  AccessibilityRole webcore_role;
  const char* internal_role_name;
};

const InternalRoleEntry kInternalRoles[] = {
    {kUnknownRole, "Unknown"},
    {kAbbrRole, "Abbr"},
    {kAlertDialogRole, "AlertDialog"},
    {kAlertRole, "Alert"},
    {kAnchorRole, "Anchor"},
    {kAnnotationRole, "Annotation"},
    {kApplicationRole, "Application"},
    {kArticleRole, "Article"},
    {kAudioRole, "Audio"},
    {kBannerRole, "Banner"},
    {kBlockquoteRole, "Blockquote"},
    // TODO(nektar): Delete busy_indicator role. It's used nowhere.
    {kBusyIndicatorRole, "BusyIndicator"},
    {kButtonRole, "Button"},
    {kCanvasRole, "Canvas"},
    {kCaptionRole, "Caption"},
    {kCellRole, "Cell"},
    {kCheckBoxRole, "CheckBox"},
    {kColorWellRole, "ColorWell"},
    {kColumnHeaderRole, "ColumnHeader"},
    {kColumnRole, "Column"},
    {kComboBoxRole, "ComboBox"},
    {kComplementaryRole, "Complementary"},
    {kContentInfoRole, "ContentInfo"},
    {kDateRole, "Date"},
    {kDateTimeRole, "DateTime"},
    {kDefinitionRole, "Definition"},
    {kDescriptionListDetailRole, "DescriptionListDetail"},
    {kDescriptionListRole, "DescriptionList"},
    {kDescriptionListTermRole, "DescriptionListTerm"},
    {kDetailsRole, "Details"},
    {kDialogRole, "Dialog"},
    {kDirectoryRole, "Directory"},
    {kDisclosureTriangleRole, "DisclosureTriangle"},
    {kDocumentRole, "Document"},
    {kEmbeddedObjectRole, "EmbeddedObject"},
    {kFeedRole, "feed"},
    {kFigcaptionRole, "Figcaption"},
    {kFigureRole, "Figure"},
    {kFooterRole, "Footer"},
    {kFormRole, "Form"},
    {kGenericContainerRole, "GenericContainer"},
    {kGridRole, "Grid"},
    {kGroupRole, "Group"},
    {kHeadingRole, "Heading"},
    {kIframePresentationalRole, "IframePresentational"},
    {kIframeRole, "Iframe"},
    {kIgnoredRole, "Ignored"},
    {kImageMapLinkRole, "ImageMapLink"},
    {kImageMapRole, "ImageMap"},
    {kImageRole, "Image"},
    {kInlineTextBoxRole, "InlineTextBox"},
    {kInputTimeRole, "InputTime"},
    {kLabelRole, "Label"},
    {kLegendRole, "Legend"},
    {kLinkRole, "Link"},
    {kLineBreakRole, "LineBreak"},
    {kListBoxOptionRole, "ListBoxOption"},
    {kListBoxRole, "ListBox"},
    {kListItemRole, "ListItem"},
    {kListMarkerRole, "ListMarker"},
    {kListRole, "List"},
    {kLogRole, "Log"},
    {kMainRole, "Main"},
    {kMarkRole, "Mark"},
    {kMarqueeRole, "Marquee"},
    {kMathRole, "Math"},
    {kMenuBarRole, "MenuBar"},
    {kMenuButtonRole, "MenuButton"},
    {kMenuItemRole, "MenuItem"},
    {kMenuItemCheckBoxRole, "MenuItemCheckBox"},
    {kMenuItemRadioRole, "MenuItemRadio"},
    {kMenuListOptionRole, "MenuListOption"},
    {kMenuListPopupRole, "MenuListPopup"},
    {kMenuRole, "Menu"},
    {kMeterRole, "Meter"},
    {kNavigationRole, "Navigation"},
    {kNoneRole, "None"},
    {kNoteRole, "Note"},
    {kOutlineRole, "Outline"},
    {kParagraphRole, "Paragraph"},
    {kPopUpButtonRole, "PopUpButton"},
    {kPreRole, "Pre"},
    {kPresentationalRole, "Presentational"},
    {kProgressIndicatorRole, "ProgressIndicator"},
    {kRadioButtonRole, "RadioButton"},
    {kRadioGroupRole, "RadioGroup"},
    {kRegionRole, "Region"},
    {kRootWebAreaRole, "RootWebArea"},
    {kRowHeaderRole, "RowHeader"},
    {kRowRole, "Row"},
    {kRubyRole, "Ruby"},
    {kRulerRole, "Ruler"},
    {kSVGRootRole, "SVGRoot"},
    {kScrollAreaRole, "ScrollArea"},
    {kScrollBarRole, "ScrollBar"},
    {kSeamlessWebAreaRole, "SeamlessWebArea"},
    {kSearchRole, "Search"},
    {kSearchBoxRole, "SearchBox"},
    {kSliderRole, "Slider"},
    {kSliderThumbRole, "SliderThumb"},
    {kSpinButtonPartRole, "SpinButtonPart"},
    {kSpinButtonRole, "SpinButton"},
    {kSplitterRole, "Splitter"},
    {kStaticTextRole, "StaticText"},
    {kStatusRole, "Status"},
    {kSwitchRole, "Switch"},
    {kTabGroupRole, "TabGroup"},
    {kTabListRole, "TabList"},
    {kTabPanelRole, "TabPanel"},
    {kTabRole, "Tab"},
    {kTableHeaderContainerRole, "TableHeaderContainer"},
    {kTableRole, "Table"},
    {kTermRole, "Term"},
    {kTextFieldRole, "TextField"},
    {kTimeRole, "Time"},
    {kTimerRole, "Timer"},
    {kToggleButtonRole, "ToggleButton"},
    {kToolbarRole, "Toolbar"},
    {kTreeGridRole, "TreeGrid"},
    {kTreeItemRole, "TreeItem"},
    {kTreeRole, "Tree"},
    {kUserInterfaceTooltipRole, "UserInterfaceTooltip"},
    {kVideoRole, "Video"},
    {kWebAreaRole, "WebArea"},
    {kWindowRole, "Window"}};

static_assert(WTF_ARRAY_LENGTH(kInternalRoles) == kNumRoles,
              "Not all internal roles have an entry in internalRoles array");

// Roles which we need to map in the other direction
const RoleEntry kReverseRoles[] = {
    {"button", kToggleButtonRole},     {"combobox", kPopUpButtonRole},
    {"contentinfo", kFooterRole},      {"menuitem", kMenuButtonRole},
    {"menuitem", kMenuListOptionRole}, {"progressbar", kMeterRole},
    {"textbox", kTextFieldRole}};

static ARIARoleMap* CreateARIARoleMap() {
  ARIARoleMap* role_map = new ARIARoleMap;

  for (size_t i = 0; i < WTF_ARRAY_LENGTH(kRoles); ++i)
    role_map->Set(String(kRoles[i].aria_role), kRoles[i].webcore_role);

  // Grids "ignore" their non-row children during computation of children.
  role_map->Set("rowgroup", kIgnoredRole);

  return role_map;
}

static Vector<AtomicString>* CreateRoleNameVector() {
  Vector<AtomicString>* role_name_vector = new Vector<AtomicString>(kNumRoles);
  for (int i = 0; i < kNumRoles; i++)
    (*role_name_vector)[i] = g_null_atom;

  for (size_t i = 0; i < WTF_ARRAY_LENGTH(kRoles); ++i) {
    (*role_name_vector)[kRoles[i].webcore_role] =
        AtomicString(kRoles[i].aria_role);
  }

  for (size_t i = 0; i < WTF_ARRAY_LENGTH(kReverseRoles); ++i) {
    (*role_name_vector)[kReverseRoles[i].webcore_role] =
        AtomicString(kReverseRoles[i].aria_role);
  }

  return role_name_vector;
}

static Vector<AtomicString>* CreateInternalRoleNameVector() {
  Vector<AtomicString>* internal_role_name_vector =
      new Vector<AtomicString>(kNumRoles);
  for (size_t i = 0; i < WTF_ARRAY_LENGTH(kInternalRoles); i++) {
    (*internal_role_name_vector)[kInternalRoles[i].webcore_role] =
        AtomicString(kInternalRoles[i].internal_role_name);
  }

  return internal_role_name_vector;
}

HTMLDialogElement* GetActiveDialogElement(Node* node) {
  return node->GetDocument().ActiveModalDialog();
}

}  // namespace

unsigned AXObject::number_of_live_ax_objects_ = 0;

AXObject::AXObject(AXObjectCacheImpl& ax_object_cache)
    : id_(0),
      have_children_(false),
      role_(kUnknownRole),
      last_known_is_ignored_value_(kDefaultBehavior),
      explicit_container_id_(0),
      parent_(nullptr),
      last_modification_count_(-1),
      cached_is_ignored_(false),
      cached_is_inert_or_aria_hidden_(false),
      cached_is_descendant_of_leaf_node_(false),
      cached_is_descendant_of_disabled_node_(false),
      cached_has_inherited_presentational_role_(false),
      cached_is_presentational_child_(false),
      cached_ancestor_exposes_active_descendant_(false),
      cached_live_region_root_(nullptr),
      ax_object_cache_(&ax_object_cache) {
  ++number_of_live_ax_objects_;
}

AXObject::~AXObject() {
  DCHECK(IsDetached());
  --number_of_live_ax_objects_;
}

void AXObject::Detach() {
  // Clear any children and call detachFromParent on them so that
  // no children are left with dangling pointers to their parent.
  ClearChildren();

  ax_object_cache_ = nullptr;
}

bool AXObject::IsDetached() const {
  return !ax_object_cache_;
}

const AtomicString& AXObject::GetAOMPropertyOrARIAAttribute(
    AOMStringProperty property) const {
  if (Element* element = this->GetElement())
    return AccessibleNode::GetPropertyOrARIAAttribute(element, property);
  return g_null_atom;
}

Element* AXObject::GetAOMPropertyOrARIAAttribute(
    AOMRelationProperty property) const {
  Element* element = this->GetElement();
  if (!element)
    return nullptr;

  AccessibleNode* target =
      AccessibleNode::GetPropertyOrARIAAttribute(element, property);
  return target ? target->element() : nullptr;
}

bool AXObject::HasAOMProperty(AOMRelationListProperty property,
                              HeapVector<Member<Element>>& result) const {
  Element* element = this->GetElement();
  if (!element)
    return false;

  return AccessibleNode::GetProperty(element, property, result);
}

bool AXObject::HasAOMPropertyOrARIAAttribute(
    AOMRelationListProperty property,
    HeapVector<Member<Element>>& result) const {
  Element* element = this->GetElement();
  if (!element)
    return false;

  return AccessibleNode::GetPropertyOrARIAAttribute(element, property, result);
}

bool AXObject::HasAOMPropertyOrARIAAttribute(AOMBooleanProperty property,
                                             bool& result) const {
  Element* element = this->GetElement();
  if (!element)
    return false;

  bool is_null = true;
  result =
      AccessibleNode::GetPropertyOrARIAAttribute(element, property, is_null);
  return !is_null;
}

bool AXObject::AOMPropertyOrARIAAttributeIsTrue(
    AOMBooleanProperty property) const {
  bool result;
  if (HasAOMPropertyOrARIAAttribute(property, result))
    return result;
  return false;
}

bool AXObject::AOMPropertyOrARIAAttributeIsFalse(
    AOMBooleanProperty property) const {
  bool result;
  if (HasAOMPropertyOrARIAAttribute(property, result))
    return !result;
  return false;
}

bool AXObject::HasAOMPropertyOrARIAAttribute(AOMUIntProperty property,
                                             uint32_t& result) const {
  Element* element = this->GetElement();
  if (!element)
    return false;

  bool is_null = true;
  result =
      AccessibleNode::GetPropertyOrARIAAttribute(element, property, is_null);
  return !is_null;
}

bool AXObject::HasAOMPropertyOrARIAAttribute(AOMIntProperty property,
                                             int32_t& result) const {
  Element* element = this->GetElement();
  if (!element)
    return false;

  bool is_null = true;
  result =
      AccessibleNode::GetPropertyOrARIAAttribute(element, property, is_null);
  return !is_null;
}

bool AXObject::HasAOMPropertyOrARIAAttribute(AOMFloatProperty property,
                                             float& result) const {
  Element* element = this->GetElement();
  if (!element)
    return false;

  bool is_null = true;
  result =
      AccessibleNode::GetPropertyOrARIAAttribute(element, property, is_null);
  return !is_null;
}

bool AXObject::IsARIATextControl() const {
  return AriaRoleAttribute() == kTextFieldRole ||
         AriaRoleAttribute() == kSearchBoxRole ||
         AriaRoleAttribute() == kComboBoxRole;
}

bool AXObject::IsButton() const {
  AccessibilityRole role = RoleValue();

  return role == kButtonRole || role == kPopUpButtonRole ||
         role == kToggleButtonRole;
}

bool AXObject::IsCheckable() const {
  switch (RoleValue()) {
    case kCheckBoxRole:
    case kMenuItemCheckBoxRole:
    case kMenuItemRadioRole:
    case kRadioButtonRole:
    case kSwitchRole:
    case kToggleButtonRole:
      return true;
    case kTreeItemRole:
    case kListBoxOptionRole:
    case kMenuListOptionRole:
      return AriaCheckedIsPresent();
    default:
      return false;
  }
}

// Why this is here instead of AXNodeObject:
// Because an AXMenuListOption (<option>) can
// have an ARIA role of menuitemcheckbox/menuitemradio
// yet does not inherit from AXNodeObject
AccessibilityCheckedState AXObject::CheckedState() const {
  if (!IsCheckable())
    return kCheckedStateUndefined;

  // Try ARIA checked/pressed state
  const AccessibilityRole role = RoleValue();
  const auto prop = role == kToggleButtonRole ? AOMStringProperty::kPressed
                                              : AOMStringProperty::kChecked;
  const AtomicString& checked_attribute = GetAOMPropertyOrARIAAttribute(prop);
  if (checked_attribute) {
    if (EqualIgnoringASCIICase(checked_attribute, "true"))
      return kCheckedStateTrue;

    if (EqualIgnoringASCIICase(checked_attribute, "mixed")) {
      // Only checkable role that doesn't support mixed is the switch.
      if (role != kSwitchRole)
        return kCheckedStateMixed;
    }

    if (EqualIgnoringASCIICase(checked_attribute, "false"))
      return kCheckedStateFalse;
  }

  // Native checked state
  if (role != kToggleButtonRole) {
    const Node* node = this->GetNode();
    if (!node)
      return kCheckedStateUndefined;

    // Expose native checkbox mixed state as accessibility mixed state. However,
    // do not expose native radio mixed state as accessibility mixed state.
    // This would confuse the JAWS screen reader, which reports a mixed radio as
    // both checked and partially checked, but a native mixed native radio
    // button sinply means no radio buttons have been checked in the group yet.
    if (IsNativeCheckboxInMixedState(node))
      return kCheckedStateMixed;

    if (isHTMLInputElement(*node) &&
        toHTMLInputElement(*node).ShouldAppearChecked()) {
      return kCheckedStateTrue;
    }
  }

  return kCheckedStateFalse;
}

bool AXObject::IsNativeCheckboxInMixedState(const Node* node) {
  if (!isHTMLInputElement(node))
    return false;

  const HTMLInputElement* input = toHTMLInputElement(node);
  const auto inputType = input->type();
  if (inputType != InputTypeNames::checkbox)
    return false;
  return input->ShouldAppearIndeterminate();
}

bool AXObject::IsLandmarkRelated() const {
  switch (RoleValue()) {
    case kApplicationRole:
    case kArticleRole:
    case kBannerRole:
    case kComplementaryRole:
    case kContentInfoRole:
    case kFooterRole:
    case kFormRole:
    case kMainRole:
    case kNavigationRole:
    case kRegionRole:
    case kSearchRole:
      return true;
    default:
      return false;
  }
}

bool AXObject::IsMenuRelated() const {
  switch (RoleValue()) {
    case kMenuRole:
    case kMenuBarRole:
    case kMenuButtonRole:
    case kMenuItemRole:
    case kMenuItemCheckBoxRole:
    case kMenuItemRadioRole:
      return true;
    default:
      return false;
  }
}

bool AXObject::IsPasswordFieldAndShouldHideValue() const {
  Settings* settings = GetDocument()->GetSettings();
  if (!settings || settings->GetAccessibilityPasswordValuesEnabled())
    return false;

  return IsPasswordField();
}

bool AXObject::IsClickable() const {
  switch (RoleValue()) {
    case kButtonRole:
    case kCheckBoxRole:
    case kColorWellRole:
    case kComboBoxRole:
    case kImageMapLinkRole:
    case kLinkRole:
    case kListBoxOptionRole:
    case kMenuButtonRole:
    case kPopUpButtonRole:
    case kRadioButtonRole:
    case kSpinButtonRole:
    case kTabRole:
    case kTextFieldRole:
    case kToggleButtonRole:
      return true;
    default:
      return false;
  }
}

bool AXObject::AccessibilityIsIgnored() {
  Node* node = GetNode();
  if (!node) {
    AXObject* parent = this->ParentObject();
    while (!node && parent) {
      node = parent->GetNode();
      parent = parent->ParentObject();
    }
  }

  if (node)
    node->UpdateDistribution();

  // TODO(aboxhall): Instead of this, propagate inert down through frames
  Document* document = GetDocument();
  while (document && document->LocalOwner()) {
    document->LocalOwner()->UpdateDistribution();
    document = document->LocalOwner()->ownerDocument();
  }

  UpdateCachedAttributeValuesIfNeeded();
  return cached_is_ignored_;
}

void AXObject::UpdateCachedAttributeValuesIfNeeded() const {
  if (IsDetached())
    return;

  AXObjectCacheImpl& cache = AxObjectCache();

  if (cache.ModificationCount() == last_modification_count_)
    return;

  last_modification_count_ = cache.ModificationCount();
  cached_background_color_ = ComputeBackgroundColor();
  cached_is_inert_or_aria_hidden_ = ComputeIsInertOrAriaHidden();
  cached_is_descendant_of_leaf_node_ = (LeafNodeAncestor() != 0);
  cached_is_descendant_of_disabled_node_ = (DisabledAncestor() != 0);
  cached_has_inherited_presentational_role_ =
      (InheritsPresentationalRoleFrom() != 0);
  cached_is_presentational_child_ =
      (AncestorForWhichThisIsAPresentationalChild() != 0);
  cached_is_ignored_ = ComputeAccessibilityIsIgnored();
  cached_live_region_root_ =
      IsLiveRegion()
          ? const_cast<AXObject*>(this)
          : (ParentObjectIfExists() ? ParentObjectIfExists()->LiveRegionRoot()
                                    : 0);
  cached_ancestor_exposes_active_descendant_ =
      ComputeAncestorExposesActiveDescendant();
}

bool AXObject::AccessibilityIsIgnoredByDefault(
    IgnoredReasons* ignored_reasons) const {
  return DefaultObjectInclusion(ignored_reasons) == kIgnoreObject;
}

AXObjectInclusion AXObject::AccessibilityPlatformIncludesObject() const {
  if (IsMenuListPopup() || IsMenuListOption())
    return kIncludeObject;

  return kDefaultBehavior;
}

AXObjectInclusion AXObject::DefaultObjectInclusion(
    IgnoredReasons* ignored_reasons) const {
  if (IsInertOrAriaHidden()) {
    if (ignored_reasons)
      ComputeIsInertOrAriaHidden(ignored_reasons);
    return kIgnoreObject;
  }

  if (IsPresentationalChild()) {
    if (ignored_reasons) {
      AXObject* ancestor = AncestorForWhichThisIsAPresentationalChild();
      ignored_reasons->push_back(
          IgnoredReason(kAXAncestorDisallowsChild, ancestor));
    }
    return kIgnoreObject;
  }

  return AccessibilityPlatformIncludesObject();
}

bool AXObject::IsInertOrAriaHidden() const {
  UpdateCachedAttributeValuesIfNeeded();
  return cached_is_inert_or_aria_hidden_;
}

bool AXObject::ComputeIsInertOrAriaHidden(
    IgnoredReasons* ignored_reasons) const {
  if (GetNode()) {
    if (GetNode()->IsInert()) {
      if (ignored_reasons) {
        HTMLDialogElement* dialog = GetActiveDialogElement(GetNode());
        if (dialog) {
          AXObject* dialog_object = AxObjectCache().GetOrCreate(dialog);
          if (dialog_object) {
            ignored_reasons->push_back(
                IgnoredReason(kAXActiveModalDialog, dialog_object));
          } else {
            ignored_reasons->push_back(IgnoredReason(kAXInertElement));
          }
        } else {
          const AXObject* inert_root_el = InertRoot();
          if (inert_root_el == this) {
            ignored_reasons->push_back(IgnoredReason(kAXInertElement));
          } else {
            ignored_reasons->push_back(
                IgnoredReason(kAXInertSubtree, inert_root_el));
          }
        }
      }
      return true;
    }
  } else {
    AXObject* parent = ParentObject();
    if (parent && parent->IsInertOrAriaHidden()) {
      if (ignored_reasons)
        parent->ComputeIsInertOrAriaHidden(ignored_reasons);
      return true;
    }
  }

  const AXObject* hidden_root = AriaHiddenRoot();
  if (hidden_root) {
    if (ignored_reasons) {
      if (hidden_root == this) {
        ignored_reasons->push_back(IgnoredReason(kAXAriaHiddenElement));
      } else {
        ignored_reasons->push_back(
            IgnoredReason(kAXAriaHiddenSubtree, hidden_root));
      }
    }
    return true;
  }

  return false;
}

bool AXObject::IsDescendantOfLeafNode() const {
  UpdateCachedAttributeValuesIfNeeded();
  return cached_is_descendant_of_leaf_node_;
}

AXObject* AXObject::LeafNodeAncestor() const {
  if (AXObject* parent = ParentObject()) {
    if (!parent->CanHaveChildren())
      return parent;

    return parent->LeafNodeAncestor();
  }

  return 0;
}

const AXObject* AXObject::AriaHiddenRoot() const {
  for (const AXObject* object = this; object; object = object->ParentObject()) {
    if (object->AOMPropertyOrARIAAttributeIsTrue(AOMBooleanProperty::kHidden))
      return object;
  }

  return 0;
}

const AXObject* AXObject::InertRoot() const {
  const AXObject* object = this;
  if (!RuntimeEnabledFeatures::InertAttributeEnabled())
    return 0;

  while (object && !object->IsAXNodeObject())
    object = object->ParentObject();
  Node* node = object->GetNode();
  Element* element = node->IsElementNode()
                         ? ToElement(node)
                         : FlatTreeTraversal::ParentElement(*node);
  while (element) {
    if (element->hasAttribute(inertAttr))
      return AxObjectCache().GetOrCreate(element);
    element = FlatTreeTraversal::ParentElement(*element);
  }

  return 0;
}

bool AXObject::IsDescendantOfDisabledNode() const {
  UpdateCachedAttributeValuesIfNeeded();
  return cached_is_descendant_of_disabled_node_;
}

const AXObject* AXObject::DisabledAncestor() const {
  bool disabled = false;
  if (HasAOMPropertyOrARIAAttribute(AOMBooleanProperty::kDisabled, disabled)) {
    if (disabled)
      return this;
    return nullptr;
  }

  if (AXObject* parent = ParentObject())
    return parent->DisabledAncestor();

  return nullptr;
}

bool AXObject::LastKnownIsIgnoredValue() {
  if (last_known_is_ignored_value_ == kDefaultBehavior) {
    last_known_is_ignored_value_ =
        AccessibilityIsIgnored() ? kIgnoreObject : kIncludeObject;
  }

  return last_known_is_ignored_value_ == kIgnoreObject;
}

void AXObject::SetLastKnownIsIgnoredValue(bool is_ignored) {
  last_known_is_ignored_value_ = is_ignored ? kIgnoreObject : kIncludeObject;
}

bool AXObject::HasInheritedPresentationalRole() const {
  UpdateCachedAttributeValuesIfNeeded();
  return cached_has_inherited_presentational_role_;
}

bool AXObject::IsPresentationalChild() const {
  UpdateCachedAttributeValuesIfNeeded();
  return cached_is_presentational_child_;
}

bool AXObject::CanReceiveAccessibilityFocus() const {
  const Element* elem = GetElement();
  if (!elem)
    return false;

  // Focusable, and not forwarding the focus somewhere else
  if (elem->IsFocusable() &&
      !GetAOMPropertyOrARIAAttribute(AOMRelationProperty::kActiveDescendant))
    return true;

  // aria-activedescendant focus
  return elem->FastHasAttribute(idAttr) && AncestorExposesActiveDescendant();
}

bool AXObject::CanSetValueAttribute() const {
  switch (RoleValue()) {
    case kColorWellRole:
    case kComboBoxRole:
    case kDateRole:
    case kDateTimeRole:
    case kListBoxRole:
    case kMenuButtonRole:
    case kScrollBarRole:
    case kSliderRole:
    case kSpinButtonRole:
    case kSplitterRole:
    case kTextFieldRole:
    case kTimeRole:
    case kSearchBoxRole:
      return Restriction() == kNone;
    default:
      break;
  }
  return false;
}

bool AXObject::CanSetFocusAttribute() const {
  Node* node = GetNode();
  if (!node)
    return false;

  if (IsWebArea())
    return true;

  // Children of elements with an aria-activedescendant attribute should be
  // focusable if they have a (non-presentational) ARIA role.
  if (!IsPresentational() && AriaRoleAttribute() != kUnknownRole &&
      AncestorExposesActiveDescendant())
    return true;

  // NOTE: It would be more accurate to ask the document whether
  // setFocusedNode() would do anything. For example, setFocusedNode() will do
  // nothing if the current focused node will not relinquish the focus.
  if (IsDisabledFormControl(node))
    return false;

  // Check for options here because AXListBoxOption and AXMenuListOption
  // don't help when the <option> is canvas fallback, and because
  // a common case for aria-owns from a textbox that points to a list
  // does not change the hierarchy (textboxes don't support children)
  if (RoleValue() == kListBoxOptionRole || RoleValue() == kMenuListOptionRole)
    return true;

  return node->IsElementNode() && ToElement(node)->SupportsFocus();
}

bool AXObject::AncestorExposesActiveDescendant() const {
  UpdateCachedAttributeValuesIfNeeded();
  return cached_ancestor_exposes_active_descendant_;
}

bool AXObject::ComputeAncestorExposesActiveDescendant() const {
  const AXObject* parent = ParentObjectUnignored();
  if (!parent)
    return false;

  if (parent->SupportsActiveDescendant() &&
      parent->GetAOMPropertyOrARIAAttribute(
          AOMRelationProperty::kActiveDescendant)) {
    return true;
  }

  return parent->AncestorExposesActiveDescendant();
}

bool AXObject::CanSetSelectedAttribute() const {
  // Sub-widget elements can be selected if not disabled (native or ARIA)
  return IsSubWidget(RoleValue()) && Restriction() != kDisabled;
}

bool AXObject::IsSubWidget(AccessibilityRole role) {
  switch (role) {
    case kCellRole:
    case kColumnHeaderRole:
    case kColumnRole:
    case kListBoxOptionRole:
    case kMenuListOptionRole:
    case kRowHeaderRole:
    case kRowRole:
    case kTabRole:
    case kTreeItemRole:
      return true;
    default:
      break;
  }
  return false;
}

// Simplify whitespace, but preserve a single leading and trailing whitespace
// character if it's present.
// static
String AXObject::CollapseWhitespace(const String& str) {
  StringBuilder result;
  if (!str.IsEmpty() && IsHTMLSpace<UChar>(str[0]))
    result.Append(' ');
  result.Append(str.SimplifyWhiteSpace(IsHTMLSpace<UChar>));
  if (!str.IsEmpty() && IsHTMLSpace<UChar>(str[str.length() - 1]))
    result.Append(' ');
  return result.ToString();
}

String AXObject::ComputedName() const {
  AXNameFrom name_from;
  AXObject::AXObjectVector name_objects;
  return GetName(name_from, &name_objects);
}

String AXObject::GetName(AXNameFrom& name_from,
                         AXObject::AXObjectVector* name_objects) const {
  HeapHashSet<Member<const AXObject>> visited;
  AXRelatedObjectVector related_objects;
  String text = TextAlternative(false, false, visited, name_from,
                                &related_objects, nullptr);

  AccessibilityRole role = RoleValue();
  if (!GetNode() || (!isHTMLBRElement(GetNode()) && role != kStaticTextRole &&
                     role != kInlineTextBoxRole))
    text = CollapseWhitespace(text);

  if (name_objects) {
    name_objects->clear();
    for (size_t i = 0; i < related_objects.size(); i++)
      name_objects->push_back(related_objects[i]->object);
  }

  return text;
}

String AXObject::GetName(NameSources* name_sources) const {
  AXObjectSet visited;
  AXNameFrom tmp_name_from;
  AXRelatedObjectVector tmp_related_objects;
  String text = TextAlternative(false, false, visited, tmp_name_from,
                                &tmp_related_objects, name_sources);
  text = text.SimplifyWhiteSpace(IsHTMLSpace<UChar>);
  return text;
}

String AXObject::RecursiveTextAlternative(const AXObject& ax_obj,
                                          bool in_aria_labelled_by_traversal,
                                          AXObjectSet& visited) {
  if (visited.Contains(&ax_obj) && !in_aria_labelled_by_traversal)
    return String();

  AXNameFrom tmp_name_from;
  return ax_obj.TextAlternative(true, in_aria_labelled_by_traversal, visited,
                                tmp_name_from, nullptr, nullptr);
}

bool AXObject::IsHiddenForTextAlternativeCalculation() const {
  if (AOMPropertyOrARIAAttributeIsFalse(AOMBooleanProperty::kHidden))
    return false;

  if (GetLayoutObject())
    return GetLayoutObject()->Style()->Visibility() != EVisibility::kVisible;

  // This is an obscure corner case: if a node has no LayoutObject, that means
  // it's not rendered, but we still may be exploring it as part of a text
  // alternative calculation, for example if it was explicitly referenced by
  // aria-labelledby. So we need to explicitly call the style resolver to check
  // whether it's invisible or display:none, rather than relying on the style
  // cached in the LayoutObject.
  Document* document = GetDocument();
  if (!document || !document->GetFrame())
    return false;
  if (Node* node = GetNode()) {
    if (node->isConnected() && node->IsElementNode()) {
      RefPtr<ComputedStyle> style =
          document->EnsureStyleResolver().StyleForElement(ToElement(node));
      return style->Display() == EDisplay::kNone ||
             style->Visibility() != EVisibility::kVisible;
    }
  }
  return false;
}

String AXObject::AriaTextAlternative(bool recursive,
                                     bool in_aria_labelled_by_traversal,
                                     AXObjectSet& visited,
                                     AXNameFrom& name_from,
                                     AXRelatedObjectVector* related_objects,
                                     NameSources* name_sources,
                                     bool* found_text_alternative) const {
  String text_alternative;
  bool already_visited = visited.Contains(this);
  visited.insert(this);

  // Step 2A from: http://www.w3.org/TR/accname-aam-1.1
  // If you change this logic, update AXNodeObject::nameFromLabelElement, too.
  if (!in_aria_labelled_by_traversal &&
      IsHiddenForTextAlternativeCalculation()) {
    *found_text_alternative = true;
    return String();
  }

  // Step 2B from: http://www.w3.org/TR/accname-aam-1.1
  // If you change this logic, update AXNodeObject::nameFromLabelElement, too.
  if (!in_aria_labelled_by_traversal && !already_visited) {
    name_from = kAXNameFromRelatedElement;

    // Check AOM property first.
    HeapVector<Member<Element>> elements;
    if (HasAOMProperty(AOMRelationListProperty::kLabeledBy, elements)) {
      if (name_sources) {
        name_sources->push_back(
            NameSource(*found_text_alternative, aria_labelledbyAttr));
        name_sources->back().type = name_from;
      }

      // Operate on a copy of |visited| so that if |nameSources| is not null,
      // the set of visited objects is preserved unmodified for future
      // calculations.
      AXObjectSet visited_copy = visited;
      text_alternative =
          TextFromElements(true, visited_copy, elements, related_objects);
      if (!text_alternative.IsNull()) {
        if (name_sources) {
          NameSource& source = name_sources->back();
          source.type = name_from;
          source.related_objects = *related_objects;
          source.text = text_alternative;
          *found_text_alternative = true;
        } else {
          *found_text_alternative = true;
          return text_alternative;
        }
      } else if (name_sources) {
        name_sources->back().invalid = true;
      }
    } else {
      // Now check ARIA attribute
      const QualifiedName& attr =
          HasAttribute(aria_labeledbyAttr) && !HasAttribute(aria_labelledbyAttr)
              ? aria_labeledbyAttr
              : aria_labelledbyAttr;

      if (name_sources) {
        name_sources->push_back(NameSource(*found_text_alternative, attr));
        name_sources->back().type = name_from;
      }

      const AtomicString& aria_labelledby = GetAttribute(attr);
      if (!aria_labelledby.IsNull()) {
        if (name_sources)
          name_sources->back().attribute_value = aria_labelledby;

        // Operate on a copy of |visited| so that if |nameSources| is not null,
        // the set of visited objects is preserved unmodified for future
        // calculations.
        AXObjectSet visited_copy = visited;
        text_alternative =
            TextFromAriaLabelledby(visited_copy, related_objects);
        if (!text_alternative.IsNull()) {
          if (name_sources) {
            NameSource& source = name_sources->back();
            source.type = name_from;
            source.related_objects = *related_objects;
            source.text = text_alternative;
            *found_text_alternative = true;
          } else {
            *found_text_alternative = true;
            return text_alternative;
          }
        } else if (name_sources) {
          name_sources->back().invalid = true;
        }
      }
    }
  }

  // Step 2C from: http://www.w3.org/TR/accname-aam-1.1
  // If you change this logic, update AXNodeObject::nameFromLabelElement, too.
  name_from = kAXNameFromAttribute;
  if (name_sources) {
    name_sources->push_back(
        NameSource(*found_text_alternative, aria_labelAttr));
    name_sources->back().type = name_from;
  }
  const AtomicString& aria_label =
      GetAOMPropertyOrARIAAttribute(AOMStringProperty::kLabel);
  if (!aria_label.IsEmpty()) {
    text_alternative = aria_label;

    if (name_sources) {
      NameSource& source = name_sources->back();
      source.text = text_alternative;
      source.attribute_value = aria_label;
      *found_text_alternative = true;
    } else {
      *found_text_alternative = true;
      return text_alternative;
    }
  }

  return text_alternative;
}

String AXObject::TextFromElements(
    bool in_aria_labelledby_traversal,
    AXObjectSet& visited,
    HeapVector<Member<Element>>& elements,
    AXRelatedObjectVector* related_objects) const {
  StringBuilder accumulated_text;
  bool found_valid_element = false;
  AXRelatedObjectVector local_related_objects;

  for (const auto& element : elements) {
    AXObject* ax_element = AxObjectCache().GetOrCreate(element);
    if (ax_element) {
      found_valid_element = true;

      String result = RecursiveTextAlternative(
          *ax_element, in_aria_labelledby_traversal, visited);
      local_related_objects.push_back(
          new NameSourceRelatedObject(ax_element, result));
      if (!result.IsEmpty()) {
        if (!accumulated_text.IsEmpty())
          accumulated_text.Append(' ');
        accumulated_text.Append(result);
      }
    }
  }
  if (!found_valid_element)
    return String();
  if (related_objects)
    *related_objects = local_related_objects;
  return accumulated_text.ToString();
}

void AXObject::TokenVectorFromAttribute(Vector<String>& tokens,
                                        const QualifiedName& attribute) const {
  Node* node = this->GetNode();
  if (!node || !node->IsElementNode())
    return;

  String attribute_value = GetAttribute(attribute).GetString();
  if (attribute_value.IsEmpty())
    return;

  attribute_value.SimplifyWhiteSpace();
  attribute_value.Split(' ', tokens);
}

void AXObject::ElementsFromAttribute(HeapVector<Member<Element>>& elements,
                                     const QualifiedName& attribute) const {
  Vector<String> ids;
  TokenVectorFromAttribute(ids, attribute);
  if (ids.IsEmpty())
    return;

  TreeScope& scope = GetNode()->GetTreeScope();
  for (const auto& id : ids) {
    if (Element* id_element = scope.getElementById(AtomicString(id)))
      elements.push_back(id_element);
  }
}

void AXObject::AriaLabelledbyElementVector(
    HeapVector<Member<Element>>& elements) const {
  // Try both spellings, but prefer aria-labelledby, which is the official spec.
  ElementsFromAttribute(elements, aria_labelledbyAttr);
  if (!elements.size())
    ElementsFromAttribute(elements, aria_labeledbyAttr);
}

String AXObject::TextFromAriaLabelledby(
    AXObjectSet& visited,
    AXRelatedObjectVector* related_objects) const {
  HeapVector<Member<Element>> elements;
  AriaLabelledbyElementVector(elements);
  return TextFromElements(true, visited, elements, related_objects);
}

String AXObject::TextFromAriaDescribedby(
    AXRelatedObjectVector* related_objects) const {
  AXObjectSet visited;
  HeapVector<Member<Element>> elements;
  ElementsFromAttribute(elements, aria_describedbyAttr);
  return TextFromElements(true, visited, elements, related_objects);
}

RGBA32 AXObject::BackgroundColor() const {
  UpdateCachedAttributeValuesIfNeeded();
  return cached_background_color_;
}

AccessibilityOrientation AXObject::Orientation() const {
  // In ARIA 1.1, the default value for aria-orientation changed from
  // horizontal to undefined.
  return kAccessibilityOrientationUndefined;
}

AXDefaultActionVerb AXObject::Action() const {
  if (!ActionElement())
    return AXDefaultActionVerb::kNone;

  switch (RoleValue()) {
    case kButtonRole:
    case kToggleButtonRole:
      return AXDefaultActionVerb::kPress;
    case kTextFieldRole:
      return AXDefaultActionVerb::kActivate;
    case kMenuItemRadioRole:
    case kRadioButtonRole:
      return AXDefaultActionVerb::kSelect;
    case kLinkRole:
      return AXDefaultActionVerb::kJump;
    case kPopUpButtonRole:
      return AXDefaultActionVerb::kOpen;
    default:
      if (IsCheckable()) {
        return CheckedState() != kCheckedStateTrue
                   ? AXDefaultActionVerb::kCheck
                   : AXDefaultActionVerb::kUncheck;
      }
      return AXDefaultActionVerb::kClick;
  }
}
bool AXObject::AriaPressedIsPresent() const {
  return !GetAttribute(aria_pressedAttr).IsEmpty();
}

bool AXObject::AriaCheckedIsPresent() const {
  return !GetAttribute(aria_checkedAttr).IsEmpty();
}

bool AXObject::SupportsActiveDescendant() const {
  // According to the ARIA Spec, all ARIA composite widgets, ARIA text boxes
  // and ARIA groups should be able to expose an active descendant.
  // Implicitly, <input> and <textarea> elements should also have this ability.
  switch (AriaRoleAttribute()) {
    case kComboBoxRole:
    case kGridRole:
    case kGroupRole:
    case kListBoxRole:
    case kMenuRole:
    case kMenuBarRole:
    case kRadioGroupRole:
    case kRowRole:
    case kSearchBoxRole:
    case kTabListRole:
    case kTextFieldRole:
    case kToolbarRole:
    case kTreeRole:
    case kTreeGridRole:
      return true;
    default:
      return false;
  }
}

bool AXObject::SupportsARIAAttributes() const {
  return IsLiveRegion() || SupportsARIADragging() || SupportsARIADropping() ||
         SupportsARIAFlowTo() || SupportsARIAOwns() ||
         HasAttribute(aria_labelAttr) || HasAttribute(aria_currentAttr);
}

bool AXObject::SupportsRangeValue() const {
  return IsProgressIndicator() || IsMeter() || IsSlider() || IsScrollbar() ||
         IsSpinButton() || IsMoveableSplitter();
}

bool AXObject::SupportsSetSizeAndPosInSet() const {
  AXObject* parent = ParentObjectUnignored();
  if (!parent)
    return false;

  int role = RoleValue();
  int parent_role = parent->RoleValue();

  if ((role == kListBoxOptionRole && parent_role == kListBoxRole) ||
      (role == kListItemRole && parent_role == kListRole) ||
      (role == kMenuItemRole && parent_role == kMenuRole) ||
      (role == kRadioButtonRole) ||
      (role == kTabRole && parent_role == kTabListRole) ||
      (role == kTreeItemRole && parent_role == kTreeRole) ||
      (role == kTreeItemRole && parent_role == kTreeItemRole)) {
    return true;
  }

  return false;
}

int AXObject::IndexInParent() const {
  if (!ParentObject())
    return 0;

  const auto& siblings = ParentObject()->Children();
  int child_count = siblings.size();

  for (int index = 0; index < child_count; ++index) {
    if (siblings[index].Get() == this) {
      return index;
    }
  }
  return 0;
}

bool AXObject::IsLiveRegion() const {
  const AtomicString& live_region = LiveRegionStatus();
  return EqualIgnoringASCIICase(live_region, "polite") ||
         EqualIgnoringASCIICase(live_region, "assertive");
}

AXObject* AXObject::LiveRegionRoot() const {
  UpdateCachedAttributeValuesIfNeeded();
  return cached_live_region_root_;
}

const AtomicString& AXObject::ContainerLiveRegionStatus() const {
  UpdateCachedAttributeValuesIfNeeded();
  return cached_live_region_root_ ? cached_live_region_root_->LiveRegionStatus()
                                  : g_null_atom;
}

const AtomicString& AXObject::ContainerLiveRegionRelevant() const {
  UpdateCachedAttributeValuesIfNeeded();
  return cached_live_region_root_
             ? cached_live_region_root_->LiveRegionRelevant()
             : g_null_atom;
}

bool AXObject::ContainerLiveRegionAtomic() const {
  UpdateCachedAttributeValuesIfNeeded();
  return cached_live_region_root_ &&
         cached_live_region_root_->LiveRegionAtomic();
}

bool AXObject::ContainerLiveRegionBusy() const {
  UpdateCachedAttributeValuesIfNeeded();
  return cached_live_region_root_ && cached_live_region_root_->LiveRegionBusy();
}

AXObject* AXObject::ElementAccessibilityHitTest(const IntPoint& point) const {
  // Check if there are any mock elements that need to be handled.
  for (const auto& child : children_) {
    if (child->IsMockObject() &&
        child->GetBoundsInFrameCoordinates().Contains(point))
      return child->ElementAccessibilityHitTest(point);
  }

  return const_cast<AXObject*>(this);
}

const AXObject::AXObjectVector& AXObject::Children() {
  UpdateChildrenIfNecessary();

  return children_;
}

AXObject* AXObject::ParentObject() const {
  if (IsDetached())
    return 0;

  if (parent_)
    return parent_;

  if (AxObjectCache().IsAriaOwned(this))
    return AxObjectCache().GetAriaOwnedParent(this);

  return ComputeParent();
}

AXObject* AXObject::ParentObjectIfExists() const {
  if (IsDetached())
    return 0;

  if (parent_)
    return parent_;

  return ComputeParentIfExists();
}

AXObject* AXObject::ParentObjectUnignored() const {
  AXObject* parent;
  for (parent = ParentObject(); parent && parent->AccessibilityIsIgnored();
       parent = parent->ParentObject()) {
  }

  return parent;
}

// Container widgets are those that a user tabs into and arrows around
// sub-widgets
bool AXObject::IsContainerWidget() const {
  switch (RoleValue()) {
    case kComboBoxRole:
    case kGridRole:
    case kListBoxRole:
    case kMenuBarRole:
    case kMenuRole:
    case kRadioGroupRole:
    case kSpinButtonRole:
    case kTabListRole:
    case kToolbarRole:
    case kTreeGridRole:
    case kTreeRole:
      return true;
    default:
      return false;
  }
}

AXObject* AXObject::ContainerWidget() const {
  AXObject* ancestor = ParentObjectUnignored();
  while (ancestor && !ancestor->IsContainerWidget())
    ancestor = ancestor->ParentObjectUnignored();

  return ancestor;
}

void AXObject::UpdateChildrenIfNecessary() {
  if (!HasChildren())
    AddChildren();
}

void AXObject::ClearChildren() {
  // Detach all weak pointers from objects to their parents.
  for (const auto& child : children_)
    child->DetachFromParent();

  children_.clear();
  have_children_ = false;
}

Element* AXObject::GetElement() const {
  Node* node = GetNode();
  return node && node->IsElementNode() ? ToElement(node) : nullptr;
}

Document* AXObject::GetDocument() const {
  LocalFrameView* frame_view = DocumentFrameView();
  if (!frame_view)
    return 0;

  return frame_view->GetFrame().GetDocument();
}

LocalFrameView* AXObject::DocumentFrameView() const {
  const AXObject* object = this;
  while (object && !object->IsAXLayoutObject())
    object = object->ParentObject();

  if (!object)
    return 0;

  return object->DocumentFrameView();
}

String AXObject::Language() const {
  const AtomicString& lang = GetAttribute(langAttr);
  if (!lang.IsEmpty())
    return lang;

  AXObject* parent = ParentObject();

  // As a last resort, fall back to the content language specified in the meta
  // tag.
  if (!parent) {
    Document* doc = GetDocument();
    if (doc)
      return doc->ContentLanguage();
    return g_null_atom;
  }

  return parent->Language();
}

bool AXObject::HasAttribute(const QualifiedName& attribute) const {
  if (Element* element = GetElement())
    return element->FastHasAttribute(attribute);
  return false;
}

const AtomicString& AXObject::GetAttribute(
    const QualifiedName& attribute) const {
  if (Element* element = GetElement())
    return element->FastGetAttribute(attribute);
  return g_null_atom;
}

//
// Scrollable containers.
//

bool AXObject::IsScrollableContainer() const {
  return !!GetScrollableAreaIfScrollable();
}

IntPoint AXObject::GetScrollOffset() const {
  ScrollableArea* area = GetScrollableAreaIfScrollable();
  if (!area)
    return IntPoint();

  return IntPoint(area->ScrollOffsetInt().Width(),
                  area->ScrollOffsetInt().Height());
}

IntPoint AXObject::MinimumScrollOffset() const {
  ScrollableArea* area = GetScrollableAreaIfScrollable();
  if (!area)
    return IntPoint();

  return IntPoint(area->MinimumScrollOffsetInt().Width(),
                  area->MinimumScrollOffsetInt().Height());
}

IntPoint AXObject::MaximumScrollOffset() const {
  ScrollableArea* area = GetScrollableAreaIfScrollable();
  if (!area)
    return IntPoint();

  return IntPoint(area->MaximumScrollOffsetInt().Width(),
                  area->MaximumScrollOffsetInt().Height());
}

void AXObject::SetScrollOffset(const IntPoint& offset) const {
  ScrollableArea* area = GetScrollableAreaIfScrollable();
  if (!area)
    return;

  // TODO(bokan): This should potentially be a UserScroll.
  area->SetScrollOffset(ScrollOffset(offset.X(), offset.Y()),
                        kProgrammaticScroll);
}

void AXObject::GetRelativeBounds(AXObject** out_container,
                                 FloatRect& out_bounds_in_container,
                                 SkMatrix44& out_container_transform) const {
  *out_container = nullptr;
  out_bounds_in_container = FloatRect();
  out_container_transform.setIdentity();

  // First check if it has explicit bounds, for example if this element is tied
  // to a canvas path. When explicit coordinates are provided, the ID of the
  // explicit container element that the coordinates are relative to must be
  // provided too.
  if (!explicit_element_rect_.IsEmpty()) {
    *out_container = AxObjectCache().ObjectFromAXID(explicit_container_id_);
    if (*out_container) {
      out_bounds_in_container = FloatRect(explicit_element_rect_);
      return;
    }
  }

  LayoutObject* layout_object = LayoutObjectForRelativeBounds();
  if (!layout_object)
    return;

  if (IsWebArea()) {
    if (layout_object->GetFrame()->View()) {
      out_bounds_in_container.SetSize(
          FloatSize(layout_object->GetFrame()->View()->ContentsSize()));
    }
    return;
  }

  // First compute the container. The container must be an ancestor in the
  // accessibility tree, and its LayoutObject must be an ancestor in the layout
  // tree. Get the first such ancestor that's either scrollable or has a paint
  // layer.
  AXObject* container = ParentObjectUnignored();
  LayoutObject* container_layout_object = nullptr;
  while (container) {
    container_layout_object = container->GetLayoutObject();
    if (container_layout_object && container_layout_object->IsBox() &&
        layout_object->IsDescendantOf(container_layout_object)) {
      if (container->IsScrollableContainer() ||
          container_layout_object->HasLayer())
        break;
    }

    container = container->ParentObjectUnignored();
  }

  if (!container)
    return;
  *out_container = container;
  out_bounds_in_container =
      layout_object->LocalBoundingBoxRectForAccessibility();

  // If the container has a scroll offset, subtract that out because we want our
  // bounds to be relative to the *unscrolled* position of the container object.
  ScrollableArea* scrollable_area = container->GetScrollableAreaIfScrollable();
  if (scrollable_area && !container->IsWebArea()) {
    ScrollOffset scroll_offset = scrollable_area->GetScrollOffset();
    out_bounds_in_container.Move(scroll_offset);
  }

  // Compute the transform between the container's coordinate space and this
  // object.  If the transform is just a simple translation, apply that to the
  // bounding box, but if it's a non-trivial transformation like a rotation,
  // scaling, etc. then return the full matrix instead.
  TransformationMatrix transform = layout_object->LocalToAncestorTransform(
      ToLayoutBoxModelObject(container_layout_object));
  if (transform.IsIdentityOr2DTranslation()) {
    out_bounds_in_container.Move(transform.To2DTranslation());
  } else {
    out_container_transform = TransformationMatrix::ToSkMatrix44(transform);
  }
}

LayoutRect AXObject::GetBoundsInFrameCoordinates() const {
  AXObject* container = nullptr;
  FloatRect bounds;
  SkMatrix44 transform;
  GetRelativeBounds(&container, bounds, transform);
  FloatRect computed_bounds(0, 0, bounds.Width(), bounds.Height());
  while (container && container != this) {
    computed_bounds.Move(bounds.X(), bounds.Y());
    if (!container->IsWebArea()) {
      computed_bounds.Move(-container->GetScrollOffset().X(),
                           -container->GetScrollOffset().Y());
    }
    if (!transform.isIdentity()) {
      TransformationMatrix transformation_matrix(transform);
      transformation_matrix.MapRect(computed_bounds);
    }
    container->GetRelativeBounds(&container, bounds, transform);
  }
  return LayoutRect(computed_bounds);
}

//
// Modify or take an action on an object.
//

bool AXObject::Press() {
  Document* document = GetDocument();
  if (!document)
    return false;

  UserGestureIndicator gesture_indicator(
      UserGestureToken::Create(document, UserGestureToken::kNewGesture));
  Element* action_elem = ActionElement();
  if (action_elem) {
    action_elem->AccessKeyAction(true);
    return true;
  }

  if (CanSetFocusAttribute()) {
    SetFocused(true);
    return true;
  }

  return false;
}

void AXObject::ScrollToMakeVisible() const {
  IntRect object_rect = PixelSnappedIntRect(GetBoundsInFrameCoordinates());
  object_rect.SetLocation(IntPoint());
  ScrollToMakeVisibleWithSubFocus(object_rect);
}

// This is a 1-dimensional scroll offset helper function that's applied
// separately in the horizontal and vertical directions, because the
// logic is the same. The goal is to compute the best scroll offset
// in order to make an object visible within a viewport.
//
// If the object is already fully visible, returns the same scroll
// offset.
//
// In case the whole object cannot fit, you can specify a
// subfocus - a smaller region within the object that should
// be prioritized. If the whole object can fit, the subfocus is
// ignored.
//
// If possible, the object and subfocus are centered within the
// viewport.
//
// Example 1: the object is already visible, so nothing happens.
//   +----------Viewport---------+
//                 +---Object---+
//                 +--SubFocus--+
//
// Example 2: the object is not fully visible, so it's centered
// within the viewport.
//   Before:
//   +----------Viewport---------+
//                         +---Object---+
//                         +--SubFocus--+
//
//   After:
//                 +----------Viewport---------+
//                         +---Object---+
//                         +--SubFocus--+
//
// Example 3: the object is larger than the viewport, so the
// viewport moves to show as much of the object as possible,
// while also trying to center the subfocus.
//   Before:
//   +----------Viewport---------+
//     +---------------Object--------------+
//                         +-SubFocus-+
//
//   After:
//             +----------Viewport---------+
//     +---------------Object--------------+
//                         +-SubFocus-+
//
// When constraints cannot be fully satisfied, the min
// (left/top) position takes precedence over the max (right/bottom).
//
// Note that the return value represents the ideal new scroll offset.
// This may be out of range - the calling function should clip this
// to the available range.
static int ComputeBestScrollOffset(int current_scroll_offset,
                                   int subfocus_min,
                                   int subfocus_max,
                                   int object_min,
                                   int object_max,
                                   int viewport_min,
                                   int viewport_max) {
  int viewport_size = viewport_max - viewport_min;

  // If the object size is larger than the viewport size, consider
  // only a portion that's as large as the viewport, centering on
  // the subfocus as much as possible.
  if (object_max - object_min > viewport_size) {
    // Since it's impossible to fit the whole object in the
    // viewport, exit now if the subfocus is already within the viewport.
    if (subfocus_min - current_scroll_offset >= viewport_min &&
        subfocus_max - current_scroll_offset <= viewport_max)
      return current_scroll_offset;

    // Subfocus must be within focus.
    subfocus_min = std::max(subfocus_min, object_min);
    subfocus_max = std::min(subfocus_max, object_max);

    // Subfocus must be no larger than the viewport size; favor top/left.
    if (subfocus_max - subfocus_min > viewport_size)
      subfocus_max = subfocus_min + viewport_size;

    // Compute the size of an object centered on the subfocus, the size of the
    // viewport.
    int centered_object_min = (subfocus_min + subfocus_max - viewport_size) / 2;
    int centered_object_max = centered_object_min + viewport_size;

    object_min = std::max(object_min, centered_object_min);
    object_max = std::min(object_max, centered_object_max);
  }

  // Exit now if the focus is already within the viewport.
  if (object_min - current_scroll_offset >= viewport_min &&
      object_max - current_scroll_offset <= viewport_max)
    return current_scroll_offset;

  // Center the object in the viewport.
  return (object_min + object_max - viewport_min - viewport_max) / 2;
}

void AXObject::ScrollToMakeVisibleWithSubFocus(const IntRect& subfocus) const {
  // Search up the parent chain until we find the first one that's scrollable.
  const AXObject* scroll_parent = ParentObject() ? ParentObject() : this;
  ScrollableArea* scrollable_area = 0;
  while (scroll_parent) {
    scrollable_area = scroll_parent->GetScrollableAreaIfScrollable();
    if (scrollable_area)
      break;
    scroll_parent = scroll_parent->ParentObject();
  }
  if (!scroll_parent || !scrollable_area)
    return;

  IntRect object_rect = PixelSnappedIntRect(GetBoundsInFrameCoordinates());
  IntSize scroll_offset = scrollable_area->ScrollOffsetInt();
  IntRect scroll_visible_rect = scrollable_area->VisibleContentRect();

  // Convert the object rect into local coordinates.
  if (!scroll_parent->IsWebArea()) {
    object_rect.MoveBy(IntPoint(scroll_offset));
    object_rect.MoveBy(
        -PixelSnappedIntRect(scroll_parent->GetBoundsInFrameCoordinates())
             .Location());
  }

  int desired_x = ComputeBestScrollOffset(
      scroll_offset.Width(), object_rect.X() + subfocus.X(),
      object_rect.X() + subfocus.MaxX(), object_rect.X(), object_rect.MaxX(), 0,
      scroll_visible_rect.Width());
  int desired_y = ComputeBestScrollOffset(
      scroll_offset.Height(), object_rect.Y() + subfocus.Y(),
      object_rect.Y() + subfocus.MaxY(), object_rect.Y(), object_rect.MaxY(), 0,
      scroll_visible_rect.Height());

  scroll_parent->SetScrollOffset(IntPoint(desired_x, desired_y));

  // Convert the subfocus into the coordinates of the scroll parent.
  IntRect new_subfocus = subfocus;
  IntRect new_element_rect = PixelSnappedIntRect(GetBoundsInFrameCoordinates());
  IntRect scroll_parent_rect =
      PixelSnappedIntRect(scroll_parent->GetBoundsInFrameCoordinates());
  new_subfocus.Move(new_element_rect.X(), new_element_rect.Y());
  new_subfocus.Move(-scroll_parent_rect.X(), -scroll_parent_rect.Y());

  if (scroll_parent->ParentObject()) {
    // Recursively make sure the scroll parent itself is visible.
    scroll_parent->ScrollToMakeVisibleWithSubFocus(new_subfocus);
  } else {
    // To minimize the number of notifications, only fire one on the topmost
    // object that has been scrolled.
    AxObjectCache().PostNotification(const_cast<AXObject*>(this),
                                     AXObjectCacheImpl::kAXLocationChanged);
  }
}

void AXObject::ScrollToGlobalPoint(const IntPoint& global_point) const {
  // Search up the parent chain and create a vector of all scrollable parent
  // objects and ending with this object itself.
  HeapVector<Member<const AXObject>> objects;
  AXObject* parent_object;
  for (parent_object = this->ParentObject(); parent_object;
       parent_object = parent_object->ParentObject()) {
    if (parent_object->GetScrollableAreaIfScrollable())
      objects.push_front(parent_object);
  }
  objects.push_back(this);

  // Start with the outermost scrollable (the main window) and try to scroll the
  // next innermost object to the given point.
  int offset_x = 0, offset_y = 0;
  IntPoint point = global_point;
  size_t levels = objects.size() - 1;
  for (size_t i = 0; i < levels; i++) {
    const AXObject* outer = objects[i];
    const AXObject* inner = objects[i + 1];
    ScrollableArea* scrollable_area = outer->GetScrollableAreaIfScrollable();

    IntRect inner_rect =
        inner->IsWebArea()
            ? PixelSnappedIntRect(
                  inner->ParentObject()->GetBoundsInFrameCoordinates())
            : PixelSnappedIntRect(inner->GetBoundsInFrameCoordinates());
    IntRect object_rect = inner_rect;
    IntSize scroll_offset = scrollable_area->ScrollOffsetInt();

    // Convert the object rect into local coordinates.
    object_rect.Move(offset_x, offset_y);
    if (!outer->IsWebArea())
      object_rect.Move(scroll_offset.Width(), scroll_offset.Height());

    int desired_x = ComputeBestScrollOffset(
        0, object_rect.X(), object_rect.MaxX(), object_rect.X(),
        object_rect.MaxX(), point.X(), point.X());
    int desired_y = ComputeBestScrollOffset(
        0, object_rect.Y(), object_rect.MaxY(), object_rect.Y(),
        object_rect.MaxY(), point.Y(), point.Y());
    outer->SetScrollOffset(IntPoint(desired_x, desired_y));

    if (outer->IsWebArea() && !inner->IsWebArea()) {
      // If outer object we just scrolled is a web area (frame) but the inner
      // object is not, keep track of the coordinate transformation to apply to
      // future nested calculations.
      scroll_offset = scrollable_area->ScrollOffsetInt();
      offset_x -= (scroll_offset.Width() + point.X());
      offset_y -= (scroll_offset.Height() + point.Y());
      point.Move(scroll_offset.Width() - inner_rect.Width(),
                 scroll_offset.Height() - inner_rect.Y());
    } else if (inner->IsWebArea()) {
      // Otherwise, if the inner object is a web area, reset the coordinate
      // transformation.
      offset_x = 0;
      offset_y = 0;
    }
  }

  // To minimize the number of notifications, only fire one on the topmost
  // object that has been scrolled.
  DCHECK(objects[0]);
  // TODO(nektar): Switch to postNotification(objects[0] and remove |getNode|.
  AxObjectCache().PostNotification(objects[0]->GetNode(),
                                   AXObjectCacheImpl::kAXLocationChanged);
}

void AXObject::SetSequentialFocusNavigationStartingPoint() {
  // Call it on the nearest ancestor that overrides this with a specific
  // implementation.
  if (ParentObject())
    ParentObject()->SetSequentialFocusNavigationStartingPoint();
}

void AXObject::NotifyIfIgnoredValueChanged() {
  bool is_ignored = AccessibilityIsIgnored();
  if (LastKnownIsIgnoredValue() != is_ignored) {
    AxObjectCache().ChildrenChanged(ParentObject());
    SetLastKnownIsIgnoredValue(is_ignored);
  }
}

void AXObject::SelectionChanged() {
  if (AXObject* parent = ParentObjectIfExists())
    parent->SelectionChanged();
}

int AXObject::LineForPosition(const VisiblePosition& position) const {
  if (position.IsNull() || !GetNode())
    return -1;

  // If the position is not in the same editable region as this AX object,
  // return -1.
  Node* container_node = position.DeepEquivalent().ComputeContainerNode();
  if (!container_node->IsShadowIncludingInclusiveAncestorOf(GetNode()) &&
      !GetNode()->IsShadowIncludingInclusiveAncestorOf(container_node))
    return -1;

  int line_count = -1;
  VisiblePosition current_position = position;
  VisiblePosition previous_position;

  // Move up until we get to the top.
  // FIXME: This only takes us to the top of the rootEditableElement, not the
  // top of the top document.
  do {
    previous_position = current_position;
    current_position = PreviousLinePosition(current_position, LayoutUnit(),
                                            kHasEditableAXRole);
    ++line_count;
  } while (current_position.IsNotNull() &&
           !InSameLine(current_position, previous_position));

  return line_count;
}

bool AXObject::IsARIAControl(AccessibilityRole aria_role) {
  return IsARIAInput(aria_role) || aria_role == kButtonRole ||
         aria_role == kComboBoxRole || aria_role == kSliderRole;
}

bool AXObject::IsARIAInput(AccessibilityRole aria_role) {
  return aria_role == kRadioButtonRole || aria_role == kCheckBoxRole ||
         aria_role == kTextFieldRole || aria_role == kSwitchRole ||
         aria_role == kSearchBoxRole;
}

AccessibilityRole AXObject::AriaRoleToWebCoreRole(const String& value) {
  DCHECK(!value.IsEmpty());

  static const ARIARoleMap* role_map = CreateARIARoleMap();

  Vector<String> role_vector;
  value.Split(' ', role_vector);
  AccessibilityRole role = kUnknownRole;
  for (const auto& child : role_vector) {
    role = role_map->at(child);
    if (role)
      return role;
  }

  return role;
}

bool AXObject::NameFromContents(bool recursive) const {
  // ARIA 1.1, section 5.2.7.5.
  bool result = false;

  switch (RoleValue()) {
    // ----- NameFrom: contents -------------------------
    // Get their own name from contents, or contribute to ancestors
    case kAnchorRole:
    case kButtonRole:
    case kCellRole:
    case kCheckBoxRole:
    case kColumnHeaderRole:
    case kDisclosureTriangleRole:
    case kHeadingRole:
    case kLineBreakRole:
    case kLinkRole:
    case kListBoxOptionRole:
    case kMenuButtonRole:
    case kMenuItemRole:
    case kMenuItemCheckBoxRole:
    case kMenuItemRadioRole:
    case kMenuListOptionRole:
    case kPopUpButtonRole:
    case kRadioButtonRole:
    case kRowHeaderRole:
    case kStaticTextRole:
    case kSwitchRole:
    case kTabRole:
    case kToggleButtonRole:
    case kTreeItemRole:
    case kUserInterfaceTooltipRole:
      result = true;
      break;

    // ----- No name from contents -------------------------
    // These never have or contribute a name from contents, as they are
    // containers for many subobjects. Superset of nameFrom:author ARIA roles.
    case kAlertRole:
    case kAlertDialogRole:
    case kApplicationRole:
    case kAudioRole:
    case kArticleRole:
    case kBannerRole:
    case kBlockquoteRole:
    case kColorWellRole:
    case kColumnRole:
    case kComboBoxRole:
    case kComplementaryRole:
    case kContentInfoRole:
    case kDateRole:
    case kDateTimeRole:
    case kDefinitionRole:
    case kDialogRole:
    case kDirectoryRole:
    case kDocumentRole:
    case kEmbeddedObjectRole:
    case kFeedRole:
    case kFigureRole:
    case kFormRole:
    case kGridRole:
    case kGroupRole:
    case kIframePresentationalRole:
    case kIframeRole:
    case kImageRole:
    case kInputTimeRole:
    case kListBoxRole:
    case kLogRole:
    case kMainRole:
    case kMarqueeRole:
    case kMathRole:
    case kMenuListPopupRole:
    case kMenuRole:
    case kMenuBarRole:
    case kMeterRole:
    case kNavigationRole:
    case kNoteRole:
    case kOutlineRole:
    case kProgressIndicatorRole:
    case kRadioGroupRole:
    case kRootWebAreaRole:
    case kScrollBarRole:
    case kSearchRole:
    case kSearchBoxRole:
    case kSplitterRole:
    case kSliderRole:
    case kSpinButtonRole:
    case kStatusRole:
    case kScrollAreaRole:
    case kSeamlessWebAreaRole:
    case kSliderThumbRole:
    case kSpinButtonPartRole:
    case kSVGRootRole:
    case kTableRole:
    case kTableHeaderContainerRole:
    case kTabGroupRole:
    case kTabListRole:
    case kTabPanelRole:
    case kTermRole:
    case kTextFieldRole:
    case kTimeRole:
    case kTimerRole:
    case kToolbarRole:
    case kTreeRole:
    case kTreeGridRole:
    case kVideoRole:
    case kWebAreaRole:
    case kWindowRole:
      result = false;
      break;

    // ----- Conditional: contribute to ancestor only, unless focusable -------
    // Some objects can contribute their contents to ancestor names, but
    // only have their own name if they are focusable
    case kAbbrRole:
    case kAnnotationRole:
    case kBusyIndicatorRole:
    case kCanvasRole:
    case kCaptionRole:
    case kDescriptionListDetailRole:
    case kDescriptionListRole:
    case kDescriptionListTermRole:
    case kDetailsRole:
    case kFigcaptionRole:
    case kFooterRole:
    case kGenericContainerRole:
    case kIgnoredRole:
    case kImageMapLinkRole:
    case kImageMapRole:
    case kInlineTextBoxRole:
    case kLabelRole:
    case kLegendRole:
    case kListRole:
    case kListItemRole:
    case kListMarkerRole:
    case kMarkRole:
    case kNoneRole:
    case kParagraphRole:
    case kPreRole:
    case kPresentationalRole:
    case kRegionRole:
    // Spec says we should always expose the name on rows,
    // but for performance reasons we only do it
    // if the row might receive focus
    case kRowRole:
    case kRubyRole:
    case kRulerRole:
      result = recursive || (CanReceiveAccessibilityFocus() && !IsEditable());
      break;

    case kUnknownRole:
    case kNumRoles:
      LOG(ERROR) << "kUnknownRole for " << GetNode();
      NOTREACHED();
      break;
  }

  return result;
}

AccessibilityRole AXObject::ButtonRoleType() const {
  // If aria-pressed is present, then it should be exposed as a toggle button.
  // http://www.w3.org/TR/wai-aria/states_and_properties#aria-pressed
  if (AriaPressedIsPresent())
    return kToggleButtonRole;
  if (AriaHasPopup())
    return kPopUpButtonRole;
  // We don't contemplate RadioButtonRole, as it depends on the input
  // type.

  return kButtonRole;
}

const AtomicString& AXObject::RoleName(AccessibilityRole role) {
  static const Vector<AtomicString>* role_name_vector = CreateRoleNameVector();

  return role_name_vector->at(role);
}

const AtomicString& AXObject::InternalRoleName(AccessibilityRole role) {
  static const Vector<AtomicString>* internal_role_name_vector =
      CreateInternalRoleNameVector();

  return internal_role_name_vector->at(role);
}

DEFINE_TRACE(AXObject) {
  visitor->Trace(children_);
  visitor->Trace(parent_);
  visitor->Trace(cached_live_region_root_);
  visitor->Trace(ax_object_cache_);
}

}  // namespace blink
