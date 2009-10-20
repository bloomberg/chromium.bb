// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "AccessibilityObject.h"

#undef LOG

#include "webkit/api/public/WebAccessibilityObject.h"
#include "webkit/glue/glue_util.h"
#include "webkit/tools/test_shell/accessibility_ui_element.h"

using namespace WebCore;

using WebKit::WebAccessibilityObject;

using webkit_glue::StringToStdString;
using webkit_glue::AccessibilityObjectToWebAccessibilityObject;
using webkit_glue::WebAccessibilityObjectToAccessibilityObject;

namespace {

static PassRefPtr<AccessibilityObject> AXObject(
    const WebAccessibilityObject& object) {
  RefPtr<AccessibilityObject> o =
      WebAccessibilityObjectToAccessibilityObject(object);
  // Always ask to update the render tree before querying accessibility object.
  o->updateBackingStore();
  return o;
}

// Map role value to string, matching Safari/Mac platform implementation to
// avoid rebaselining layout tests.
static std::string RoleToString(AccessibilityRole role) {
  std::string result = "AXRole: AX";
  switch (role) {
    case ButtonRole:
      return result.append("Button");
    case RadioButtonRole:
      return result.append("RadioButton");
    case CheckBoxRole:
      return result.append("CheckBox");
    case SliderRole:
      return result.append("Slider");
    case TabGroupRole:
      return result.append("TabGroup");
    case TextFieldRole:
      return result.append("TextField");
    case StaticTextRole:
      return result.append("StaticText");
    case TextAreaRole:
      return result.append("TextArea");
    case ScrollAreaRole:
      return result.append("ScrollArea");
    case PopUpButtonRole:
      return result.append("PopUpButton");
    case MenuButtonRole:
      return result.append("MenuButton");
    case TableRole:
      return result.append("Table");
    case ApplicationRole:
      return result.append("Application");
    case GroupRole:
      return result.append("Group");
    case RadioGroupRole:
      return result.append("RadioGroup");
    case ListRole:
      return result.append("List");
    case ScrollBarRole:
      return result.append("ScrollBar");
    case ValueIndicatorRole:
      return result.append("ValueIndicator");
    case ImageRole:
      return result.append("Image");
    case MenuBarRole:
      return result.append("MenuBar");
    case MenuRole:
      return result.append("Menu");
    case MenuItemRole:
      return result.append("MenuItem");
    case ColumnRole:
      return result.append("Column");
    case RowRole:
      return result.append("Row");
    case ToolbarRole:
      return result.append("Toolbar");
    case BusyIndicatorRole:
      return result.append("BusyIndicator");
    case ProgressIndicatorRole:
      return result.append("ProgressIndicator");
    case WindowRole:
      return result.append("Window");
    case DrawerRole:
      return result.append("Drawer");
    case SystemWideRole:
      return result.append("SystemWide");
    case OutlineRole:
      return result.append("Outline");
    case IncrementorRole:
      return result.append("Incrementor");
    case BrowserRole:
      return result.append("Browser");
    case ComboBoxRole:
      return result.append("ComboBox");
    case SplitGroupRole:
      return result.append("SplitGroup");
    case SplitterRole:
      return result.append("Splitter");
    case ColorWellRole:
      return result.append("ColorWell");
    case GrowAreaRole:
      return result.append("GrowArea");
    case SheetRole:
      return result.append("Sheet");
    case HelpTagRole:
      return result.append("HelpTag");
    case MatteRole:
      return result.append("Matte");
    case RulerRole:
      return result.append("Ruler");
    case RulerMarkerRole:
      return result.append("RulerMarker");
    case LinkRole:
      return result.append("Link");
    case DisclosureTriangleRole:
      return result.append("DisclosureTriangle");
    case GridRole:
      return result.append("Grid");
    case CellRole:
      return result.append("Cell");
    case ColumnHeaderRole:
      return result.append("ColumnHeader");
    case RowHeaderRole:
      return result.append("RowHeader");
    case WebCoreLinkRole:
      // Maps to Link role.
      return result.append("Link");
    case ImageMapLinkRole:
      return result.append("ImageMapLink");
    case ImageMapRole:
      return result.append("ImageMap");
    case ListMarkerRole:
      return result.append("ListMarker");
    case WebAreaRole:
      return result.append("WebArea");
    case HeadingRole:
      return result.append("Heading");
    case ListBoxRole:
      return result.append("ListBox");
    case ListBoxOptionRole:
      return result.append("ListBoxOption");
    case TableHeaderContainerRole:
      return result.append("TableHeaderContainer");
    case DefinitionListTermRole:
      return result.append("DefinitionListTerm");
    case DefinitionListDefinitionRole:
      return result.append("DefinitionListDefinition");
    case AnnotationRole:
      return result.append("Annotation");
    case SliderThumbRole:
      return result.append("SliderThumb");
    case LandmarkApplicationRole:
      return result.append("LandmarkApplication");
    case LandmarkBannerRole:
      return result.append("LandmarkBanner");
    case LandmarkComplementaryRole:
      return result.append("LandmarkComplementary");
    case LandmarkContentInfoRole:
      return result.append("LandmarkContentInfo");
    case LandmarkMainRole:
      return result.append("LandmarkMain");
    case LandmarkNavigationRole:
      return result.append("LandmarkNavigation");
    case LandmarkSearchRole:
      return result.append("LandmarkSearch");
    case ApplicationLogRole:
      return result.append("ApplicationLog");
    case ApplicationMarqueeRole:
      return result.append("ApplicationMarquee");
    case ApplicationStatusRole:
      return result.append("ApplicationStatus");
    case ApplicationTimerRole:
      return result.append("ApplicationTimer");
    case DocumentRole:
      return result.append("Document");
    case DocumentArticleRole:
      return result.append("DocumentArticle");
    case DocumentNoteRole:
      return result.append("DocumentNote");
    case DocumentRegionRole:
      return result.append("DocumentRegion");
    case UserInterfaceTooltipRole:
      return result.append("UserInterfaceTooltip");
    default:
      // Also matches UnknownRole.
      return result.append("Unknown");
  }
}

}  // namespace

AccessibilityUIElement::AccessibilityUIElement(
    const WebKit::WebAccessibilityObject& object,
    Factory* factory)
    : accessibility_object_(object),
      factory_(factory) {

  DCHECK(factory);

  BindMethod("allAttributes", &AccessibilityUIElement::AllAttributesCallback);
  BindMethod("attributesOfLinkedUIElements",
             &AccessibilityUIElement::AttributesOfLinkedUIElementsCallback);
  BindMethod("attributesOfDocumentLinks",
             &AccessibilityUIElement::AttributesOfDocumentLinksCallback);
  BindMethod("attributesOfChildren",
             &AccessibilityUIElement::AttributesOfChildrenCallback);
  BindMethod("parameterizedAttributeNames",
             &AccessibilityUIElement::ParametrizedAttributeNamesCallback);
  BindMethod("lineForIndex", &AccessibilityUIElement::LineForIndexCallback);
  BindMethod("boundsForRange", &AccessibilityUIElement::BoundsForRangeCallback);
  BindMethod("stringForRange", &AccessibilityUIElement::StringForRangeCallback);
  BindMethod("childAtIndex", &AccessibilityUIElement::ChildAtIndexCallback);
  BindMethod("elementAtPoint", &AccessibilityUIElement::ElementAtPointCallback);
  BindMethod("attributesOfColumnHeaders",
             &AccessibilityUIElement::AttributesOfColumnHeadersCallback);
  BindMethod("attributesOfRowHeaders",
             &AccessibilityUIElement::AttributesOfRowHeadersCallback);
  BindMethod("attributesOfColumns",
             &AccessibilityUIElement::AttributesOfColumnsCallback);
  BindMethod("attributesOfRows",
             &AccessibilityUIElement::AttributesOfRowsCallback);
  BindMethod("attributesOfVisibleCells",
             &AccessibilityUIElement::AttributesOfVisibleCellsCallback);
  BindMethod("attributesOfHeader",
             &AccessibilityUIElement::AttributesOfHeaderCallback);
  BindMethod("indexInTable", &AccessibilityUIElement::IndexInTableCallback);
  BindMethod("rowIndexRange", &AccessibilityUIElement::RowIndexRangeCallback);
  BindMethod("columnIndexRange",
             &AccessibilityUIElement::ColumnIndexRangeCallback);
  BindMethod("cellForColumnAndRow",
             &AccessibilityUIElement::CellForColumnAndRowCallback);
  BindMethod("titleUIElement", &AccessibilityUIElement::TitleUIElementCallback);
  BindMethod("setSelectedTextRange",
             &AccessibilityUIElement::SetSelectedTextRangeCallback);
  BindMethod("attributeValue", &AccessibilityUIElement::AttributeValueCallback);
  BindMethod("isAttributeSettable",
             &AccessibilityUIElement::IsAttributeSettableCallback);
  BindMethod("isActionSupported",
             &AccessibilityUIElement::IsActionSupportedCallback);
  BindMethod("parentElement", &AccessibilityUIElement::ParentElementCallback);
  BindMethod("increment", &AccessibilityUIElement::IncrementCallback);
  BindMethod("decrement", &AccessibilityUIElement::DecrementCallback);

  BindProperty("role", &AccessibilityUIElement::RoleGetterCallback);
  BindProperty("subrole", &subrole_);
  BindProperty("title", &AccessibilityUIElement::TitleGetterCallback);
  BindProperty("description",
      &AccessibilityUIElement::DescriptionGetterCallback);
  BindProperty("language", &language_);
  BindProperty("x", &x_);
  BindProperty("y", &y_);
  BindProperty("width", &width_);
  BindProperty("height", &height_);
  BindProperty("clickPointX", &click_point_x_);
  BindProperty("clickPointY", &click_point_y_);
  BindProperty("intValue", &int_value_);
  BindProperty("minValue", &min_value_);
  BindProperty("maxValue", &max_value_);
  BindProperty("childrenCount",
      &AccessibilityUIElement::ChildrenCountGetterCallback);
  BindProperty("insertionPointLineNumber", &insertion_point_line_number_);
  BindProperty("selectedTextRange", &selected_text_range);
  BindProperty("isEnabled", &AccessibilityUIElement::IsEnabledGetterCallback);
  BindProperty("isRequired", &is_required_);
  BindProperty("valueDescription", &value_description_);

  BindFallbackMethod(&AccessibilityUIElement::FallbackCallback);
}

AccessibilityUIElement* AccessibilityUIElement::GetChildAtIndex(
    unsigned index) {
  RefPtr<AccessibilityObject> object = AXObject(accessibility_object());
  if (object->children().size() <= index)
    return NULL;

  WebAccessibilityObject child =
      AccessibilityObjectToWebAccessibilityObject(object->children()[index]);
  return factory_->Create(child);
}

std::string AccessibilityUIElement::GetTitle() {
  std::string title = StringToStdString(AXObject(
      accessibility_object())->title());
  return title.insert(0, "AXTitle: ");
}

std::string AccessibilityUIElement::GetDescription() {
  std::string description = StringToStdString(AXObject(
      accessibility_object())->accessibilityDescription());
  return description.insert(0, "AXDescription: ");
}

void AccessibilityUIElement::AllAttributesCallback(
    const CppArgumentList& args, CppVariant* result) {
  // TODO(dglazkov): Concatenate all attributes of the AccessibilityObject.
  std::string attributes(GetTitle());
  result->Set(attributes);
}

void AccessibilityUIElement::AttributesOfLinkedUIElementsCallback(
    const CppArgumentList& args, CppVariant* result) {
  result->SetNull();
}

void AccessibilityUIElement::AttributesOfDocumentLinksCallback(
    const CppArgumentList& args, CppVariant* result) {
  result->SetNull();
}

void AccessibilityUIElement::AttributesOfChildrenCallback(
    const CppArgumentList& args, CppVariant* result) {
  result->SetNull();
}

void AccessibilityUIElement::ParametrizedAttributeNamesCallback(
    const CppArgumentList& args, CppVariant* result) {
  result->SetNull();
}

void AccessibilityUIElement::LineForIndexCallback(
    const CppArgumentList& args, CppVariant* result) {
  result->SetNull();
}

void AccessibilityUIElement::BoundsForRangeCallback(
    const CppArgumentList& args, CppVariant* result) {
  result->SetNull();
}

void AccessibilityUIElement::StringForRangeCallback(
    const CppArgumentList& args, CppVariant* result) {
  result->SetNull();
}

void AccessibilityUIElement::ChildAtIndexCallback(
    const CppArgumentList& args, CppVariant* result) {
  if (args.size() == 0 || !args[0].isNumber()) {
    result->SetNull();
    return;
  }

  AccessibilityUIElement* child = GetChildAtIndex(args[0].ToInt32());
  if (!child) {
    result->SetNull();
    return;
  }

  result->Set(*(child->GetAsCppVariant()));
}

void AccessibilityUIElement::ElementAtPointCallback(
    const CppArgumentList& args, CppVariant* result) {
  result->SetNull();
}

void AccessibilityUIElement::AttributesOfColumnHeadersCallback(
    const CppArgumentList& args, CppVariant* result) {
  result->SetNull();
}

void AccessibilityUIElement::AttributesOfRowHeadersCallback(
    const CppArgumentList& args, CppVariant* result) {
  result->SetNull();
}

void AccessibilityUIElement::AttributesOfColumnsCallback(
    const CppArgumentList& args, CppVariant* result) {
  result->SetNull();
}

void AccessibilityUIElement::AttributesOfRowsCallback(
    const CppArgumentList& args, CppVariant* result) {
  result->SetNull();
}

void AccessibilityUIElement::AttributesOfVisibleCellsCallback(
    const CppArgumentList& args, CppVariant* result) {
  result->SetNull();
}

void AccessibilityUIElement::AttributesOfHeaderCallback(
    const CppArgumentList& args, CppVariant* result) {
  result->SetNull();
}

void AccessibilityUIElement::IndexInTableCallback(
    const CppArgumentList& args, CppVariant* result) {
  result->SetNull();
}

void AccessibilityUIElement::RowIndexRangeCallback(
    const CppArgumentList& args, CppVariant* result) {
  result->SetNull();
}

void AccessibilityUIElement::ColumnIndexRangeCallback(
    const CppArgumentList& args, CppVariant* result) {
  result->SetNull();
}

void AccessibilityUIElement::CellForColumnAndRowCallback(
    const CppArgumentList& args, CppVariant* result) {
  result->SetNull();
}

void AccessibilityUIElement::TitleUIElementCallback(
    const CppArgumentList& args, CppVariant* result) {
  result->SetNull();
}

void AccessibilityUIElement::SetSelectedTextRangeCallback(
    const CppArgumentList& args, CppVariant* result) {
  result->SetNull();
}

void AccessibilityUIElement::AttributeValueCallback(
    const CppArgumentList& args, CppVariant* result) {
  result->SetNull();
}

void AccessibilityUIElement::IsAttributeSettableCallback(
    const CppArgumentList& args, CppVariant* result) {
  result->SetNull();
}

void AccessibilityUIElement::IsActionSupportedCallback(
    const CppArgumentList& args, CppVariant* result) {
  // This one may be really hard to implement.
  // Not exposed by AccessibilityObject.
  result->SetNull();
}

void AccessibilityUIElement::ParentElementCallback(
    const CppArgumentList& args, CppVariant* result) {
  result->SetNull();
}

void AccessibilityUIElement::IncrementCallback(
    const CppArgumentList& args, CppVariant* result) {
  result->SetNull();
}

void AccessibilityUIElement::DecrementCallback(
    const CppArgumentList& args, CppVariant* result) {
  result->SetNull();
}

void AccessibilityUIElement::FallbackCallback(const CppArgumentList &args,
                                              CppVariant *result) {
  // TODO(dglazkov): Implement this.
  result->SetNull();
}

void AccessibilityUIElement::ChildrenCountGetterCallback(CppVariant* result) {
  int count = 1;  // Root object always has only one child, the WebView.
  if (!IsRoot())
    count = AXObject(accessibility_object())->children().size();
  result->Set(count);
}

void AccessibilityUIElement::DescriptionGetterCallback(CppVariant *result) {
  result->Set(GetDescription());
}

void AccessibilityUIElement::IsEnabledGetterCallback(CppVariant* result) {
  result->Set(AXObject(accessibility_object())->isEnabled());
}

void AccessibilityUIElement::RoleGetterCallback(CppVariant* result) {
  result->Set(RoleToString(AXObject(accessibility_object())->roleValue()));
}

void AccessibilityUIElement::TitleGetterCallback(CppVariant* result) {
  result->Set(GetTitle());
}


RootAccessibilityUIElement::RootAccessibilityUIElement(
    const WebKit::WebAccessibilityObject &object,
    Factory *factory) : AccessibilityUIElement(object, factory) { }

AccessibilityUIElement* RootAccessibilityUIElement::GetChildAtIndex(
    unsigned index) {
  if (index != 0)
    return NULL;

  return factory()->Create(accessibility_object());
}


AccessibilityUIElementList ::~AccessibilityUIElementList() {
  Clear();
}

void AccessibilityUIElementList::Clear() {
  for (ElementList::iterator i = elements_.begin(); i != elements_.end(); ++i)
    delete (*i);
  elements_.clear();
}

AccessibilityUIElement* AccessibilityUIElementList::Create(
    const WebAccessibilityObject& object) {
  AccessibilityUIElement* element = new AccessibilityUIElement(object, this);
  elements_.push_back(element);
  return element;
}

AccessibilityUIElement* AccessibilityUIElementList::CreateRoot(
    const WebAccessibilityObject& object) {
  AccessibilityUIElement* element =
      new RootAccessibilityUIElement(object, this);
  elements_.push_back(element);
  return element;
}
