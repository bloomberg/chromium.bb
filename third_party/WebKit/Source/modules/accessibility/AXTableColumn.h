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

#ifndef AXTableColumn_h
#define AXTableColumn_h

#include "modules/ModulesExport.h"
#include "modules/accessibility/AXMockObject.h"
#include "modules/accessibility/AXTable.h"

namespace blink {

class AXObjectCacheImpl;

class MODULES_EXPORT AXTableColumn final : public AXMockObject {
  WTF_MAKE_NONCOPYABLE(AXTableColumn);

 private:
  explicit AXTableColumn(AXObjectCacheImpl&);

 public:
  static AXTableColumn* Create(AXObjectCacheImpl&);
  ~AXTableColumn() override;

  // retrieves the topmost "column" header (th)
  AXObject* HeaderObject();
  // retrieves the "column" headers (th, scope) from top to bottom
  void HeaderObjectsForColumn(AXObjectVector&);

  AccessibilityRole RoleValue() const override { return kColumnRole; }

  void SetColumnIndex(int column_index) { column_index_ = column_index; }
  int ColumnIndex() const { return column_index_; }
  virtual bool CanSetSelectedAttribute() const { return false; }

  void AddChildren() override;
  void SetParent(AXObject*) override;

 private:
  unsigned column_index_;

  bool IsTableCol() const override { return true; }
  bool ComputeAccessibilityIsIgnored(IgnoredReasons* = nullptr) const override;
};

DEFINE_AX_OBJECT_TYPE_CASTS(AXTableColumn, IsTableCol());

}  // namespace blink

#endif  // AXTableColumn_h
