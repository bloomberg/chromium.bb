// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_AX_NODE_DATA_H_
#define UI_ACCESSIBILITY_AX_NODE_DATA_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/strings/string16.h"
#include "base/strings/string_split.h"
#include "ui/accessibility/ax_enums.h"
#include "ui/accessibility/ax_export.h"
#include "ui/gfx/geometry/rect_f.h"

namespace gfx {
class Transform;
};

namespace ui {

// Return true if |attr| should be interpreted as the id of another node
// in the same tree.
AX_EXPORT bool IsNodeIdIntAttribute(AXIntAttribute attr);

// Return true if |attr| should be interpreted as a list of ids of
// nodes in the same tree.
AX_EXPORT bool IsNodeIdIntListAttribute(AXIntListAttribute attr);

// A compact representation of the accessibility information for a
// single accessible object, in a form that can be serialized and sent from
// one process to another.
struct AX_EXPORT AXNodeData {
  AXNodeData();
  virtual ~AXNodeData();

  AXNodeData(const AXNodeData& other);
  AXNodeData& operator=(AXNodeData other);

  // Accessing accessibility attributes:
  //
  // There are dozens of possible attributes for an accessibility node,
  // but only a few tend to apply to any one object, so we store them
  // in sparse arrays of <attribute id, attribute value> pairs, organized
  // by type (bool, int, float, string, int list).
  //
  // There are three accessors for each type of attribute: one that returns
  // true if the attribute is present and false if not, one that takes a
  // pointer argument and returns true if the attribute is present (if you
  // need to distinguish between the default value and a missing attribute),
  // and another that returns the default value for that type if the
  // attribute is not present. In addition, strings can be returned as
  // either std::string or base::string16, for convenience.

  bool HasBoolAttribute(AXBoolAttribute attr) const;
  bool GetBoolAttribute(AXBoolAttribute attr) const;
  bool GetBoolAttribute(AXBoolAttribute attr, bool* value) const;

  bool HasFloatAttribute(AXFloatAttribute attr) const;
  float GetFloatAttribute(AXFloatAttribute attr) const;
  bool GetFloatAttribute(AXFloatAttribute attr, float* value) const;

  bool HasIntAttribute(AXIntAttribute attribute) const;
  int GetIntAttribute(AXIntAttribute attribute) const;
  bool GetIntAttribute(AXIntAttribute attribute, int* value) const;

  bool HasStringAttribute(
      AXStringAttribute attribute) const;
  const std::string& GetStringAttribute(AXStringAttribute attribute) const;
  bool GetStringAttribute(AXStringAttribute attribute,
                          std::string* value) const;

  bool GetString16Attribute(AXStringAttribute attribute,
                            base::string16* value) const;
  base::string16 GetString16Attribute(
      AXStringAttribute attribute) const;

  bool HasIntListAttribute(AXIntListAttribute attribute) const;
  const std::vector<int32_t>& GetIntListAttribute(
      AXIntListAttribute attribute) const;
  bool GetIntListAttribute(AXIntListAttribute attribute,
                           std::vector<int32_t>* value) const;

  bool GetHtmlAttribute(const char* attr, base::string16* value) const;
  bool GetHtmlAttribute(const char* attr, std::string* value) const;

  // Setting accessibility attributes.
  void AddStringAttribute(AXStringAttribute attribute,
                          const std::string& value);
  void AddIntAttribute(AXIntAttribute attribute, int value);
  void AddFloatAttribute(AXFloatAttribute attribute, float value);
  void AddBoolAttribute(AXBoolAttribute attribute, bool value);
  void AddIntListAttribute(AXIntListAttribute attribute,
                           const std::vector<int32_t>& value);

  // Convenience functions, mainly for writing unit tests.
  // Equivalent to AddStringAttribute(ATTR_NAME, name).
  void SetName(const std::string& name);
  void SetName(const base::string16& name);
  // Equivalent to AddStringAttribute(ATTR_VALUE, value).
  void SetValue(const std::string& value);
  void SetValue(const base::string16& value);

  // Returns true if the given enum bit is 1.
  bool HasState(ui::AXState state_enum) const;
  bool HasAction(ui::AXAction state_enum) const;

  // Set bits in the given enum's corresponding bitfield.
  void AddState(ui::AXState state_enum);
  void AddAction(ui::AXAction action_enum);

  // Return a string representation of this data, for debugging.
  virtual std::string ToString() const;

  // As much as possible this should behave as a simple, serializable,
  // copyable struct.
  int32_t id;
  AXRole role;
  uint32_t state;
  uint32_t actions;
  std::vector<std::pair<AXStringAttribute, std::string>> string_attributes;
  std::vector<std::pair<AXIntAttribute, int32_t>> int_attributes;
  std::vector<std::pair<AXFloatAttribute, float>> float_attributes;
  std::vector<std::pair<AXBoolAttribute, bool>> bool_attributes;
  std::vector<std::pair<AXIntListAttribute, std::vector<int32_t>>>
      intlist_attributes;
  base::StringPairs html_attributes;
  std::vector<int32_t> child_ids;

  // TODO(dmazzoni): replace the following three members with a single
  // instance of AXRelativeBounds.

  // The id of an ancestor node in the same AXTree that this object's
  // bounding box is relative to, or -1 if there's no offset container.
  int offset_container_id;

  // The relative bounding box of this node.
  gfx::RectF location;

  // An additional transform to apply to position this object and its subtree.
  // NOTE: this member is a std::unique_ptr because it's rare and gfx::Transform
  // takes up a fair amount of space. The assignment operator and copy
  // constructor both make a duplicate of the owned pointer, so it acts more
  // like a member than a pointer.
  std::unique_ptr<gfx::Transform> transform;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_AX_NODE_DATA_H_
