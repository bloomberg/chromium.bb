// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FXCRT_SHARED_COPY_ON_WRITE_H_
#define CORE_FXCRT_SHARED_COPY_ON_WRITE_H_

#include "core/fxcrt/fx_system.h"
#include "core/fxcrt/retain_ptr.h"

namespace fxcrt {

// A shared pointer to an object with Copy on Write semantics that makes it
// appear as if all instances were independent. |ObjClass| must implement the
// requirements of |Retainable| from retain_ptr.h, and must also provide a
// Clone() method. Often this will just call MakeRetain<>(*this) but will need
// to be virtual if |ObjClass| is subclassed.
template <class ObjClass>
class SharedCopyOnWrite {
 public:
  SharedCopyOnWrite() = default;
  SharedCopyOnWrite(const SharedCopyOnWrite& other)
      : m_pObject(other.m_pObject) {}
  ~SharedCopyOnWrite() = default;

  template <typename... Args>
  ObjClass* Emplace(Args... params) {
    m_pObject = pdfium::MakeRetain<ObjClass>(params...);
    return m_pObject.Get();
  }

  SharedCopyOnWrite& operator=(const SharedCopyOnWrite& that) {
    if (*this != that)
      m_pObject = that.m_pObject;
    return *this;
  }

  void SetNull() { m_pObject.Reset(); }
  const ObjClass* GetObject() const { return m_pObject.Get(); }

  template <typename... Args>
  ObjClass* GetPrivateCopy(Args... params) {
    if (!m_pObject)
      return Emplace(params...);
    if (!m_pObject->HasOneRef())
      m_pObject = m_pObject->Clone();
    return m_pObject.Get();
  }

  bool operator==(const SharedCopyOnWrite& that) const {
    return m_pObject == that.m_pObject;
  }
  bool operator!=(const SharedCopyOnWrite& that) const {
    return !(*this == that);
  }
  explicit operator bool() const { return !!m_pObject; }

 private:
  RetainPtr<ObjClass> m_pObject;
};

}  // namespace fxcrt

using fxcrt::SharedCopyOnWrite;

#endif  // CORE_FXCRT_SHARED_COPY_ON_WRITE_H_
