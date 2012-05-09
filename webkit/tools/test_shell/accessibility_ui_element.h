// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_TOOLS_TEST_SHELL_ACCESSIBILITY_UI_ELEMENT_H_
#define WEBKIT_TOOLS_TEST_SHELL_ACCESSIBILITY_UI_ELEMENT_H_

#include "webkit/glue/cpp_bound_class.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebAccessibilityObject.h"

class AccessibilityUIElement : public webkit_glue::CppBoundClass {
 public:
  class Factory {
  public:
    virtual ~Factory() { }
    virtual AccessibilityUIElement* Create(
        const WebKit::WebAccessibilityObject& object) = 0;
  };

  AccessibilityUIElement(
      const WebKit::WebAccessibilityObject& accessibility_object,
      Factory* factory);
  virtual ~AccessibilityUIElement();

  virtual AccessibilityUIElement* GetChildAtIndex(unsigned index);
  virtual bool IsRoot() const;

 protected:
  const WebKit::WebAccessibilityObject& accessibility_object() const {
    return accessibility_object_;
  }
  Factory* factory() const { return factory_; }

 private:
  // Bound methods and properties.
  void AllAttributesCallback(
      const webkit_glue::CppArgumentList& args,
      webkit_glue::CppVariant* result);
  void AttributesOfLinkedUIElementsCallback(
      const webkit_glue::CppArgumentList& args,
      webkit_glue::CppVariant* result);
  void AttributesOfDocumentLinksCallback(
      const webkit_glue::CppArgumentList& args,
      webkit_glue::CppVariant* result);
  void AttributesOfChildrenCallback(
      const webkit_glue::CppArgumentList& args,
      webkit_glue::CppVariant* result);
  void ParametrizedAttributeNamesCallback(
      const webkit_glue::CppArgumentList& args,
      webkit_glue::CppVariant* result);
  void LineForIndexCallback(
      const webkit_glue::CppArgumentList& args,
      webkit_glue::CppVariant* result);
  void BoundsForRangeCallback(
      const webkit_glue::CppArgumentList& args,
      webkit_glue::CppVariant* result);
  void StringForRangeCallback(
      const webkit_glue::CppArgumentList& args,
      webkit_glue::CppVariant* result);
  void ChildAtIndexCallback(
      const webkit_glue::CppArgumentList& args,
      webkit_glue::CppVariant* result);
  void ElementAtPointCallback(
      const webkit_glue::CppArgumentList& args,
      webkit_glue::CppVariant* result);
  void AttributesOfColumnHeadersCallback(
      const webkit_glue::CppArgumentList& args,
      webkit_glue::CppVariant* result);
  void AttributesOfRowHeadersCallback(
      const webkit_glue::CppArgumentList& args,
      webkit_glue::CppVariant* result);
  void AttributesOfColumnsCallback(
      const webkit_glue::CppArgumentList& args,
      webkit_glue::CppVariant* result);
  void AttributesOfRowsCallback(
      const webkit_glue::CppArgumentList& args,
      webkit_glue::CppVariant* result);
  void AttributesOfVisibleCellsCallback(
      const webkit_glue::CppArgumentList& args,
      webkit_glue::CppVariant* result);
  void AttributesOfHeaderCallback(
      const webkit_glue::CppArgumentList& args,
      webkit_glue::CppVariant* result);
  void IndexInTableCallback(
      const webkit_glue::CppArgumentList& args,
      webkit_glue::CppVariant* result);
  void RowIndexRangeCallback(
      const webkit_glue::CppArgumentList& args,
      webkit_glue::CppVariant* result);
  void ColumnIndexRangeCallback(
      const webkit_glue::CppArgumentList& args,
      webkit_glue::CppVariant* result);
  void CellForColumnAndRowCallback(
      const webkit_glue::CppArgumentList& args,
      webkit_glue::CppVariant* result);
  void TitleUIElementCallback(
      const webkit_glue::CppArgumentList& args,
      webkit_glue::CppVariant* result);
  void SetSelectedTextRangeCallback(
      const webkit_glue::CppArgumentList& args,
      webkit_glue::CppVariant* result);
  void AttributeValueCallback(
      const webkit_glue::CppArgumentList& args,
      webkit_glue::CppVariant* result);
  void IsAttributeSettableCallback(
      const webkit_glue::CppArgumentList& args,
      webkit_glue::CppVariant* result);
  void IsActionSupportedCallback(
      const webkit_glue::CppArgumentList& args,
      webkit_glue::CppVariant* result);
  void ParentElementCallback(
      const webkit_glue::CppArgumentList& args,
      webkit_glue::CppVariant* result);
  void IncrementCallback(
      const webkit_glue::CppArgumentList& args,
      webkit_glue::CppVariant* result);
  void DecrementCallback(
      const webkit_glue::CppArgumentList& args,
      webkit_glue::CppVariant* result);
  void FallbackCallback(
      const webkit_glue::CppArgumentList& args,
      webkit_glue::CppVariant* result);

  void ChildrenCountGetterCallback(webkit_glue::CppVariant* result);
  void DescriptionGetterCallback(webkit_glue::CppVariant* result);
  void IsEnabledGetterCallback(webkit_glue::CppVariant* result);
  void IsSelectedGetterCallback(webkit_glue::CppVariant* result);
  void RoleGetterCallback(webkit_glue::CppVariant* result);
  void TitleGetterCallback(webkit_glue::CppVariant* result);

  webkit_glue::CppVariant subrole_;
  webkit_glue::CppVariant language_;
  webkit_glue::CppVariant x_;
  webkit_glue::CppVariant y_;
  webkit_glue::CppVariant width_;
  webkit_glue::CppVariant height_;
  webkit_glue::CppVariant click_point_x_;
  webkit_glue::CppVariant click_point_y_;
  webkit_glue::CppVariant int_value_;
  webkit_glue::CppVariant min_value_;
  webkit_glue::CppVariant max_value_;
  webkit_glue::CppVariant children_count_;
  webkit_glue::CppVariant insertion_point_line_number_;
  webkit_glue::CppVariant selected_text_range;
  webkit_glue::CppVariant is_required_;
  webkit_glue::CppVariant value_description_;

  WebKit::WebAccessibilityObject accessibility_object_;
  Factory* factory_;
};


class RootAccessibilityUIElement : public AccessibilityUIElement {
 public:
  RootAccessibilityUIElement(
      const WebKit::WebAccessibilityObject& accessibility_object,
      Factory* factory);
  virtual ~RootAccessibilityUIElement();

  virtual AccessibilityUIElement* GetChildAtIndex(unsigned index) OVERRIDE;
  virtual bool IsRoot() const OVERRIDE;
};


// Provides simple lifetime management of the AccessibilityUIElement instances:
// all AccessibilityUIElements ever created from the controller are stored in
// a list and cleared explicitly.
class AccessibilityUIElementList : public AccessibilityUIElement::Factory {
 public:
  AccessibilityUIElementList();
  virtual ~AccessibilityUIElementList();

  void Clear();
  virtual AccessibilityUIElement* Create(
      const WebKit::WebAccessibilityObject& object) OVERRIDE;
  AccessibilityUIElement* CreateRoot(
      const WebKit::WebAccessibilityObject& object);

 private:
  typedef std::vector<AccessibilityUIElement*> ElementList;
  ElementList elements_;
};

#endif  // WEBKIT_TOOLS_TEST_SHELL_ACCESSIBILITY_UI_ELEMENT_H_
