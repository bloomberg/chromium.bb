/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "AccessibilityUIElementChromium.h"

#include "TestCommon.h"
#include "public/platform/WebCString.h"
#include "public/platform/WebPoint.h"
#include "public/platform/WebRect.h"
#include "public/platform/WebString.h"
#include "public/web/WebAXObject.h"

using namespace WebKit;
using namespace std;

namespace WebTestRunner {

namespace {

// Map role value to string, matching Safari/Mac platform implementation to
// avoid rebaselining layout tests.
string roleToString(WebAXRole role)
{
    string result = "AXRole: AX";
    switch (role) {
    case WebAXRoleAlertDialog:
        return result.append("AlertDialog");
    case WebAXRoleAlert:
        return result.append("Alert");
    case WebAXRoleAnnotation:
        return result.append("Annotation");
    case WebAXRoleApplication:
        return result.append("Application");
    case WebAXRoleArticle:
        return result.append("Article");
    case WebAXRoleBanner:
        return result.append("Banner");
    case WebAXRoleBrowser:
        return result.append("Browser");
    case WebAXRoleBusyIndicator:
        return result.append("BusyIndicator");
    case WebAXRoleButton:
        return result.append("Button");
    case WebAXRoleCanvas:
        return result.append("Canvas");
    case WebAXRoleCell:
        return result.append("Cell");
    case WebAXRoleCheckBox:
        return result.append("CheckBox");
    case WebAXRoleColorWell:
        return result.append("ColorWell");
    case WebAXRoleColumnHeader:
        return result.append("ColumnHeader");
    case WebAXRoleColumn:
        return result.append("Column");
    case WebAXRoleComboBox:
        return result.append("ComboBox");
    case WebAXRoleComplementary:
        return result.append("Complementary");
    case WebAXRoleContentInfo:
        return result.append("ContentInfo");
    case WebAXRoleDefinition:
        return result.append("Definition");
    case WebAXRoleDescriptionListDetail:
        return result.append("DescriptionListDetail");
    case WebAXRoleDescriptionListTerm:
        return result.append("DescriptionListTerm");
    case WebAXRoleDialog:
        return result.append("Dialog");
    case WebAXRoleDirectory:
        return result.append("Directory");
    case WebAXRoleDisclosureTriangle:
        return result.append("DisclosureTriangle");
    case WebAXRoleDiv:
        return result.append("Div");
    case WebAXRoleDocument:
        return result.append("Document");
    case WebAXRoleDrawer:
        return result.append("Drawer");
    case WebAXRoleEditableText:
        return result.append("EditableText");
    case WebAXRoleFooter:
        return result.append("Footer");
    case WebAXRoleForm:
        return result.append("Form");
    case WebAXRoleGrid:
        return result.append("Grid");
    case WebAXRoleGroup:
        return result.append("Group");
    case WebAXRoleGrowArea:
        return result.append("GrowArea");
    case WebAXRoleHeading:
        return result.append("Heading");
    case WebAXRoleHelpTag:
        return result.append("HelpTag");
    case WebAXRoleHorizontalRule:
        return result.append("HorizontalRule");
    case WebAXRoleIgnored:
        return result.append("Ignored");
    case WebAXRoleImageMapLink:
        return result.append("ImageMapLink");
    case WebAXRoleImageMap:
        return result.append("ImageMap");
    case WebAXRoleImage:
        return result.append("Image");
    case WebAXRoleIncrementor:
        return result.append("Incrementor");
    case WebAXRoleLabel:
        return result.append("Label");
    case WebAXRoleLegend:
        return result.append("Legend");
    case WebAXRoleLink:
        return result.append("Link");
    case WebAXRoleListBoxOption:
        return result.append("ListBoxOption");
    case WebAXRoleListBox:
        return result.append("ListBox");
    case WebAXRoleListItem:
        return result.append("ListItem");
    case WebAXRoleListMarker:
        return result.append("ListMarker");
    case WebAXRoleList:
        return result.append("List");
    case WebAXRoleLog:
        return result.append("Log");
    case WebAXRoleMain:
        return result.append("Main");
    case WebAXRoleMarquee:
        return result.append("Marquee");
    case WebAXRoleMathElement:
        return result.append("MathElement");
    case WebAXRoleMath:
        return result.append("Math");
    case WebAXRoleMatte:
        return result.append("Matte");
    case WebAXRoleMenuBar:
        return result.append("MenuBar");
    case WebAXRoleMenuButton:
        return result.append("MenuButton");
    case WebAXRoleMenuItem:
        return result.append("MenuItem");
    case WebAXRoleMenuListOption:
        return result.append("MenuListOption");
    case WebAXRoleMenuListPopup:
        return result.append("MenuListPopup");
    case WebAXRoleMenu:
        return result.append("Menu");
    case WebAXRoleNavigation:
        return result.append("Navigation");
    case WebAXRoleNote:
        return result.append("Note");
    case WebAXRoleOutline:
        return result.append("Outline");
    case WebAXRoleParagraph:
        return result.append("Paragraph");
    case WebAXRolePopUpButton:
        return result.append("PopUpButton");
    case WebAXRolePresentational:
        return result.append("Presentational");
    case WebAXRoleProgressIndicator:
        return result.append("ProgressIndicator");
    case WebAXRoleRadioButton:
        return result.append("RadioButton");
    case WebAXRoleRadioGroup:
        return result.append("RadioGroup");
    case WebAXRoleRegion:
        return result.append("Region");
    case WebAXRoleRootWebArea:
        return result.append("RootWebArea");
    case WebAXRoleRowHeader:
        return result.append("RowHeader");
    case WebAXRoleRow:
        return result.append("Row");
    case WebAXRoleRulerMarker:
        return result.append("RulerMarker");
    case WebAXRoleRuler:
        return result.append("Ruler");
    case WebAXRoleSVGRoot:
        return result.append("SVGRoot");
    case WebAXRoleScrollArea:
        return result.append("ScrollArea");
    case WebAXRoleScrollBar:
        return result.append("ScrollBar");
    case WebAXRoleSeamlessWebArea:
        return result.append("SeamlessWebArea");
    case WebAXRoleSearch:
        return result.append("Search");
    case WebAXRoleSheet:
        return result.append("Sheet");
    case WebAXRoleSlider:
        return result.append("Slider");
    case WebAXRoleSliderThumb:
        return result.append("SliderThumb");
    case WebAXRoleSpinButtonPart:
        return result.append("SpinButtonPart");
    case WebAXRoleSpinButton:
        return result.append("SpinButton");
    case WebAXRoleSplitGroup:
        return result.append("SplitGroup");
    case WebAXRoleSplitter:
        return result.append("Splitter");
    case WebAXRoleStaticText:
        return result.append("StaticText");
    case WebAXRoleStatus:
        return result.append("Status");
    case WebAXRoleSystemWide:
        return result.append("SystemWide");
    case WebAXRoleTabGroup:
        return result.append("TabGroup");
    case WebAXRoleTabList:
        return result.append("TabList");
    case WebAXRoleTabPanel:
        return result.append("TabPanel");
    case WebAXRoleTab:
        return result.append("Tab");
    case WebAXRoleTableHeaderContainer:
        return result.append("TableHeaderContainer");
    case WebAXRoleTable:
        return result.append("Table");
    case WebAXRoleTextArea:
        return result.append("TextArea");
    case WebAXRoleTextField:
        return result.append("TextField");
    case WebAXRoleTimer:
        return result.append("Timer");
    case WebAXRoleToggleButton:
        return result.append("ToggleButton");
    case WebAXRoleToolbar:
        return result.append("Toolbar");
    case WebAXRoleTreeGrid:
        return result.append("TreeGrid");
    case WebAXRoleTreeItem:
        return result.append("TreeItem");
    case WebAXRoleTree:
        return result.append("Tree");
    case WebAXRoleUnknown:
        return result.append("Unknown");
    case WebAXRoleUserInterfaceTooltip:
        return result.append("UserInterfaceTooltip");
    case WebAXRoleValueIndicator:
        return result.append("ValueIndicator");
    case WebAXRoleWebArea:
        return result.append("WebArea");
    case WebAXRoleWindow:
        return result.append("Window");
    default:
        return result.append("Unknown");
    }
}

string getDescription(const WebAXObject& object)
{
    string description = object.accessibilityDescription().utf8();
    return description.insert(0, "AXDescription: ");
}

string getHelpText(const WebAXObject& object)
{
    string helpText = object.helpText().utf8();
    return helpText.insert(0, "AXHelp: ");
}

string getStringValue(const WebAXObject& object)
{
    string value;
    if (object.role() == WebAXRoleColorWell) {
        int r, g, b;
        char buffer[100];
        object.colorValue(r, g, b);
        snprintf(buffer, sizeof(buffer), "rgb %7.5f %7.5f %7.5f 1", r / 255., g / 255., b / 255.);
        value = buffer;
    } else
        value = object.stringValue().utf8();
    return value.insert(0, "AXValue: ");
}

string getRole(const WebAXObject& object)
{
    string roleString = roleToString(object.role());

    // Special-case canvas with fallback content because Chromium wants to
    // treat this as essentially a separate role that it can map differently depending
    // on the platform.
    if (object.role() == WebAXRoleCanvas && object.canvasHasFallbackContent())
        roleString += "WithFallbackContent";

    return roleString;
}

string getTitle(const WebAXObject& object)
{
    string title = object.title().utf8();
    return title.insert(0, "AXTitle: ");
}

string getOrientation(const WebAXObject& object)
{
    if (object.isVertical())
        return "AXOrientation: AXVerticalOrientation";

    return "AXOrientation: AXHorizontalOrientation";
}

string getValueDescription(const WebAXObject& object)
{
    string valueDescription = object.valueDescription().utf8();
    return valueDescription.insert(0, "AXValueDescription: ");
}

string getAttributes(const WebAXObject& object)
{
    // FIXME: Concatenate all attributes of the AccessibilityObject.
    string attributes(getTitle(object));
    attributes.append("\n");
    attributes.append(getRole(object));
    attributes.append("\n");
    attributes.append(getDescription(object));
    return attributes;
}


// Collects attributes into a string, delimited by dashes. Used by all methods
// that output lists of attributes: attributesOfLinkedUIElementsCallback,
// AttributesOfChildrenCallback, etc.
class AttributesCollector {
public:
    void collectAttributes(const WebAXObject& object)
    {
        m_attributes.append("\n------------\n");
        m_attributes.append(getAttributes(object));
    }

    string attributes() const { return m_attributes; }

private:
    string m_attributes;
};

}

AccessibilityUIElement::AccessibilityUIElement(const WebAXObject& object, Factory* factory)
    : m_accessibilityObject(object)
    , m_factory(factory)
{

    WEBKIT_ASSERT(factory);

    //
    // Properties
    //

    bindProperty("role", &AccessibilityUIElement::roleGetterCallback);
    bindProperty("title", &AccessibilityUIElement::titleGetterCallback);
    bindProperty("description", &AccessibilityUIElement::descriptionGetterCallback);
    bindProperty("helpText", &AccessibilityUIElement::helpTextGetterCallback);
    bindProperty("stringValue", &AccessibilityUIElement::stringValueGetterCallback);
    bindProperty("x", &AccessibilityUIElement::xGetterCallback);
    bindProperty("y", &AccessibilityUIElement::yGetterCallback);
    bindProperty("width", &AccessibilityUIElement::widthGetterCallback);
    bindProperty("height", &AccessibilityUIElement::heightGetterCallback);
    bindProperty("intValue", &AccessibilityUIElement::intValueGetterCallback);
    bindProperty("minValue", &AccessibilityUIElement::minValueGetterCallback);
    bindProperty("maxValue", &AccessibilityUIElement::maxValueGetterCallback);
    bindProperty("valueDescription", &AccessibilityUIElement::valueDescriptionGetterCallback);
    bindProperty("childrenCount", &AccessibilityUIElement::childrenCountGetterCallback);
    bindProperty("insertionPointLineNumber", &AccessibilityUIElement::insertionPointLineNumberGetterCallback);
    bindProperty("selectedTextRange", &AccessibilityUIElement::selectedTextRangeGetterCallback);
    bindProperty("isEnabled", &AccessibilityUIElement::isEnabledGetterCallback);
    bindProperty("isRequired", &AccessibilityUIElement::isRequiredGetterCallback);
    bindProperty("isFocused", &AccessibilityUIElement::isFocusedGetterCallback);
    bindProperty("isFocusable", &AccessibilityUIElement::isFocusableGetterCallback);
    bindProperty("isSelected", &AccessibilityUIElement::isSelectedGetterCallback);
    bindProperty("isSelectable", &AccessibilityUIElement::isSelectableGetterCallback);
    bindProperty("isMultiSelectable", &AccessibilityUIElement::isMultiSelectableGetterCallback);
    bindProperty("isSelectedOptionActive", &AccessibilityUIElement::isSelectedOptionActiveGetterCallback);
    bindProperty("isExpanded", &AccessibilityUIElement::isExpandedGetterCallback);
    bindProperty("isChecked", &AccessibilityUIElement::isCheckedGetterCallback);
    bindProperty("isVisible", &AccessibilityUIElement::isVisibleGetterCallback);
    bindProperty("isOffScreen", &AccessibilityUIElement::isOffScreenGetterCallback);
    bindProperty("isCollapsed", &AccessibilityUIElement::isCollapsedGetterCallback);
    bindProperty("hasPopup", &AccessibilityUIElement::hasPopupGetterCallback);
    bindProperty("isValid", &AccessibilityUIElement::isValidGetterCallback);
    bindProperty("isReadOnly", &AccessibilityUIElement::isReadOnlyGetterCallback);
    bindProperty("orientation", &AccessibilityUIElement::orientationGetterCallback);
    bindProperty("clickPointX", &AccessibilityUIElement::clickPointXGetterCallback);
    bindProperty("clickPointY", &AccessibilityUIElement::clickPointYGetterCallback);
    bindProperty("rowCount", &AccessibilityUIElement::rowCountGetterCallback);
    bindProperty("columnCount", &AccessibilityUIElement::columnCountGetterCallback);
    bindProperty("isClickable", &AccessibilityUIElement::isClickableGetterCallback);

    //
    // Methods
    //

    bindMethod("allAttributes", &AccessibilityUIElement::allAttributesCallback);
    bindMethod("attributesOfLinkedUIElements", &AccessibilityUIElement::attributesOfLinkedUIElementsCallback);
    bindMethod("attributesOfDocumentLinks", &AccessibilityUIElement::attributesOfDocumentLinksCallback);
    bindMethod("attributesOfChildren", &AccessibilityUIElement::attributesOfChildrenCallback);
    bindMethod("lineForIndex", &AccessibilityUIElement::lineForIndexCallback);
    bindMethod("boundsForRange", &AccessibilityUIElement::boundsForRangeCallback);
    bindMethod("stringForRange", &AccessibilityUIElement::stringForRangeCallback);
    bindMethod("childAtIndex", &AccessibilityUIElement::childAtIndexCallback);
    bindMethod("elementAtPoint", &AccessibilityUIElement::elementAtPointCallback);
    bindMethod("attributesOfColumnHeaders", &AccessibilityUIElement::attributesOfColumnHeadersCallback);
    bindMethod("attributesOfRowHeaders", &AccessibilityUIElement::attributesOfRowHeadersCallback);
    bindMethod("attributesOfColumns", &AccessibilityUIElement::attributesOfColumnsCallback);
    bindMethod("attributesOfRows", &AccessibilityUIElement::attributesOfRowsCallback);
    bindMethod("attributesOfVisibleCells", &AccessibilityUIElement::attributesOfVisibleCellsCallback);
    bindMethod("attributesOfHeader", &AccessibilityUIElement::attributesOfHeaderCallback);
    bindMethod("tableHeader", &AccessibilityUIElement::tableHeaderCallback);
    bindMethod("indexInTable", &AccessibilityUIElement::indexInTableCallback);
    bindMethod("rowIndexRange", &AccessibilityUIElement::rowIndexRangeCallback);
    bindMethod("columnIndexRange", &AccessibilityUIElement::columnIndexRangeCallback);
    bindMethod("cellForColumnAndRow", &AccessibilityUIElement::cellForColumnAndRowCallback);
    bindMethod("titleUIElement", &AccessibilityUIElement::titleUIElementCallback);
    bindMethod("setSelectedTextRange", &AccessibilityUIElement::setSelectedTextRangeCallback);
    bindMethod("attributeValue", &AccessibilityUIElement::attributeValueCallback);
    bindMethod("isAttributeSettable", &AccessibilityUIElement::isAttributeSettableCallback);
    bindMethod("isPressActionSupported", &AccessibilityUIElement::isPressActionSupportedCallback);
    bindMethod("isIncrementActionSupported", &AccessibilityUIElement::isIncrementActionSupportedCallback);
    bindMethod("isDecrementActionSupported", &AccessibilityUIElement::isDecrementActionSupportedCallback);
    bindMethod("parentElement", &AccessibilityUIElement::parentElementCallback);
    bindMethod("increment", &AccessibilityUIElement::incrementCallback);
    bindMethod("decrement", &AccessibilityUIElement::decrementCallback);
    bindMethod("showMenu", &AccessibilityUIElement::showMenuCallback);
    bindMethod("press", &AccessibilityUIElement::pressCallback);
    bindMethod("isEqual", &AccessibilityUIElement::isEqualCallback);
    bindMethod("addNotificationListener", &AccessibilityUIElement::addNotificationListenerCallback);
    bindMethod("removeNotificationListener", &AccessibilityUIElement::removeNotificationListenerCallback);
    bindMethod("takeFocus", &AccessibilityUIElement::takeFocusCallback);
    bindMethod("scrollToMakeVisible", &AccessibilityUIElement::scrollToMakeVisibleCallback);
    bindMethod("scrollToMakeVisibleWithSubFocus", &AccessibilityUIElement::scrollToMakeVisibleWithSubFocusCallback);
    bindMethod("scrollToGlobalPoint", &AccessibilityUIElement::scrollToGlobalPointCallback);

    bindFallbackMethod(&AccessibilityUIElement::fallbackCallback);
}

AccessibilityUIElement* AccessibilityUIElement::getChildAtIndex(unsigned index)
{
    return m_factory->getOrCreate(accessibilityObject().childAt(index));
}

bool AccessibilityUIElement::isEqual(const WebKit::WebAXObject& other)
{
    return accessibilityObject().equals(other);
}

void AccessibilityUIElement::notificationReceived(const char* notificationName)
{
    size_t callbackCount = m_notificationCallbacks.size();
    for (size_t i = 0; i < callbackCount; i++) {
        CppVariant notificationNameArgument;
        notificationNameArgument.set(notificationName);
        CppVariant invokeResult;
        m_notificationCallbacks[i].invokeDefault(&notificationNameArgument, 1, invokeResult);
    }
}

//
// Properties
//

void AccessibilityUIElement::roleGetterCallback(CppVariant* result)
{
    result->set(getRole(accessibilityObject()));
}

void AccessibilityUIElement::titleGetterCallback(CppVariant* result)
{
    result->set(getTitle(accessibilityObject()));
}

void AccessibilityUIElement::descriptionGetterCallback(CppVariant* result)
{
    result->set(getDescription(accessibilityObject()));
}

void AccessibilityUIElement::helpTextGetterCallback(CppVariant* result)
{
    result->set(getHelpText(accessibilityObject()));
}

void AccessibilityUIElement::stringValueGetterCallback(CppVariant* result)
{
    result->set(getStringValue(accessibilityObject()));
}

void AccessibilityUIElement::xGetterCallback(CppVariant* result)
{
    result->set(accessibilityObject().boundingBoxRect().x);
}

void AccessibilityUIElement::yGetterCallback(CppVariant* result)
{
    result->set(accessibilityObject().boundingBoxRect().y);
}

void AccessibilityUIElement::widthGetterCallback(CppVariant* result)
{
    result->set(accessibilityObject().boundingBoxRect().width);
}

void AccessibilityUIElement::heightGetterCallback(CppVariant* result)
{
    result->set(accessibilityObject().boundingBoxRect().height);
}

void AccessibilityUIElement::intValueGetterCallback(CppVariant* result)
{
    if (accessibilityObject().supportsRangeValue())
        result->set(accessibilityObject().valueForRange());
    else if (accessibilityObject().role() == WebAXRoleHeading)
        result->set(accessibilityObject().headingLevel());
    else
        result->set(atoi(accessibilityObject().stringValue().utf8().data()));
}

void AccessibilityUIElement::minValueGetterCallback(CppVariant* result)
{
    result->set(accessibilityObject().minValueForRange());
}

void AccessibilityUIElement::maxValueGetterCallback(CppVariant* result)
{
    result->set(accessibilityObject().maxValueForRange());
}

void AccessibilityUIElement::valueDescriptionGetterCallback(CppVariant* result)
{
    result->set(getValueDescription(accessibilityObject()));
}

void AccessibilityUIElement::childrenCountGetterCallback(CppVariant* result)
{
    int count = 1; // Root object always has only one child, the WebView.
    if (!isRoot())
        count = accessibilityObject().childCount();
    result->set(count);
}

void AccessibilityUIElement::insertionPointLineNumberGetterCallback(CppVariant* result)
{
    if (!accessibilityObject().isFocused()) {
        result->set(-1);
        return;
    }

    int lineNumber = accessibilityObject().selectionEndLineNumber();
    result->set(lineNumber);
}

void AccessibilityUIElement::selectedTextRangeGetterCallback(CppVariant* result)
{
    unsigned selectionStart = accessibilityObject().selectionStart();
    unsigned selectionEnd = accessibilityObject().selectionEnd();
    char buffer[100];
    snprintf(buffer, sizeof(buffer), "{%d, %d}", selectionStart, selectionEnd - selectionStart);

    result->set(std::string(buffer));
}

void AccessibilityUIElement::isEnabledGetterCallback(CppVariant* result)
{
    result->set(accessibilityObject().isEnabled());
}

void AccessibilityUIElement::isRequiredGetterCallback(CppVariant* result)
{
    result->set(accessibilityObject().isRequired());
}

void AccessibilityUIElement::isFocusedGetterCallback(CppVariant* result)
{
    result->set(accessibilityObject().isFocused());
}

void AccessibilityUIElement::isFocusableGetterCallback(CppVariant* result)
{
    result->set(accessibilityObject().canSetFocusAttribute());
}

void AccessibilityUIElement::isSelectedGetterCallback(CppVariant* result)
{
    result->set(accessibilityObject().isSelected());
}

void AccessibilityUIElement::isSelectableGetterCallback(CppVariant* result)
{
    result->set(accessibilityObject().canSetSelectedAttribute());
}

void AccessibilityUIElement::isMultiSelectableGetterCallback(CppVariant* result)
{
    result->set(accessibilityObject().isMultiSelectable());
}

void AccessibilityUIElement::isSelectedOptionActiveGetterCallback(CppVariant* result)
{
    result->set(accessibilityObject().isSelectedOptionActive());
}

void AccessibilityUIElement::isExpandedGetterCallback(CppVariant* result)
{
    result->set(!accessibilityObject().isCollapsed());
}

void AccessibilityUIElement::isCheckedGetterCallback(CppVariant* result)
{
    result->set(accessibilityObject().isChecked());
}

void AccessibilityUIElement::isVisibleGetterCallback(CppVariant* result)
{
    result->set(accessibilityObject().isVisible());
}

void AccessibilityUIElement::isOffScreenGetterCallback(CppVariant* result)
{
    result->set(accessibilityObject().isOffScreen());
}

void AccessibilityUIElement::isCollapsedGetterCallback(CppVariant* result)
{
    result->set(accessibilityObject().isCollapsed());
}

void AccessibilityUIElement::hasPopupGetterCallback(CppVariant* result)
{
    result->set(accessibilityObject().ariaHasPopup());
}

void AccessibilityUIElement::isValidGetterCallback(CppVariant* result)
{
    result->set(!accessibilityObject().isDetached());
}

void AccessibilityUIElement::isReadOnlyGetterCallback(CppVariant* result)
{
    result->set(accessibilityObject().isReadOnly());
}

void AccessibilityUIElement::orientationGetterCallback(CppVariant* result)
{
    result->set(getOrientation(accessibilityObject()));
}

void AccessibilityUIElement::clickPointXGetterCallback(CppVariant* result)
{
    result->set(accessibilityObject().clickPoint().x);
}

void AccessibilityUIElement::clickPointYGetterCallback(CppVariant* result)
{
    result->set(accessibilityObject().clickPoint().y);
}

void AccessibilityUIElement::rowCountGetterCallback(CppVariant* result)
{
    result->set(static_cast<int32_t>(accessibilityObject().rowCount()));
}

void AccessibilityUIElement::columnCountGetterCallback(CppVariant* result)
{
    result->set(static_cast<int32_t>(accessibilityObject().columnCount()));
}

void AccessibilityUIElement::isClickableGetterCallback(CppVariant* result)
{
    result->set(accessibilityObject().isClickable());
}

//
// Methods
//

void AccessibilityUIElement::allAttributesCallback(const CppArgumentList&, CppVariant* result)
{
    result->set(getAttributes(accessibilityObject()));
}

void AccessibilityUIElement::attributesOfLinkedUIElementsCallback(const CppArgumentList&, CppVariant* result)
{
    result->setNull();
}

void AccessibilityUIElement::attributesOfDocumentLinksCallback(const CppArgumentList&, CppVariant* result)
{
    result->setNull();
}

void AccessibilityUIElement::attributesOfChildrenCallback(const CppArgumentList& arguments, CppVariant* result)
{
    AttributesCollector collector;
    unsigned size = accessibilityObject().childCount();
    for (unsigned i = 0; i < size; ++i)
        collector.collectAttributes(accessibilityObject().childAt(i));
    result->set(collector.attributes());
}

void AccessibilityUIElement::parametrizedAttributeNamesCallback(const CppArgumentList&, CppVariant* result)
{
    result->setNull();
}

void AccessibilityUIElement::lineForIndexCallback(const CppArgumentList& arguments, CppVariant* result)
{
    if (!arguments.size() || !arguments[0].isNumber()) {
        result->setNull();
        return;
    }

    int index = arguments[0].toInt32();

    WebVector<int> lineBreaks;
    accessibilityObject().lineBreaks(lineBreaks);
    int line = 0;
    int vectorSize = static_cast<int>(lineBreaks.size());
    while (line < vectorSize && lineBreaks[line] <= index)
        line++;
    result->set(line);
}

void AccessibilityUIElement::boundsForRangeCallback(const CppArgumentList&, CppVariant* result)
{
    result->setNull();
}

void AccessibilityUIElement::stringForRangeCallback(const CppArgumentList&, CppVariant* result)
{
    result->setNull();
}

void AccessibilityUIElement::childAtIndexCallback(const CppArgumentList& arguments, CppVariant* result)
{
    if (!arguments.size() || !arguments[0].isNumber()) {
        result->setNull();
        return;
    }

    AccessibilityUIElement* child = getChildAtIndex(arguments[0].toInt32());
    if (!child) {
        result->setNull();
        return;
    }

    result->set(*(child->getAsCppVariant()));
}

void AccessibilityUIElement::elementAtPointCallback(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();

    if (arguments.size() != 2 || !arguments[0].isNumber() || !arguments[1].isNumber())
        return;

    int x = arguments[0].toInt32();
    int y = arguments[1].toInt32();
    WebPoint point(x, y);
    WebAXObject obj = accessibilityObject().hitTest(point);
    if (obj.isNull())
        return;

    result->set(*(m_factory->getOrCreate(obj)->getAsCppVariant()));
}

void AccessibilityUIElement::attributesOfColumnHeadersCallback(const CppArgumentList&, CppVariant* result)
{
    result->setNull();
}

void AccessibilityUIElement::attributesOfRowHeadersCallback(const CppArgumentList&, CppVariant* result)
{
    result->setNull();
}

void AccessibilityUIElement::attributesOfColumnsCallback(const CppArgumentList&, CppVariant* result)
{
    result->setNull();
}

void AccessibilityUIElement::attributesOfRowsCallback(const CppArgumentList&, CppVariant* result)
{
    result->setNull();
}

void AccessibilityUIElement::attributesOfVisibleCellsCallback(const CppArgumentList&, CppVariant* result)
{
    result->setNull();
}

void AccessibilityUIElement::attributesOfHeaderCallback(const CppArgumentList&, CppVariant* result)
{
    result->setNull();
}

void AccessibilityUIElement::tableHeaderCallback(const CppArgumentList&, CppVariant* result)
{
    WebAXObject obj = accessibilityObject().headerContainerObject();
    if (obj.isNull()) {
        result->setNull();
        return;
    }

    result->set(*(m_factory->getOrCreate(obj)->getAsCppVariant()));
}

void AccessibilityUIElement::indexInTableCallback(const CppArgumentList&, CppVariant* result)
{
    result->setNull();
}

void AccessibilityUIElement::rowIndexRangeCallback(const CppArgumentList&, CppVariant* result)
{
    unsigned rowIndex = accessibilityObject().cellRowIndex();
    unsigned rowSpan = accessibilityObject().cellRowSpan();
    char buffer[100];
    snprintf(buffer, sizeof(buffer), "{%d, %d}", rowIndex, rowSpan);
    string value = buffer;
    result->set(std::string(buffer));
}

void AccessibilityUIElement::columnIndexRangeCallback(const CppArgumentList&, CppVariant* result)
{
    unsigned columnIndex = accessibilityObject().cellColumnIndex();
    unsigned columnSpan = accessibilityObject().cellColumnSpan();
    char buffer[100];
    snprintf(buffer, sizeof(buffer), "{%d, %d}", columnIndex, columnSpan);
    result->set(std::string(buffer));
}

void AccessibilityUIElement::cellForColumnAndRowCallback(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() != 2 || !arguments[0].isNumber() || !arguments[1].isNumber())
        return;

    int column = arguments[0].toInt32();
    int row = arguments[1].toInt32();
    WebAXObject obj = accessibilityObject().cellForColumnAndRow(column, row);
    if (obj.isNull()) {
        result->setNull();
        return;
    }

    result->set(*(m_factory->getOrCreate(obj)->getAsCppVariant()));
}

void AccessibilityUIElement::titleUIElementCallback(const CppArgumentList&, CppVariant* result)
{
    WebAXObject obj = accessibilityObject().titleUIElement();
    if (obj.isNull()) {
        result->setNull();
        return;
    }

    result->set(*(m_factory->getOrCreate(obj)->getAsCppVariant()));
}

void AccessibilityUIElement::setSelectedTextRangeCallback(const CppArgumentList&arguments, CppVariant* result)
{
    result->setNull();
    if (arguments.size() != 2 || !arguments[0].isNumber() || !arguments[1].isNumber())
        return;

    int selectionStart = arguments[0].toInt32();
    int selectionEnd = selectionStart + arguments[1].toInt32();
    accessibilityObject().setSelectedTextRange(selectionStart, selectionEnd);
}

void AccessibilityUIElement::attributeValueCallback(const CppArgumentList&, CppVariant* result)
{
    result->setNull();
}

void AccessibilityUIElement::isAttributeSettableCallback(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() < 1 && !arguments[0].isString()) {
        result->setNull();
        return;
    }

    string attribute = arguments[0].toString();
    bool settable = false;
    if (attribute == "AXValue")
        settable = accessibilityObject().canSetValueAttribute();
    result->set(settable);
}

void AccessibilityUIElement::isPressActionSupportedCallback(const CppArgumentList&, CppVariant* result)
{
    result->set(accessibilityObject().canPress());
}

void AccessibilityUIElement::isIncrementActionSupportedCallback(const CppArgumentList&, CppVariant* result)
{
    result->set(accessibilityObject().canIncrement());
}

void AccessibilityUIElement::isDecrementActionSupportedCallback(const CppArgumentList&, CppVariant* result)
{
    result->set(accessibilityObject().canDecrement());
}

void AccessibilityUIElement::parentElementCallback(const CppArgumentList&, CppVariant* result)
{
    AccessibilityUIElement* parent = m_factory->getOrCreate(accessibilityObject().parentObject());
    if (!parent) {
        result->setNull();
        return;
    }

    result->set(*(parent->getAsCppVariant()));
}

void AccessibilityUIElement::incrementCallback(const CppArgumentList&, CppVariant* result)
{
    accessibilityObject().increment();
    result->setNull();
}

void AccessibilityUIElement::decrementCallback(const CppArgumentList&, CppVariant* result)
{
    accessibilityObject().decrement();
    result->setNull();
}

void AccessibilityUIElement::showMenuCallback(const CppArgumentList&, CppVariant* result)
{
    result->setNull();
}

void AccessibilityUIElement::pressCallback(const CppArgumentList&, CppVariant* result)
{
    accessibilityObject().press();
    result->setNull();
}

void AccessibilityUIElement::isEqualCallback(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() < 1 || !arguments[0].isObject()) {
        result->setNull();
        return;
    }

    result->set(arguments[0].isEqual(*getAsCppVariant()));
}

void AccessibilityUIElement::addNotificationListenerCallback(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() < 1 || !arguments[0].isObject()) {
        result->setNull();
        return;
    }

    m_notificationCallbacks.push_back(arguments[0]);
    result->setNull();
}

void AccessibilityUIElement::removeNotificationListenerCallback(const CppArgumentList&, CppVariant* result)
{
    // FIXME: Implement this.
    result->setNull();
}

void AccessibilityUIElement::takeFocusCallback(const CppArgumentList&, CppVariant* result)
{
    accessibilityObject().setFocused(true);
    result->setNull();
}

void AccessibilityUIElement::scrollToMakeVisibleCallback(const CppArgumentList&, CppVariant* result)
{
    accessibilityObject().scrollToMakeVisible();
    result->setNull();
}

void AccessibilityUIElement::scrollToMakeVisibleWithSubFocusCallback(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();

    if (arguments.size() != 4
        || !arguments[0].isNumber()
        || !arguments[1].isNumber()
        || !arguments[2].isNumber()
        || !arguments[3].isNumber())
        return;

    int x = arguments[0].toInt32();
    int y = arguments[1].toInt32();
    int width = arguments[2].toInt32();
    int height = arguments[3].toInt32();
    accessibilityObject().scrollToMakeVisibleWithSubFocus(WebRect(x, y, width, height));
    result->setNull();
}

void AccessibilityUIElement::scrollToGlobalPointCallback(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();

    if (arguments.size() != 2
        || !arguments[0].isNumber()
        || !arguments[1].isNumber())
        return;

    int x = arguments[0].toInt32();
    int y = arguments[1].toInt32();

    accessibilityObject().scrollToGlobalPoint(WebPoint(x, y));
    result->setNull();
}

void AccessibilityUIElement::fallbackCallback(const CppArgumentList &, CppVariant* result)
{
    // FIXME: Implement this.
    result->setNull();
}

RootAccessibilityUIElement::RootAccessibilityUIElement(const WebAXObject &object, Factory *factory)
    : AccessibilityUIElement(object, factory) { }

AccessibilityUIElement* RootAccessibilityUIElement::getChildAtIndex(unsigned index)
{
    if (index)
        return 0;

    return factory()->getOrCreate(accessibilityObject());
}


AccessibilityUIElementList ::~AccessibilityUIElementList()
{
    clear();
}

void AccessibilityUIElementList::clear()
{
    for (ElementList::iterator i = m_elements.begin(); i != m_elements.end(); ++i)
        delete (*i);
    m_elements.clear();
}

AccessibilityUIElement* AccessibilityUIElementList::getOrCreate(const WebAXObject& object)
{
    if (object.isNull())
        return 0;

    size_t elementCount = m_elements.size();
    for (size_t i = 0; i < elementCount; i++) {
        if (m_elements[i]->isEqual(object))
            return m_elements[i];
    }

    AccessibilityUIElement* element = new AccessibilityUIElement(object, this);
    m_elements.push_back(element);
    return element;
}

AccessibilityUIElement* AccessibilityUIElementList::createRoot(const WebAXObject& object)
{
    AccessibilityUIElement* element = new RootAccessibilityUIElement(object, this);
    m_elements.push_back(element);
    return element;
}

}
