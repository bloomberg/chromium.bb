// Copyright 2019 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/fxcrt/observed_ptr.h"

#include "third_party/base/check.h"
#include "third_party/base/containers/contains.h"

namespace fxcrt {

Observable::Observable() = default;

Observable::~Observable() {
  NotifyObservers();
}

void Observable::AddObserver(ObserverIface* pObserver) {
  DCHECK(!pdfium::Contains(m_Observers, pObserver));
  m_Observers.insert(pObserver);
}

void Observable::RemoveObserver(ObserverIface* pObserver) {
  DCHECK(pdfium::Contains(m_Observers, pObserver));
  m_Observers.erase(pObserver);
}

void Observable::NotifyObservers() {
  for (auto* pObserver : m_Observers)
    pObserver->OnObservableDestroyed();
  m_Observers.clear();
}

}  // namespace fxcrt
