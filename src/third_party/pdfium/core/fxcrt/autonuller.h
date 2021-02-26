// Copyright 2020 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CORE_FXCRT_AUTONULLER_H_
#define CORE_FXCRT_AUTONULLER_H_

namespace fxcrt {

template <typename T>
class AutoNuller {
 public:
  explicit AutoNuller(T* location) : m_Location(location) {}
  ~AutoNuller() {
    if (m_Location)
      *m_Location = nullptr;
  }
  void AbandonNullification() { m_Location = nullptr; }

 private:
  T* m_Location;
};

}  // namespace fxcrt

using fxcrt::AutoNuller;

#endif  // CORE_FXCRT_AUTONULLER_H_
