// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FXFA_PARSER_XFA_RESOLVENODE_RS_H_
#define XFA_FXFA_PARSER_XFA_RESOLVENODE_RS_H_

#include <vector>

#include "v8/include/cppgc/macros.h"
#include "v8/include/cppgc/member.h"
#include "xfa/fxfa/parser/xfa_basic_data.h"

class CXFA_Object;

class XFA_ResolveNodeRS {
  CPPGC_STACK_ALLOCATED();  // Allow raw/unowned pointers.

 public:
  enum class Type {
    kNodes,
    kAttribute,
    kCreateNodeOne,
    kCreateNodeAll,
    kCreateNodeMidAll,
    kExistNodes,
  };

  XFA_ResolveNodeRS();
  ~XFA_ResolveNodeRS();

  Type dwFlags = Type::kNodes;
  XFA_SCRIPTATTRIBUTEINFO script_attribute;

  // Vector of Member would be correct for stack-based vectors, if
  // STL worked with cppgc.
  std::vector<cppgc::Member<CXFA_Object>> objects;
};

#endif  // XFA_FXFA_PARSER_XFA_RESOLVENODE_RS_H_
