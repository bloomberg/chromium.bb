// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FWL_CFWL_MESSAGE_H_
#define XFA_FWL_CFWL_MESSAGE_H_

#include "core/fxcrt/mask.h"
#include "core/fxcrt/unowned_ptr.h"
#include "v8/include/cppgc/macros.h"

class CFWL_Widget;

class CFWL_Message {
  CPPGC_STACK_ALLOCATED();  // Allow Raw/Unowned pointers.

 public:
  enum class Type { kKey, kKillFocus, kMouse, kMouseWheel, kSetFocus };

  virtual ~CFWL_Message();

  Type GetType() const { return m_type; }
  CFWL_Widget* GetDstTarget() const { return m_pDstTarget; }
  void SetDstTarget(CFWL_Widget* pWidget) { m_pDstTarget = pWidget; }

 protected:
  CFWL_Message(Type type, CFWL_Widget* pDstTarget);
  CFWL_Message(const CFWL_Message& that) = delete;
  CFWL_Message& operator=(const CFWL_Message& that) = delete;

 private:
  const Type m_type;
  UnownedPtr<CFWL_Widget> m_pDstTarget;
};

#endif  // XFA_FWL_CFWL_MESSAGE_H_
