//===--- ArrayRef.h - Array Reference Wrapper -------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_ADT_ARRAYREF_H
#define LLVM_ADT_ARRAYREF_H

#include "llvm/ADT/SmallVector.h"
#include <vector>

namespace llvm {
  class APInt;
  
  /// ArrayRef - Represent a constant reference to an array (0 or more elements
  /// consecutively in memory), i.e. a start pointer and a length.  It allows
  /// various APIs to take consecutive elements easily and conveniently.
  ///
  /// This class does not own the underlying data, it is expected to be used in
  /// situations where the data resides in some other buffer, whose lifetime
  /// extends past that of the ArrayRef. For this reason, it is not in general
  /// safe to store an ArrayRef.
  ///
  /// This is intended to be trivially copyable, so it should be passed by
  /// value.
  template<typename T>
  class ArrayRef {
  public:
    typedef const T *iterator;
    typedef const T *const_iterator;
    typedef size_t size_type;
    
  private:
    /// The start of the array, in an external buffer.
    const T *Data;
    
    /// The number of elements.
    size_type Length;
    
  public:
    /// @name Constructors
    /// @{
    
    /// Construct an empty ArrayRef.
    /*implicit*/ ArrayRef() : Data(0), Length(0) {}
    
    /// Construct an ArrayRef from a single element.
    /*implicit*/ ArrayRef(const T &OneElt)
      : Data(&OneElt), Length(1) {}
    
    /// Construct an ArrayRef from a pointer and length.
    /*implicit*/ ArrayRef(const T *data, size_t length)
      : Data(data), Length(length) {}
    
    /// Construct an ArrayRef from a range.
    ArrayRef(const T *begin, const T *end)
      : Data(begin), Length(end - begin) {}
    
    /// Construct an ArrayRef from a SmallVector.
    /*implicit*/ ArrayRef(const SmallVectorImpl<T> &Vec)
      : Data(Vec.data()), Length(Vec.size()) {}

    /// Construct an ArrayRef from a std::vector.
    /*implicit*/ ArrayRef(const std::vector<T> &Vec)
      : Data(Vec.empty() ? (T*)0 : &Vec[0]), Length(Vec.size()) {}
    
    /// Construct an ArrayRef from a C array.
    template <size_t N>
    /*implicit*/ ArrayRef(const T (&Arr)[N])
      : Data(Arr), Length(N) {}
    
    /// @}
    /// @name Simple Operations
    /// @{

    iterator begin() const { return Data; }
    iterator end() const { return Data + Length; }
    
    /// empty - Check if the array is empty.
    bool empty() const { return Length == 0; }
    
    const T *data() const { return Data; }
    
    /// size - Get the array size.
    size_t size() const { return Length; }
    
    /// front - Get the first element.
    const T &front() const {
      assert(!empty());
      return Data[0];
    }
    
    /// back - Get the last element.
    const T &back() const {
      assert(!empty());
      return Data[Length-1];
    }
    
    /// equals - Check for element-wise equality.
    bool equals(ArrayRef RHS) const {
      if (Length != RHS.Length)
        return false;
      for (size_type i = 0; i != Length; i++)
        if (Data[i] != RHS.Data[i])
          return false;
      return true;
    }

    /// slice(n) - Chop off the first N elements of the array.
    ArrayRef<T> slice(unsigned N) {
      assert(N <= size() && "Invalid specifier");
      return ArrayRef<T>(data()+N, size()-N);
    }

    /// slice(n, m) - Chop off the first N elements of the array, and keep M
    /// elements in the array.
    ArrayRef<T> slice(unsigned N, unsigned M) {
      assert(N+M <= size() && "Invalid specifier");
      return ArrayRef<T>(data()+N, M);
    }
    
    /// @}
    /// @name Operator Overloads
    /// @{
    const T &operator[](size_t Index) const {
      assert(Index < Length && "Invalid index!");
      return Data[Index];
    }
    
    /// @}
    /// @name Expensive Operations
    /// @{
    std::vector<T> vec() const {
      return std::vector<T>(Data, Data+Length);
    }
    
    /// @}
    /// @name Conversion operators
    /// @{
    operator std::vector<T>() const {
      return std::vector<T>(Data, Data+Length);
    }
    
    /// @}
  };

  /// @name ArrayRef Convenience constructors
  /// @{

  /// Construct an ArrayRef from a single element.
  template<typename T>
  ArrayRef<T> makeArrayRef(const T &OneElt) {
    return OneElt;
  }

  /// Construct an ArrayRef from a pointer and length.
  template<typename T>
  ArrayRef<T> makeArrayRef(const T *data, size_t length) {
    return ArrayRef<T>(data, length);
  }

  /// Construct an ArrayRef from a range.
  template<typename T>
  ArrayRef<T> makeArrayRef(const T *begin, const T *end) {
    return ArrayRef<T>(begin, end);
  }

  /// Construct an ArrayRef from a SmallVector.
  template <typename T>
  ArrayRef<T> makeArrayRef(const SmallVectorImpl<T> &Vec) {
    return Vec;
  }

  /// Construct an ArrayRef from a SmallVector.
  template <typename T, unsigned N>
  ArrayRef<T> makeArrayRef(const SmallVector<T, N> &Vec) {
    return Vec;
  }

  /// Construct an ArrayRef from a std::vector.
  template<typename T>
  ArrayRef<T> makeArrayRef(const std::vector<T> &Vec) {
    return Vec;
  }

  /// Construct an ArrayRef from a C array.
  template<typename T, size_t N>
  ArrayRef<T> makeArrayRef(const T (&Arr)[N]) {
    return ArrayRef<T>(Arr);
  }

  /// @}
  /// @name ArrayRef Comparison Operators
  /// @{

  template<typename T>
  inline bool operator==(ArrayRef<T> LHS, ArrayRef<T> RHS) {
    return LHS.equals(RHS);
  }

  template<typename T>
  inline bool operator!=(ArrayRef<T> LHS, ArrayRef<T> RHS) {
    return !(LHS == RHS);
  }

  /// @}

  // ArrayRefs can be treated like a POD type.
  template <typename T> struct isPodLike;
  template <typename T> struct isPodLike<ArrayRef<T> > {
    static const bool value = true;
  };
}

#endif
