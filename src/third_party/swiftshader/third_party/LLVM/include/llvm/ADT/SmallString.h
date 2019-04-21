//===- llvm/ADT/SmallString.h - 'Normally small' strings --------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file defines the SmallString class.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_ADT_SMALLSTRING_H
#define LLVM_ADT_SMALLSTRING_H

#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"

namespace llvm {

/// SmallString - A SmallString is just a SmallVector with methods and accessors
/// that make it work better as a string (e.g. operator+ etc).
template<unsigned InternalLen>
class SmallString : public SmallVector<char, InternalLen> {
public:
  // Default ctor - Initialize to empty.
  SmallString() {}

  // Initialize from a StringRef.
  SmallString(StringRef S) : SmallVector<char, InternalLen>(S.begin(), S.end()) {}

  // Initialize with a range.
  template<typename ItTy>
  SmallString(ItTy S, ItTy E) : SmallVector<char, InternalLen>(S, E) {}

  // Copy ctor.
  SmallString(const SmallString &RHS) : SmallVector<char, InternalLen>(RHS) {}


  // Extra methods.
  StringRef str() const { return StringRef(this->begin(), this->size()); }

  // TODO: Make this const, if it's safe...
  const char* c_str() {
    this->push_back(0);
    this->pop_back();
    return this->data();
  }

  // Implicit conversion to StringRef.
  operator StringRef() const { return str(); }

  // Extra operators.
  const SmallString &operator=(StringRef RHS) {
    this->clear();
    return *this += RHS;
  }

  SmallString &operator+=(StringRef RHS) {
    this->append(RHS.begin(), RHS.end());
    return *this;
  }
  SmallString &operator+=(char C) {
    this->push_back(C);
    return *this;
  }
};

}

#endif
