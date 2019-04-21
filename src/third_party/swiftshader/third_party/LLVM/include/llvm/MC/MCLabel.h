//===- MCLabel.h - Machine Code Directional Local Labels --------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file contains the declaration of the MCLabel class.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_MC_MCLABEL_H
#define LLVM_MC_MCLABEL_H

namespace llvm {
  class MCContext;
  class raw_ostream;

  /// MCLabel - Instances of this class represent a label name in the MC file,
  /// and MCLabel are created and unique'd by the MCContext class.  MCLabel
  /// should only be constructed for valid instances in the object file.
  class MCLabel {
    // Instance - the instance number of this Directional Local Label
    unsigned Instance;

  private:  // MCContext creates and uniques these.
    friend class MCContext;
    MCLabel(unsigned instance)
      : Instance(instance) {}

    MCLabel(const MCLabel&);       // DO NOT IMPLEMENT
    void operator=(const MCLabel&); // DO NOT IMPLEMENT
  public:
    /// getInstance - Get the current instance of this Directional Local Label.
    unsigned getInstance() const { return Instance; }

    /// incInstance - Increment the current instance of this Directional Local
    /// Label.
    unsigned incInstance() { return ++Instance; }

    /// print - Print the value to the stream \arg OS.
    void print(raw_ostream &OS) const;

    /// dump - Print the value to stderr.
    void dump() const;
  };

  inline raw_ostream &operator<<(raw_ostream &OS, const MCLabel &Label) {
    Label.print(OS);
    return OS;
  }
} // end namespace llvm

#endif
