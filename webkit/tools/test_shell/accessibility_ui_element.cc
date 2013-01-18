// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebCString.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebAccessibilityObject.h"
#include "webkit/tools/test_shell/accessibility_ui_element.h"

using WebKit::WebCString;
using WebKit::WebString;
using WebKit::WebAccessibilityObject;
using WebKit::WebAccessibilityRole;
using webkit_glue::CppArgumentList;
using webkit_glue::CppVariant;

namespace {

// Map role value to string, matching Safari/Mac platform implementation to
// avoid rebaselining layout tests.
static std::string RoleToString(WebAccessibilityRole role) {
  std::string result = "AXRole: AX";
  switch (role) {
    case WebKit::WebAccessibilityRoleButton:
      return result.append("Button");
    case WebKit::WebAccessibilityRoleRadioButton:
      return result.append("RadioButton");
    case WebKit::WebAccessibilityRoleCheckBox:
      return result.append("CheckBox");
    case WebKit::WebAccessibilityRoleSlider:
      return result.append("Slider");
    case WebKit::WebAccessibilityRoleTabGroup:
      return result.append("TabGroup");
    case WebKit::WebAccessibilityRoleTextField:
      return result.append("TextField");
    case WebKit::WebAccessibilityRoleStaticText:
      return result.append("StaticText");
    case WebKit::WebAccessibilityRoleTextArea:
      return result.append("TextArea");
    case WebKit::WebAccessibilityRoleScrollArea:
      return result.append("ScrollArea");
    case WebKit::WebAccessibilityRolePopUpButton:
      return result.append("PopUpButton");
    case WebKit::WebAccessibilityRoleMenuButton:
      return result.append("MenuButton");
    case WebKit::WebAccessibilityRoleTable:
      return result.append("Table");
    case WebKit::WebAccessibilityRoleApplication:
      return result.append("Application");
    case WebKit::WebAccessibilityRoleGroup:
      return result.append("Group");
    case WebKit::WebAccessibilityRoleRadioGroup:
      return result.append("RadioGroup");
    case WebKit::WebAccessibilityRoleList:
      return result.append("List");
    case WebKit::WebAccessibilityRoleScrollBar:
      return result.append("ScrollBar");
    case WebKit::WebAccessibilityRoleValueIndicator:
      return result.append("ValueIndicator");
    case WebKit::WebAccessibilityRoleImage:
      return result.append("Image");
    case WebKit::WebAccessibilityRoleMenuBar:
      return result.append("MenuBar");
    case WebKit::WebAccessibilityRoleMenu:
      return result.append("Menu");
    case WebKit::WebAccessibilityRoleMenuItem:
      return result.append("MenuItem");
    case WebKit::WebAccessibilityRoleColumn:
      return result.append("Column");
    case WebKit::WebAccessibilityRoleRow:
      return result.append("Row");
    case WebKit::WebAccessibilityRoleToolbar:
      return result.append("Toolbar");
    case WebKit::WebAccessibilityRoleBusyIndicator:
      return result.append("BusyIndicator");
    case WebKit::WebAccessibilityRoleProgressIndicator:
      return result.append("ProgressIndicator");
    case WebKit::WebAccessibilityRoleWindow:
      return result.append("Window");
    case WebKit::WebAccessibilityRoleDrawer:
      return result.append("Drawer");
    case WebKit::WebAccessibilityRoleSystemWide:
      return result.append("SystemWide");
    case WebKit::WebAccessibilityRoleOutline:
      return result.append("Outline");
    case WebKit::WebAccessibilityRoleIncrementor:
      return result.append("Incrementor");
    case WebKit::WebAccessibilityRoleBrowser:
      return result.append("Browser");
    case WebKit::WebAccessibilityRoleComboBox:
      return result.append("ComboBox");
    case WebKit::WebAccessibilityRoleSplitGroup:
      return result.append("SplitGroup");
    case WebKit::WebAccessibilityRoleSplitter:
      return result.append("Splitter");
    case WebKit::WebAccessibilityRoleColorWell:
      return result.append("ColorWell");
    case WebKit::WebAccessibilityRoleGrowArea:
      return result.append("GrowArea");
    case WebKit::WebAccessibilityRoleSheet:
      return result.append("Sheet");
    case WebKit::WebAccessibilityRoleHelpTag:
      return result.append("HelpTag");
    case WebKit::WebAccessibilityRoleMatte:
      return result.append("Matte");
    case WebKit::WebAccessibilityRoleRuler:
      return result.append("Ruler");
    case WebKit::WebAccessibilityRoleRulerMarker:
      return result.append("RulerMarker");
    case WebKit::WebAccessibilityRoleLink:
      return result.append("Link");
    case WebKit::WebAccessibilityRoleDisclosureTriangle:
      return result.append("DisclosureTriangle");
    case WebKit::WebAccessibilityRoleGrid:
      return result.append("Grid");
    case WebKit::WebAccessibilityRoleCell:
      return result.append("Cell");
    case WebKit::WebAccessibilityRoleColumnHeader:
      return result.append("ColumnHeader");
    case WebKit::WebAccessibilityRoleRowHeader:
      return result.append("RowHeader");
    case WebKit::WebAccessibilityRoleWebCoreLink:
      // Maps to Link role.
      return result.append("Link");
    case WebKit::WebAccessibilityRoleImageMapLink:
      return result.append("ImageMapLink");
    case WebKit::WebAccessibilityRoleImageMap:
      return result.append("ImageMap");
    case WebKit::WebAccessibilityRoleListMarker:
      return result.append("ListMarker");
    case WebKit::WebAccessibilityRoleWebArea:
      return result.append("WebArea");
    case WebKit::WebAccessibilityRoleHeading:
      return result.append("Heading");
    case WebKit::WebAccessibilityRoleListBox:
      return result.append("ListBox");
    case WebKit::WebAccessibilityRoleListBoxOption:
      return result.append("ListBoxOption");
    case WebKit::WebAccessibilityRoleTableHeaderContainer:
      return result.append("TableHeaderContainer");
    case WebKit::WebAccessibilityRoleDefinitionListTerm:
      return result.append("DefinitionListTerm");
    case WebKit::WebAccessibilityRoleDefinitionListDefinition:
      return result.append("DefinitionListDefinition");
    case WebKit::WebAccessibilityRoleAnnotation:
      return result.append("Annotation");
    case WebKit::WebAccessibilityRoleSliderThumb:
      return result.append("SliderThumb");
    case WebKit::WebAccessibilityRoleLandmarkApplication:
      return result.append("LandmarkApplication");
    case WebKit::WebAccessibilityRoleLandmarkBanner:
      return result.append("LandmarkBanner");
    case WebKit::WebAccessibilityRoleLandmarkComplementary:
      return result.append("LandmarkComplementary");
    case WebKit::WebAccessibilityRoleLandmarkContentInfo:
      return result.append("LandmarkContentInfo");
    case WebKit::WebAccessibilityRoleLandmarkMain:
      return result.append("LandmarkMain");
    case WebKit::WebAccessibilityRoleLandmarkNavigation:
      return result.append("LandmarkNavigation");
    case WebKit::WebAccessibilityRoleLandmarkSearch:
      return result.append("LandmarkSearch");
    case WebKit::WebAccessibilityRoleApplicationLog:
      return result.append("ApplicationLog");
    case WebKit::WebAccessibilityRoleApplicationMarquee:
      return result.append("ApplicationMarquee");
    case WebKit::WebAccessibilityRoleApplicationStatus:
      return result.append("ApplicationStatus");
    case WebKit::WebAccessibilityRoleApplicationTimer:
      return result.append("ApplicationTimer");
    case WebKit::WebAccessibilityRoleDocument:
      return result.append("Document");
    case WebKit::WebAccessibilityRoleDocumentArticle:
      return result.append("DocumentArticle");
    case WebKit::WebAccessibilityRoleDocumentNote:
      return result.append("DocumentNote");
    case WebKit::WebAccessibilityRoleDocumentRegion:
      return result.append("DocumentRegion");
    case WebKit::WebAccessibilityRoleUserInterfaceTooltip:
      return result.append("UserInterfaceTooltip");
    default:
      // Also matches WebAccessibilityRoleUnknown.
      return result.append("Unknown");
  }
}

std::string GetDescription(const WebAccessibilityObject& object) {
  std::string description = object.accessibilityDescription().utf8();
  return description.insert(0, "AXDescription: ");
}

std::string GetRole(const WebAccessibilityObject& object) {
  return RoleToString(object.roleValue());
}

std::string GetTitle(const WebAccessibilityObject& object) {
  std::string title = object.title().utf8();
  return title.insert(0, "AXTitle: ");
}

std::string GetAttributes(const WebAccessibilityObject& object) {
  // TODO(dglazkov): Concatenate all attributes of the AccessibilityObject.
  std::string attributes(GetTitle(object));
  attributes.append("\n");
  attributes.append(GetRole(object));
  attributes.append("\n");
  attributes.append(GetDescription(object));
  return attributes;
}


// Collects attributes into a string, delimited by dashes. Used by all methods
// that output lists of attributes: AttributesOfLinkedUIElementsCallback,
// AttributesOfChildrenCallback, etc.
class AttributesCollector {
 public:
  void CollectAttributes(const WebAccessibilityObject& object) {
    attributes_.append("\n------------\n");
    attributes_.append(GetAttributes(object));
  }
  std::string attributes() const { return attributes_; }

private:
  std::string attributes_;
};

}  // namespace

AccessibilityUIElement::AccessibilityUIElement(
    const WebKit::WebAccessibilityObject& object,
    Factory* factory)
    : accessibility_object_(object),
      factory_(factory) {

  DCHECK(factory);

  BindCallback("allAttributes",
               base::Bind(&AccessibilityUIElement::AllAttributesCallback,
                          base::Unretained(this)));
  BindCallback(
      "attributesOfLinkedUIElements",
      base::Bind(&AccessibilityUIElement::AttributesOfLinkedUIElementsCallback,
                 base::Unretained(this)));
  BindCallback(
      "attributesOfDocumentLinks",
      base::Bind(&AccessibilityUIElement::AttributesOfDocumentLinksCallback,
                 base::Unretained(this)));
  BindCallback(
      "attributesOfChildren",
      base::Bind(&AccessibilityUIElement::AttributesOfChildrenCallback,
                 base::Unretained(this)));
  BindCallback(
      "parameterizedAttributeNames",
      base::Bind(&AccessibilityUIElement::ParametrizedAttributeNamesCallback,
                 base::Unretained(this)));
  BindCallback("lineForIndex",
               base::Bind(&AccessibilityUIElement::LineForIndexCallback,
                          base::Unretained(this)));
  BindCallback("boundsForRange",
               base::Bind(&AccessibilityUIElement::BoundsForRangeCallback,
                          base::Unretained(this)));
  BindCallback("stringForRange",
               base::Bind(&AccessibilityUIElement::StringForRangeCallback,
                          base::Unretained(this)));
  BindCallback("childAtIndex",
               base::Bind(&AccessibilityUIElement::ChildAtIndexCallback,
                          base::Unretained(this)));
  BindCallback("elementAtPoint",
               base::Bind(&AccessibilityUIElement::ElementAtPointCallback,
                          base::Unretained(this)));
  BindCallback(
      "attributesOfColumnHeaders",
      base::Bind(&AccessibilityUIElement::AttributesOfColumnHeadersCallback,
                 base::Unretained(this)));
  BindCallback(
      "attributesOfRowHeaders",
      base::Bind(&AccessibilityUIElement::AttributesOfRowHeadersCallback,
                 base::Unretained(this)));
  BindCallback("attributesOfColumns",
               base::Bind(&AccessibilityUIElement::AttributesOfColumnsCallback,
                          base::Unretained(this)));
  BindCallback("attributesOfRows",
               base::Bind(&AccessibilityUIElement::AttributesOfRowsCallback,
                          base::Unretained(this)));
  BindCallback(
      "attributesOfVisibleCells",
      base::Bind(&AccessibilityUIElement::AttributesOfVisibleCellsCallback,
                 base::Unretained(this)));
  BindCallback("attributesOfHeader",
               base::Bind(&AccessibilityUIElement::AttributesOfHeaderCallback,
                          base::Unretained(this)));
  BindCallback("indexInTable",
               base::Bind(&AccessibilityUIElement::IndexInTableCallback,
                          base::Unretained(this)));
  BindCallback("rowIndexRange",
               base::Bind(&AccessibilityUIElement::RowIndexRangeCallback,
                          base::Unretained(this)));
  BindCallback("columnIndexRange",
               base::Bind(&AccessibilityUIElement::ColumnIndexRangeCallback,
                          base::Unretained(this)));
  BindCallback("cellForColumnAndRow",
               base::Bind(&AccessibilityUIElement::CellForColumnAndRowCallback,
                          base::Unretained(this)));
  BindCallback("titleUIElement",
               base::Bind(&AccessibilityUIElement::TitleUIElementCallback,
                          base::Unretained(this)));
  BindCallback("setSelectedTextRange",
               base::Bind(&AccessibilityUIElement::SetSelectedTextRangeCallback,
                          base::Unretained(this)));
  BindCallback("attributeValue",
               base::Bind(&AccessibilityUIElement::AttributeValueCallback,
                          base::Unretained(this)));
  BindCallback("isAttributeSettable",
               base::Bind(&AccessibilityUIElement::IsAttributeSettableCallback,
                          base::Unretained(this)));
  BindCallback("isActionSupported",
               base::Bind(&AccessibilityUIElement::IsActionSupportedCallback,
                          base::Unretained(this)));
  BindCallback("parentElement",
               base::Bind(&AccessibilityUIElement::ParentElementCallback,
                          base::Unretained(this)));
  BindCallback("increment",
               base::Bind(&AccessibilityUIElement::IncrementCallback,
                          base::Unretained(this)));
  BindCallback("decrement",
               base::Bind(&AccessibilityUIElement::DecrementCallback,
                          base::Unretained(this)));

  BindGetterCallback("role",
                     base::Bind(&AccessibilityUIElement::RoleGetterCallback,
                                base::Unretained(this)));
  BindProperty("subrole", &subrole_);
  BindGetterCallback("title",
                     base::Bind(&AccessibilityUIElement::TitleGetterCallback,
                                base::Unretained(this)));
  BindGetterCallback(
      "description",
      base::Bind(&AccessibilityUIElement::DescriptionGetterCallback,
                 base::Unretained(this)));
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
  BindGetterCallback(
      "childrenCount",
      base::Bind(&AccessibilityUIElement::ChildrenCountGetterCallback,
                 base::Unretained(this)));
  BindProperty("insertionPointLineNumber", &insertion_point_line_number_);
  BindProperty("selectedTextRange", &selected_text_range);
  BindGetterCallback(
      "isEnabled",
      base::Bind(&AccessibilityUIElement::IsEnabledGetterCallback,
                 base::Unretained(this)));
  BindProperty("isRequired", &is_required_);
  BindGetterCallback(
      "isSelected",
      base::Bind(&AccessibilityUIElement::IsSelectedGetterCallback,
                 base::Unretained(this)));
  BindProperty("valueDescription", &value_description_);

  BindFallbackCallback(base::Bind(&AccessibilityUIElement::FallbackCallback,
                                  base::Unretained(this)));
}

AccessibilityUIElement::~AccessibilityUIElement() {}

AccessibilityUIElement* AccessibilityUIElement::GetChildAtIndex(
    unsigned index) {
  return factory_->Create(accessibility_object().childAt(index));
}

bool AccessibilityUIElement::IsRoot() const {
  return false;
}

void AccessibilityUIElement::AllAttributesCallback(
    const CppArgumentList& args, CppVariant* result) {
  result->Set(GetAttributes(accessibility_object()));
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
  AttributesCollector collector;
  unsigned size = accessibility_object().childCount();
  for(unsigned i = 0; i < size; ++i) {
    collector.CollectAttributes(accessibility_object().childAt(i));
  }
  result->Set(collector.attributes());
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
  if (args.empty() || !args[0].isNumber()) {
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
  if (args.size() < 1 && !args[0].isString()) {
    result->SetNull();
    return;
  }

  std::string attribute = args[0].ToString();
  bool settable = false;
  if (attribute == "AXValue") {
    settable = accessibility_object().canSetValueAttribute();
  }
  result->Set(settable);
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
    count = accessibility_object().childCount();
  result->Set(count);
}

void AccessibilityUIElement::DescriptionGetterCallback(CppVariant *result) {
  result->Set(GetDescription(accessibility_object()));
}

void AccessibilityUIElement::IsEnabledGetterCallback(CppVariant* result) {
  result->Set(accessibility_object().isEnabled());
}

void AccessibilityUIElement::IsSelectedGetterCallback(CppVariant* result) {
  result->SetNull();
}

void AccessibilityUIElement::RoleGetterCallback(CppVariant* result) {
  result->Set(GetRole(accessibility_object()));
}

void AccessibilityUIElement::TitleGetterCallback(CppVariant* result) {
  result->Set(GetTitle(accessibility_object()));
}

RootAccessibilityUIElement::RootAccessibilityUIElement(
    const WebKit::WebAccessibilityObject &object,
    Factory *factory)
    : AccessibilityUIElement(object, factory) {}

RootAccessibilityUIElement::~RootAccessibilityUIElement() {}

AccessibilityUIElement* RootAccessibilityUIElement::GetChildAtIndex(
    unsigned index) {
  if (index != 0)
    return NULL;

  return factory()->Create(accessibility_object());
}

bool RootAccessibilityUIElement::IsRoot() const {
  return true;
}

AccessibilityUIElementList::AccessibilityUIElementList() {}

AccessibilityUIElementList::~AccessibilityUIElementList() {
  Clear();
}

void AccessibilityUIElementList::Clear() {
  for (ElementList::iterator i = elements_.begin(); i != elements_.end(); ++i)
    delete (*i);
  elements_.clear();
}

AccessibilityUIElement* AccessibilityUIElementList::Create(
    const WebAccessibilityObject& object) {
  if (object.isNull())
    return NULL;

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
