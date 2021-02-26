// Copyright 2017 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FXFA_PARSER_CXFA_OBJECT_H_
#define XFA_FXFA_PARSER_CXFA_OBJECT_H_

#include "core/fxcrt/fx_string.h"
#include "fxjs/gc/heap.h"
#include "fxjs/xfa/fxjse.h"
#include "v8/include/cppgc/garbage-collected.h"
#include "v8/include/cppgc/member.h"
#include "xfa/fxfa/fxfa_basic.h"

namespace cppgc {
class Visitor;
}  // namespace cppgc

enum class XFA_ObjectType {
  Object,
  List,
  Node,
  NodeC,
  NodeV,
  ModelNode,
  TextNode,
  TreeList,
  ContainerNode,
  ContentNode,
  ThisProxy,
};

class CJX_Object;
class CXFA_Document;
class CXFA_List;
class CXFA_Node;
class CXFA_ThisProxy;
class CXFA_TreeList;

class CXFA_Object : public cppgc::GarbageCollected<CXFA_Object> {
 public:
  virtual ~CXFA_Object();

  virtual void Trace(cppgc::Visitor* visitor) const;

  CXFA_Document* GetDocument() const { return m_pDocument.Get(); }
  XFA_ObjectType GetObjectType() const { return m_objectType; }

  bool IsList() const {
    return m_objectType == XFA_ObjectType::List ||
           m_objectType == XFA_ObjectType::TreeList;
  }
  bool IsNode() const {
    return m_objectType == XFA_ObjectType::Node ||
           m_objectType == XFA_ObjectType::NodeC ||
           m_objectType == XFA_ObjectType::NodeV ||
           m_objectType == XFA_ObjectType::ModelNode ||
           m_objectType == XFA_ObjectType::TextNode ||
           m_objectType == XFA_ObjectType::ContainerNode ||
           m_objectType == XFA_ObjectType::ContentNode;
  }
  bool IsTreeList() const { return m_objectType == XFA_ObjectType::TreeList; }
  bool IsContentNode() const {
    return m_objectType == XFA_ObjectType::ContentNode;
  }
  bool IsContainerNode() const {
    return m_objectType == XFA_ObjectType::ContainerNode;
  }
  bool IsModelNode() const { return m_objectType == XFA_ObjectType::ModelNode; }
  bool IsNodeV() const { return m_objectType == XFA_ObjectType::NodeV; }
  bool IsThisProxy() const { return m_objectType == XFA_ObjectType::ThisProxy; }

  CXFA_List* AsList();
  CXFA_Node* AsNode();
  CXFA_TreeList* AsTreeList();
  CXFA_ThisProxy* AsThisProxy();

  CJX_Object* JSObject() { return m_pJSObject; }
  const CJX_Object* JSObject() const { return m_pJSObject; }

  bool HasCreatedUIWidget() const {
    return m_elementType == XFA_Element::Field ||
           m_elementType == XFA_Element::Draw ||
           m_elementType == XFA_Element::Subform ||
           m_elementType == XFA_Element::ExclGroup;
  }

  XFA_Element GetElementType() const { return m_elementType; }
  ByteStringView GetClassName() const { return m_elementName; }
  uint32_t GetClassHashCode() const { return m_elementNameHash; }

  WideString GetSOMExpression();

 protected:
  CXFA_Object(CXFA_Document* pDocument,
              XFA_ObjectType objectType,
              XFA_Element eType,
              CJX_Object* jsObject);

  const XFA_ObjectType m_objectType;
  const XFA_Element m_elementType;
  const ByteStringView m_elementName;
  const uint32_t m_elementNameHash;
  cppgc::WeakMember<CXFA_Document> m_pDocument;
  cppgc::Member<CJX_Object> m_pJSObject;
};

// Helper functions that permit nullptr arguments.
CXFA_List* ToList(CXFA_Object* pObj);
CXFA_Node* ToNode(CXFA_Object* pObj);
CXFA_TreeList* ToTreeList(CXFA_Object* pObj);
CXFA_ThisProxy* ToThisProxy(CXFA_Object* pObj);

#endif  // XFA_FXFA_PARSER_CXFA_OBJECT_H_
