// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SK_REF_CNT_BASE_DEBUG_H_
#define SK_REF_CNT_BASE_DEBUG_H_

// Alternate implementation of SkRefCntBase for Chromium debug builds
class SK_API SkRefCntBase {
public:
  SkRefCntBase() : flags_(0) {}
  void aboutToRef() const { SkASSERT(flags_ != AdoptionRequired_Flag); }
  void adopted() const { flags_ |= Adopted_Flag; }
  void requireAdoption() const { flags_ |= AdoptionRequired_Flag; }
private:
  enum {
  	  Adopted_Flag = 0x1,
  	  AdoptionRequired_Flag = 0x2,
  };

  mutable int flags_;
};

// Bootstrap for Blink's WTF::RefPtr

namespace WTF {
  inline void adopted(const SkRefCntBase* object) {
    if (!object)
      return;
  	object->adopted();
  }
  inline void requireAdoption(const SkRefCntBase* object) {
    if (!object)
      return;
  	object->requireAdoption();
  }
};

using WTF::adopted;
using WTF::requireAdoption;

#endif

