// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TRIVIAL_CTOR_H_
#define TRIVIAL_CTOR_H_

template<typename T>
struct atomic {
/*
  atomic() noexcept = default;
  ~atomic() noexcept = default;
  atomic(const atomic&) = delete;
  atomic& operator=(const atomic&) = delete;
  atomic& operator=(const atomic&) volatile = delete;

  constexpr atomic(T i) noexcept : i(i) { }
*/

  T i;
};

typedef atomic<int> atomic_int;

struct MySpinLock {
  MySpinLock();
  ~MySpinLock();
  MySpinLock(const MySpinLock&);
  MySpinLock(MySpinLock&&);
  atomic_int lock_;
};

#endif  // TRIVIAL_CTOR_H_
