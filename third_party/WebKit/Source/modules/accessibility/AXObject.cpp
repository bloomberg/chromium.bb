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
#include "core/css/resolver/StyleResolver.h"
#include "core/dom/AccessibleNode.h"
#include "core/dom/AccessibleNodeList.h"
#include "core/dom/UserGestureIndicator.h"
#include "core/editing/EditingUtilities.h"
#include "core/editing/VisiblePosition.h"
#include "core/editing/VisibleUnits.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/LocalFrameView.h"
#include "core/frame/Settings.h"
#include "core/html/HTMLDialogElement.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/html/forms/HTMLInputElement.h"
#include "core/html/parser/HTMLParserIdioms.h"
#include "core/input/ContextMenuAllowedScope.h"
#include "core/input/EventHandler.h"
#include "core/input_type_names.h"
#include "core/layout/LayoutBoxModelObject.h"
#include "core/layout/LayoutView.h"
#include "core/page/Page.h"
#include "modules/accessibility/AXObjectCacheImpl.h"
#include "modules/accessibility/AXSparseAttributeSetter.h"
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
    {kParagraphRole, "Paragraph"},
    {kPopUpButtonRole, "PopUpButton"},
    {kPreRole, "Pre"},
    {kPresentationalRole, "Presentational"},
    {kProgressIndicatorRole, "ProgressIndicator"},
    {kRadioButtonRole, "RadioButton"},
    {kRadioGroupRole, "RadioGroup"},
    {kRegionRole, "Region"},
    {kRowHeaderRole, "RowHeader"},
    {kRowRole, "Row"},
    {kRubyRole, "Ruby"},
    {kSVGRootRole, "SVGRoot"},
    {kScrollBarRole, "ScrollBar"},
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
    {kWebAreaRole, "WebArea"}};

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
      aria_role_(kUnknownRole),
      last_known_is_ignored_value_(kDefaultBehavior),
      explicit_container_id_(0),
      parent_(nullptr),
      last_modification_count_(-1),
      cached_is_ignored_(false),
      cached_is_inert_or_aria_hidden_(false),
      cached_is_descendant_of_leaf_node_(false),
      cached_is_descendant_of_disabled_node_(false),
      cached_has_inherited_presentational_role_(false),
      cached_ancestor_exposes_active_descendant_(false),
      cached_is_editable_root_(false),
      cached_live_region_root_(nullptr),
      ax_object_cache_(&ax_object_cache) {
  ++number_of_live_ax_objects_;
}

AXObject::~AXObject() {
  DCHECK(IsDetached());
  --number_of_live_ax_objects_;
}

void AXObject::Init() {
  role_ = DetermineAccessibilityRole();
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
  Element* element = this->GetElement();
  if (!element)
    return g_null_atom;

  return AccessibleNode::GetPropertyOrARIAAttribute(element, property);
}

Element* AXObject::GetAOMPropertyOrARIAAttribute(
    AOMRelationProperty property) const {
  Element* element = this->GetElement();
  if (!element)
    return nullptr;

  return AccessibleNode::GetPropertyOrARIAAttribute(element, property);
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

bool AXObject::HasAOMPropertyOrARIAAttribute(AOMStringProperty property,
                                             AtomicString& result) const {
  Element* element = this->GetElement();
  if (!element)
    return false;

  result = AccessibleNode::GetPropertyOrARIAAttribute(element, property);
  return !result.IsNull();
}

AccessibleNode* AXObject::GetAccessibleNode() const {
  Element* element = GetElement();
  if (!element)
    return nullptr;

  return element->ExistingAccessibleNode();
}

void AXObject::GetSparseAXAttributes(
    AXSparseAttributeClient& sparse_attribute_client) const {
  AXSparseAttributeAOMPropertyClient property_client(*ax_object_cache_,
                                                     sparse_attribute_client);
  HashSet<QualifiedName> shadowed_aria_attributes;

  AccessibleNode* accessible_node = GetAccessibleNode();
  if (accessible_node) {
    accessible_node->GetAllAOMProperties(&property_client,
                                         shadowed_aria_attributes);
  }

  Element* element = GetElement();
  if (!element)
    return;

  AXSparseAttributeSetterMap& ax_sparse_attribute_setter_map =
      GetSparseAttributeSetterMap();
  AttributeCollection attributes = element->AttributesWithoutUpdate();
  for (const Attribute& attr : attributes) {
    if (shadowed_aria_attributes.Contains(attr.GetName()))
      continue;

    AXSparseAttributeSetter* setter =
        ax_sparse_attribute_setter_map.at(attr.GetName());
    if (setter)
      setter->Run(*this, sparse_attribute_client, attr.Value());
  }
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
    if (EqualIgnoringASCIICase(checked_attribute, "mixed")) {
      // Only checkable role that doesn't support mixed is the switch.
      if (role != kSwitchRole)
        return kCheckedStateMixed;
    }

    // Anything other than "false" should be treated as "true".
    return EqualIgnoringASCIICase(checked_attribute, "false")
               ? kCheckedStateFalse
               : kCheckedStateTrue;
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

    if (IsHTMLInputElement(*node) &&
        ToHTMLInputElement(*node).ShouldAppearChecked()) {
      return kCheckedStateTrue;
    }
  }

  return kCheckedStateFalse;
}

bool AXObject::IsNativeCheckboxInMixedState(const Node* node) {
  if (!IsHTMLInputElement(node))
    return false;

  const HTMLInputElement* input = ToHTMLInputElement(node);
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
  if (IsButton() || IsLink() || IsTextControl())
    return true;

  // TODO(dmazzoni): Ensure that kColorWellRole and kSpinButtonRole are
  // correctly handled here via their constituent parts.
  switch (RoleValue()) {
    // TODO(dmazzoni): Replace kComboBoxRole with two new combo box roles.
    // Composite widget and text field.
    case kComboBoxRole:
    case kCheckBoxRole:
    case kDisclosureTriangleRole:
    case kListBoxRole:
    case kListBoxOptionRole:
    case kMenuItemCheckBoxRole:
    case kMenuItemRadioRole:
    case kMenuItemRole:
    case kMenuListOptionRole:
    case kRadioButtonRole:
    case kSwitchRole:
    case kTabRole:
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
  cached_is_ignored_ = ComputeAccessibilityIsIgnored();
  cached_is_editable_root_ =
      GetNode() ? IsNativeTextControl() || IsRootEditableElement(*GetNode())
                : false;
  cached_live_region_root_ =
      IsLiveRegion()
          ? const_cast<AXObject*>(this)
          : (ParentObjectIfExists() ? ParentObjectIfExists()->LiveRegionRoot()
                                    : nullptr);
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

bool AXObject::DispatchEventToAOMEventListeners(Event& event,
                                                Element* target_element) {
  HeapVector<Member<AccessibleNode>> event_path;
  for (AXObject* ancestor = this; ancestor;
       ancestor = ancestor->ParentObject()) {
    Element* ancestor_element = ancestor->GetElement();
    if (!ancestor_element)
      continue;

    AccessibleNode* ancestor_accessible_node =
        ancestor_element->ExistingAccessibleNode();
    if (!ancestor_accessible_node)
      continue;

    if (!ancestor_accessible_node->HasEventListeners(event.type()))
      continue;

    event_path.push_back(ancestor_accessible_node);
  }

  // Short-circuit: if there are no AccessibleNodes attached anywhere
  // in the ancestry of this node, exit.
  if (!event_path.size())
    return false;

  // Check if the user has granted permission for this domain to use
  // AOM event listeners yet. This may trigger an infobar, but we shouldn't
  // block, so whatever decision the user makes will apply to the next
  // event received after that.
  //
  // Note that we only ask the user about this permission the first
  // time an event is received that actually would have triggered an
  // event listener. However, if the user grants this permission, it
  // persists for this origin from then on.
  if (!AxObjectCache().CanCallAOMEventListeners()) {
    AxObjectCache().RequestAOMEventListenerPermission();
    return false;
  }

  // Since we now know the AOM is being used in this document, get the
  // AccessibleNode for the target element and create it if necessary -
  // otherwise we wouldn't be able to set the event target. However note
  // that if it didn't previously exist it won't be part of the event path.
  if (!target_element)
    target_element = GetElement();
  AccessibleNode* target = nullptr;
  if (target_element) {
    target = target_element->accessibleNode();
    event.SetTarget(target);
  }

  // Capturing phase.
  event.SetEventPhase(Event::kCapturingPhase);
  for (int i = static_cast<int>(event_path.size()) - 1; i >= 0; i--) {
    // Don't call capturing event listeners on the target. Note that
    // the target may not necessarily be in the event path which is why
    // we check here.
    if (event_path[i] == target)
      break;

    event.SetCurrentTarget(event_path[i]);
    event_path[i]->FireEventListeners(&event);
    if (event.PropagationStopped())
      return true;
  }

  // Targeting phase.
  event.SetEventPhase(Event::kAtTarget);
  event.SetCurrentTarget(event_path[0]);
  event_path[0]->FireEventListeners(&event);
  if (event.PropagationStopped())
    return true;

  // Bubbling phase.
  event.SetEventPhase(Event::kBubblingPhase);
  for (size_t i = 1; i < event_path.size(); i++) {
    event.SetCurrentTarget(event_path[i]);
    event_path[i]->FireEventListeners(&event);
    if (event.PropagationStopped())
      return true;
  }

  if (event.defaultPrevented())
    return true;

  return false;
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

bool AXObject::SupportsSetSizeAndPosInSet() const {
  switch (RoleValue()) {
    case kListBoxOptionRole:
    case kListItemRole:
    case kMenuItemRole:
    case kMenuItemRadioRole:
    case kMenuItemCheckBoxRole:
    case kMenuListOptionRole:
    case kRadioButtonRole:
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
  if (!GetNode() || (!IsHTMLBRElement(GetNode()) && role != kStaticTextRole &&
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
        Vector<String> ids;
        text_alternative =
            TextFromAriaLabelledby(visited_copy, related_objects, ids);
        if (!ids.IsEmpty())
          AxObjectCache().UpdateReverseRelations(this, ids);
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
                                     const QualifiedName& attribute,
                                     Vector<String>& ids) const {
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
    HeapVector<Member<Element>>& elements,
    Vector<String>& ids) const {
  // Try both spellings, but prefer aria-labelledby, which is the official spec.
  ElementsFromAttribute(elements, aria_labelledbyAttr, ids);
  if (!ids.size())
    ElementsFromAttribute(elements, aria_labeledbyAttr, ids);
}

String AXObject::TextFromAriaLabelledby(AXObjectSet& visited,
                                        AXRelatedObjectVector* related_objects,
                                        Vector<String>& ids) const {
  HeapVector<Member<Element>> elements;
  AriaLabelledbyElementVector(elements, ids);
  return TextFromElements(true, visited, elements, related_objects);
}

String AXObject::TextFromAriaDescribedby(AXRelatedObjectVector* related_objects,
                                         Vector<String>& ids) const {
  AXObjectSet visited;
  HeapVector<Member<Element>> elements;
  ElementsFromAttribute(elements, aria_describedbyAttr, ids);
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
  Element* action_element = ActionElement();
  if (!action_element)
    return AXDefaultActionVerb::kNone;

  // TODO(dmazzoni): Ensure that combo box text field is handled here.
  if (IsTextControl())
    return AXDefaultActionVerb::kActivate;

  if (IsCheckable()) {
    return CheckedState() != kCheckedStateTrue ? AXDefaultActionVerb::kCheck
                                               : AXDefaultActionVerb::kUncheck;
  }

  switch (RoleValue()) {
    case kButtonRole:
    case kDisclosureTriangleRole:
    case kToggleButtonRole:
      return AXDefaultActionVerb::kPress;
    case kListBoxOptionRole:
    case kMenuItemRadioRole:
    case kMenuItemRole:
    case kMenuListOptionRole:
      return AXDefaultActionVerb::kSelect;
    case kLinkRole:
      return AXDefaultActionVerb::kJump;
    // TODO(dmazzoni): Change kComboBoxRole to combo box composite widget.
    case kComboBoxRole:
    case kListBoxRole:
    case kPopUpButtonRole:
      return AXDefaultActionVerb::kOpen;
    default:
      if (action_element == GetNode())
        return AXDefaultActionVerb::kClick;
      return AXDefaultActionVerb::kClickAncestor;
  }
}

bool AXObject::AriaPressedIsPresent() const {
  AtomicString result;
  return HasAOMPropertyOrARIAAttribute(AOMStringProperty::kPressed, result);
}

bool AXObject::AriaCheckedIsPresent() const {
  AtomicString result;
  return HasAOMPropertyOrARIAAttribute(AOMStringProperty::kChecked, result);
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
  return !live_region.IsEmpty() && !EqualIgnoringASCIICase(live_region, "off");
}

AXRestriction AXObject::Restriction() const {
  // According to ARIA, all elements of the base markup can be disabled.
  // According to CORE-AAM, any focusable descendant of aria-disabled
  // ancestor is also disabled.
  bool is_disabled;
  if (HasAOMPropertyOrARIAAttribute(AOMBooleanProperty::kDisabled,
                                    is_disabled)) {
    // Has aria-disabled, overrides native markup determining disabled.
    if (is_disabled)
      return kDisabled;
  } else if (CanSetFocusAttribute() && IsDescendantOfDisabledNode()) {
    // No aria-disabled, but other markup says it's disabled.
    return kDisabled;
  }

  // Check aria-readonly if supported by current role.
  bool is_read_only;
  if (CanSupportAriaReadOnly() &&
      HasAOMPropertyOrARIAAttribute(AOMBooleanProperty::kReadOnly,
                                    is_read_only)) {
    // ARIA overrides other readonly state markup.
    return is_read_only ? kReadOnly : kNone;
  }

  // This is a node that is not readonly and not disabled.
  return kNone;
}

AccessibilityRole AXObject::DetermineAccessibilityRole() {
  aria_role_ = DetermineAriaRoleAttribute();
  return aria_role_;
}

AccessibilityRole AXObject::AriaRoleAttribute() const {
  return aria_role_;
}

AccessibilityRole AXObject::DetermineAriaRoleAttribute() const {
  const AtomicString& aria_role =
      GetAOMPropertyOrARIAAttribute(AOMStringProperty::kRole);
  if (aria_role.IsNull() || aria_role.IsEmpty())
    return kUnknownRole;

  AccessibilityRole role = AriaRoleToWebCoreRole(aria_role);

  // ARIA states if an item can get focus, it should not be presentational.
  if ((role == kNoneRole || role == kPresentationalRole) &&
      CanSetFocusAttribute())
    return kUnknownRole;

  if (role == kButtonRole)
    role = ButtonRoleType();

  role = RemapAriaRoleDueToParent(role);

  if (role)
    return role;

  return kUnknownRole;
}

AccessibilityRole AXObject::RemapAriaRoleDueToParent(
    AccessibilityRole role) const {
  // Some objects change their role based on their parent.
  // However, asking for the unignoredParent calls accessibilityIsIgnored(),
  // which can trigger a loop.  While inside the call stack of creating an
  // element, we need to avoid accessibilityIsIgnored().
  // https://bugs.webkit.org/show_bug.cgi?id=65174

  if (role != kListBoxOptionRole && role != kMenuItemRole)
    return role;

  for (AXObject* parent = ParentObject();
       parent && !parent->AccessibilityIsIgnored();
       parent = parent->ParentObject()) {
    AccessibilityRole parent_aria_role = parent->AriaRoleAttribute();

    // Selects and listboxes both have options as child roles, but they map to
    // different roles within WebCore.
    if (role == kListBoxOptionRole && parent_aria_role == kMenuRole)
      return kMenuItemRole;
    // An aria "menuitem" may map to MenuButton or MenuItem depending on its
    // parent.
    if (role == kMenuItemRole && parent_aria_role == kGroupRole)
      return kMenuButtonRole;

    // If the parent had a different role, then we don't need to continue
    // searching up the chain.
    if (parent_aria_role)
      break;
  }

  return role;
}

bool AXObject::IsEditableRoot() const {
  UpdateCachedAttributeValuesIfNeeded();
  return cached_is_editable_root_;
}

AXObject* AXObject::LiveRegionRoot() const {
  UpdateCachedAttributeValuesIfNeeded();
  return cached_live_region_root_;
}

bool AXObject::LiveRegionAtomic() const {
  bool atomic = false;
  if (HasAOMPropertyOrARIAAttribute(AOMBooleanProperty::kAtomic, atomic))
    return atomic;

  // ARIA roles "alert" and "status" should have an implicit aria-atomic value
  // of true.
  return RoleValue() == kAlertRole || RoleValue() == kStatusRole;
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
  return cached_live_region_root_ &&
         cached_live_region_root_->AOMPropertyOrARIAAttributeIsTrue(
             AOMBooleanProperty::kBusy);
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

void AXObject::AddAccessibleNodeChildren() {
  Element* element = GetElement();
  if (!element)
    return;

  AccessibleNode* accessible_node = element->ExistingAccessibleNode();
  if (!accessible_node)
    return;

  for (const auto& child : accessible_node->GetChildren())
    children_.push_back(AxObjectCache().GetOrCreate(child));
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
          FloatSize(layout_object->GetFrame()->View()->Size()));
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

bool AXObject::RequestDecrementAction() {
  Element* element = GetElement();
  if (element) {
    Event* event = Event::CreateCancelable(EventTypeNames::accessibledecrement);
    if (DispatchEventToAOMEventListeners(*event, element)) {
      return true;
    }
  }

  return OnNativeDecrementAction();
}

bool AXObject::RequestClickAction() {
  Element* element = GetElement();
  if (element) {
    Event* event = Event::CreateCancelable(EventTypeNames::accessibleclick);
    if (DispatchEventToAOMEventListeners(*event, element))
      return true;
  }

  return OnNativeClickAction();
}

bool AXObject::OnNativeClickAction() {
  Document* document = GetDocument();
  if (!document)
    return false;

  std::unique_ptr<UserGestureIndicator> gesture_indicator =
      LocalFrame::CreateUserGesture(document->GetFrame(),
                                    UserGestureToken::kNewGesture);

  Element* element = GetElement();
  if (!element && GetNode())
    element = GetNode()->parentElement();

  if (element) {
    element->AccessKeyAction(true);
    return true;
  }

  if (CanSetFocusAttribute())
    return OnNativeFocusAction();

  return false;
}

bool AXObject::RequestFocusAction() {
  Element* element = GetElement();
  if (element) {
    Event* event = Event::CreateCancelable(EventTypeNames::accessiblefocus);
    if (DispatchEventToAOMEventListeners(*event, element))
      return true;
  }

  return OnNativeFocusAction();
}

bool AXObject::RequestIncrementAction() {
  Element* element = GetElement();
  if (element) {
    Event* event = Event::CreateCancelable(EventTypeNames::accessibleincrement);
    if (DispatchEventToAOMEventListeners(*event, element))
      return true;
  }

  return OnNativeIncrementAction();
}

bool AXObject::RequestScrollToGlobalPointAction(const IntPoint& point) {
  return OnNativeScrollToGlobalPointAction(point);
}

bool AXObject::RequestScrollToMakeVisibleAction() {
  Element* element = GetElement();
  if (element) {
    Event* event =
        Event::CreateCancelable(EventTypeNames::accessiblescrollintoview);
    if (DispatchEventToAOMEventListeners(*event, element))
      return true;
  }

  return OnNativeScrollToMakeVisibleAction();
}

bool AXObject::RequestScrollToMakeVisibleWithSubFocusAction(
    const IntRect& subfocus) {
  return OnNativeScrollToMakeVisibleWithSubFocusAction(subfocus);
}

bool AXObject::RequestSetSelectedAction(bool selected) {
  return OnNativeSetSelectedAction(selected);
}

bool AXObject::RequestSetSelectionAction(const AXRange& range) {
  return OnNativeSetSelectionAction(range);
}

bool AXObject::RequestSetSequentialFocusNavigationStartingPointAction() {
  return OnNativeSetSequentialFocusNavigationStartingPointAction();
}

bool AXObject::RequestSetValueAction(const String& value) {
  return OnNativeSetValueAction(value);
}

bool AXObject::RequestShowContextMenuAction() {
  Element* element = GetElement();
  if (element) {
    Event* event =
        Event::CreateCancelable(EventTypeNames::accessiblecontextmenu);
    if (DispatchEventToAOMEventListeners(*event, element))
      return true;
  }

  return OnNativeShowContextMenuAction();
}

bool AXObject::OnNativeScrollToMakeVisibleAction() const {
  Node* node = GetNode();
  LayoutObject* layout_object = node ? node->GetLayoutObject() : nullptr;
  if (!layout_object || !node->isConnected())
    return false;
  LayoutRect target_rect(layout_object->AbsoluteBoundingBoxRect());
  layout_object->ScrollRectToVisible(
      target_rect, ScrollAlignment::kAlignCenterIfNeeded,
      ScrollAlignment::kAlignCenterIfNeeded, kProgrammaticScroll,
      !GetDocument()->GetPage()->GetSettings().GetInertVisualViewport(),
      kScrollBehaviorAuto);
  AxObjectCache().PostNotification(
      AxObjectCache().GetOrCreate(GetDocument()->GetLayoutView()),
      AXObjectCacheImpl::kAXLocationChanged);
  return true;
}

bool AXObject::OnNativeScrollToMakeVisibleWithSubFocusAction(
    const IntRect& rect) const {
  Node* node = GetNode();
  LayoutObject* layout_object = node ? node->GetLayoutObject() : nullptr;
  if (!layout_object || !node->isConnected())
    return false;
  LayoutRect target_rect(
      layout_object->LocalToAbsoluteQuad(FloatQuad(FloatRect(rect)))
          .BoundingBox());
  // TODO(szager): This scroll alignment is intended to preserve existing
  // behavior to the extent possible, but it's not clear that this behavior is
  // well-spec'ed or optimal.  In particular, it favors centering things in
  // the visible viewport rather than snapping them to the closest edge, which
  // is the default behavior of element.scrollIntoView.
  ScrollAlignment scroll_alignment = {
      kScrollAlignmentNoScroll, kScrollAlignmentCenter, kScrollAlignmentCenter};
  layout_object->ScrollRectToVisible(
      target_rect, scroll_alignment, scroll_alignment, kProgrammaticScroll,
      !GetDocument()->GetPage()->GetSettings().GetInertVisualViewport(),
      kScrollBehaviorAuto);
  AxObjectCache().PostNotification(
      AxObjectCache().GetOrCreate(GetDocument()->GetLayoutView()),
      AXObjectCacheImpl::kAXLocationChanged);
  return true;
}

bool AXObject::OnNativeScrollToGlobalPointAction(
    const IntPoint& global_point) const {
  Node* node = GetNode();
  LayoutObject* layout_object = node ? node->GetLayoutObject() : nullptr;
  if (!layout_object || !node->isConnected())
    return false;
  LayoutRect target_rect(layout_object->AbsoluteBoundingBoxRect());
  target_rect.MoveBy(-global_point);
  layout_object->ScrollRectToVisible(
      target_rect, ScrollAlignment::kAlignLeftAlways,
      ScrollAlignment::kAlignTopAlways, kProgrammaticScroll,
      !GetDocument()->GetPage()->GetSettings().GetInertVisualViewport(),
      kScrollBehaviorAuto);
  AxObjectCache().PostNotification(
      AxObjectCache().GetOrCreate(GetDocument()->GetLayoutView()),
      AXObjectCacheImpl::kAXLocationChanged);
  return true;
}

bool AXObject::OnNativeSetSequentialFocusNavigationStartingPointAction() {
  // Call it on the nearest ancestor that overrides this with a specific
  // implementation.
  if (ParentObject()) {
    return ParentObject()
        ->OnNativeSetSequentialFocusNavigationStartingPointAction();
  }
  return false;
}

bool AXObject::OnNativeDecrementAction() {
  return false;
}

bool AXObject::OnNativeFocusAction() {
  return false;
}

bool AXObject::OnNativeIncrementAction() {
  return false;
}

bool AXObject::OnNativeSetValueAction(const String&) {
  return false;
}

bool AXObject::OnNativeSetSelectedAction(bool) {
  return false;
}

bool AXObject::OnNativeSetSelectionAction(const AXRange& range) {
  return false;
}

bool AXObject::OnNativeShowContextMenuAction() {
  Element* element = GetElement();
  if (!element)
    element = ParentObject() ? ParentObject()->GetElement() : nullptr;
  if (!element)
    return false;

  Document* document = GetDocument();
  if (!document || !document->GetFrame())
    return false;

  ContextMenuAllowedScope scope;
  document->GetFrame()->GetEventHandler().ShowNonLocatedContextMenu(element);
  return true;
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
    case kProgressIndicatorRole:
    case kRadioGroupRole:
    case kScrollBarRole:
    case kSearchRole:
    case kSearchBoxRole:
    case kSplitterRole:
    case kSliderRole:
    case kSpinButtonRole:
    case kStatusRole:
    case kSliderThumbRole:
    case kSpinButtonPartRole:
    case kSVGRootRole:
    case kTableRole:
    case kTableHeaderContainerRole:
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
      result = false;
      break;

    // ----- Conditional: contribute to ancestor only, unless focusable -------
    // Some objects can contribute their contents to ancestor names, but
    // only have their own name if they are focusable
    case kAbbrRole:
    case kAnnotationRole:
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

bool AXObject::CanSupportAriaReadOnly() const {
  switch (RoleValue()) {
    case kCellRole:
    case kCheckBoxRole:
    case kColorWellRole:
    case kColumnHeaderRole:
    case kComboBoxRole:
    case kDateRole:
    case kDateTimeRole:
    case kGridRole:
    case kInputTimeRole:
    case kListBoxRole:
    case kMenuButtonRole:
    case kMenuItemCheckBoxRole:
    case kMenuItemRadioRole:
    case kPopUpButtonRole:
    case kRadioGroupRole:
    case kRowHeaderRole:
    case kSearchBoxRole:
    case kSliderRole:
    case kSpinButtonRole:
    case kSwitchRole:
    case kTextFieldRole:
    case kToggleButtonRole:
    case kTreeGridRole:
      return true;
    default:
      break;
  }
  return false;
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

VisiblePosition AXObject::VisiblePositionForIndex(int) const {
  return VisiblePosition();
}

std::ostream& operator<<(std::ostream& stream, const AXObject& obj) {
  return stream << AXObject::InternalRoleName(obj.RoleValue()) << ": "
                << obj.ComputedName();
}

DEFINE_TRACE(AXObject) {
  visitor->Trace(children_);
  visitor->Trace(parent_);
  visitor->Trace(cached_live_region_root_);
  visitor->Trace(ax_object_cache_);
}

}  // namespace blink
