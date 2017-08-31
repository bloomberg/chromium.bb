/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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

#ifndef AXTableRow_h
#define AXTableRow_h

#include "modules/accessibility/AXLayoutObject.h"

namespace blink {

class AXObjectCacheImpl;

class MODULES_EXPORT AXTableRow : public AXLayoutObject {
  WTF_MAKE_NONCOPYABLE(AXTableRow);

 protected:
  AXTableRow(LayoutObject*, AXObjectCacheImpl&);

 public:
  static AXTableRow* Create(LayoutObject*, AXObjectCacheImpl&);
  ~AXTableRow() override;

  void AddChildren() final;
  bool IsTableRow() const final;

  // retrieves the "row" header (a th tag in the rightmost column)
  virtual AXObject* HeaderObject();
  // Retrieves the "row" headers (th, scope) from left to right for the each
  // row.
  virtual void HeaderObjectsForRow(AXObjectVector&);
  virtual AXObject* ParentTable() const;

  void SetRowIndex(int row_index) { row_index_ = row_index; }
  int RowIndex() const { return row_index_; }

  unsigned AriaColumnIndex() const;
  unsigned AriaRowIndex() const;

  virtual bool CanSetSelectedAttribute() const { return false; }

 protected:
  AccessibilityRole DetermineAccessibilityRole() final;

 private:
  int row_index_;

  bool ComputeAccessibilityIsIgnored(IgnoredReasons* = nullptr) const final;
};

DEFINE_AX_OBJECT_TYPE_CASTS(AXTableRow, IsTableRow());

}  // namespace blink

#endif  // AXTableRow_h
