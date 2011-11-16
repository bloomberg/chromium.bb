// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_TOOLS_TEST_SHELL_ACCESSIBILITY_UI_ELEMENT_H_
#define WEBKIT_TOOLS_TEST_SHELL_ACCESSIBILITY_UI_ELEMENT_H_

#include "webkit/glue/cpp_bound_class.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebAccessibilityObject.h"

class AccessibilityUIElement : public CppBoundClass {
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
  void AllAttributesCallback(const CppArgumentList& args, CppVariant* result);
  void AttributesOfLinkedUIElementsCallback(
      const CppArgumentList& args, CppVariant* result);
  void AttributesOfDocumentLinksCallback(
      const CppArgumentList& args, CppVariant* result);
  void AttributesOfChildrenCallback(
      const CppArgumentList& args, CppVariant* result);
  void ParametrizedAttributeNamesCallback(
      const CppArgumentList& args, CppVariant* result);
  void LineForIndexCallback(const CppArgumentList& args, CppVariant* result);
  void BoundsForRangeCallback(const CppArgumentList& args, CppVariant* result);
  void StringForRangeCallback(const CppArgumentList& args, CppVariant* result);
  void ChildAtIndexCallback(const CppArgumentList& args, CppVariant* result);
  void ElementAtPointCallback(const CppArgumentList& args, CppVariant* result);
  void AttributesOfColumnHeadersCallback(
      const CppArgumentList& args, CppVariant* result);
  void AttributesOfRowHeadersCallback(
      const CppArgumentList& args, CppVariant* result);
  void AttributesOfColumnsCallback(
      const CppArgumentList& args, CppVariant* result);
  void AttributesOfRowsCallback(
      const CppArgumentList& args, CppVariant* result);
  void AttributesOfVisibleCellsCallback(
      const CppArgumentList& args, CppVariant* result);
  void AttributesOfHeaderCallback(
      const CppArgumentList& args, CppVariant* result);
  void IndexInTableCallback(const CppArgumentList& args, CppVariant* result);
  void RowIndexRangeCallback(const CppArgumentList& args, CppVariant* result);
  void ColumnIndexRangeCallback(
      const CppArgumentList& args, CppVariant* result);
  void CellForColumnAndRowCallback(
      const CppArgumentList& args, CppVariant* result);
  void TitleUIElementCallback(const CppArgumentList& args, CppVariant* result);
  void SetSelectedTextRangeCallback(
      const CppArgumentList& args, CppVariant* result);
  void AttributeValueCallback(const CppArgumentList& args, CppVariant* result);
  void IsAttributeSettableCallback(
      const CppArgumentList& args, CppVariant* result);
  void IsActionSupportedCallback(
      const CppArgumentList& args, CppVariant* result);
  void ParentElementCallback(const CppArgumentList& args, CppVariant* result);
  void IncrementCallback(const CppArgumentList& args, CppVariant* result);
  void DecrementCallback(const CppArgumentList& args, CppVariant* result);
  void FallbackCallback(const CppArgumentList& args, CppVariant* result);

  void ChildrenCountGetterCallback(CppVariant* result);
  void DescriptionGetterCallback(CppVariant* result);
  void IsEnabledGetterCallback(CppVariant* result);
  void IsSelectedGetterCallback(CppVariant* result);
  void RoleGetterCallback(CppVariant* result);
  void TitleGetterCallback(CppVariant* result);

  CppVariant subrole_;
  CppVariant language_;
  CppVariant x_;
  CppVariant y_;
  CppVariant width_;
  CppVariant height_;
  CppVariant click_point_x_;
  CppVariant click_point_y_;
  CppVariant int_value_;
  CppVariant min_value_;
  CppVariant max_value_;
  CppVariant children_count_;
  CppVariant insertion_point_line_number_;
  CppVariant selected_text_range;
  CppVariant is_required_;
  CppVariant value_description_;

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
