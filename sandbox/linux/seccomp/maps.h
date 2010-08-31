// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MAPS_H__
#define MAPS_H__

#include <elf.h>
#include <functional>
#include <map>
#include <string>

#include "allocator.h"

#if defined(__x86_64__)
typedef Elf64_Addr Elf_Addr;
#elif defined(__i386__)
typedef Elf32_Addr Elf_Addr;
#else
#error Undefined target platform
#endif

namespace playground {

class Library;
class Maps {
  friend class Library;
 public:
  typedef std::basic_string<char, std::char_traits<char>,
                            SystemAllocator<char> > string;

  Maps(int proc_self_maps);
  ~Maps() { }

 protected:
  // A map with all the libraries currently loaded into the application.
  // The key is a unique combination of device number, inode number, and
  // file name. It should be treated as opaque.
  typedef std::map<string, Library, std::less<string>,
                   SystemAllocator<std::pair<const string,
                                             Library> > > LibraryMap;
  friend class Iterator;
  class Iterator {
    friend class Maps;

   protected:
    explicit Iterator(Maps* maps);
    Iterator(Maps* maps, bool at_beginning, bool at_end);
    Maps::LibraryMap::iterator& getIterator() const;

   public:
    Iterator begin();
    Iterator end();
    Iterator& operator++();
    Iterator operator++(int i);
    Library* operator*() const;
    bool operator==(const Iterator& iter) const;
    bool operator!=(const Iterator& iter) const;
    string name() const;

   protected:
    mutable LibraryMap::iterator iter_;
    Maps *maps_;
    bool at_beginning_;
    bool at_end_;
  };

 public:
  typedef class Iterator const_iterator;

  const_iterator begin() {
    return begin_iter_;
  }

  const_iterator end() {
    return end_iter_;
  }

  char* allocNearAddr(char *addr, size_t size, int prot) const;

  char* vsyscall() const { return vsyscall_; }

 protected:
  const int      proc_self_maps_;
  const Iterator begin_iter_;
  const Iterator end_iter_;

  LibraryMap     libs_;
  char*          vsyscall_;
};

} // namespace

#endif // MAPS_H__
