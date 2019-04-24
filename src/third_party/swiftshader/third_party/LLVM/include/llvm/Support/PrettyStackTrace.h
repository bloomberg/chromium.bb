//===- llvm/Support/PrettyStackTrace.h - Pretty Crash Handling --*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file defines the PrettyStackTraceEntry class, which is used to make
// crashes give more contextual information about what the program was doing
// when it crashed.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_SUPPORT_PRETTYSTACKTRACE_H
#define LLVM_SUPPORT_PRETTYSTACKTRACE_H

namespace llvm {
  class raw_ostream;

  /// DisablePrettyStackTrace - Set this to true to disable this module. This
  /// might be necessary if the host application installs its own signal
  /// handlers which conflict with the ones installed by this module.
  /// Defaults to false.
  extern bool DisablePrettyStackTrace;

  /// PrettyStackTraceEntry - This class is used to represent a frame of the
  /// "pretty" stack trace that is dumped when a program crashes. You can define
  /// subclasses of this and declare them on the program stack: when they are
  /// constructed and destructed, they will add their symbolic frames to a
  /// virtual stack trace.  This gets dumped out if the program crashes.
  class PrettyStackTraceEntry {
    const PrettyStackTraceEntry *NextEntry;
    PrettyStackTraceEntry(const PrettyStackTraceEntry &); // DO NOT IMPLEMENT
    void operator=(const PrettyStackTraceEntry&);         // DO NOT IMPLEMENT
  public:
    PrettyStackTraceEntry();
    virtual ~PrettyStackTraceEntry();

    /// print - Emit information about this stack frame to OS.
    virtual void print(raw_ostream &OS) const = 0;

    /// getNextEntry - Return the next entry in the list of frames.
    const PrettyStackTraceEntry *getNextEntry() const { return NextEntry; }
  };

  /// PrettyStackTraceString - This object prints a specified string (which
  /// should not contain newlines) to the stream as the stack trace when a crash
  /// occurs.
  class PrettyStackTraceString : public PrettyStackTraceEntry {
    const char *Str;
  public:
    PrettyStackTraceString(const char *str) : Str(str) {}
    virtual void print(raw_ostream &OS) const;
  };

  /// PrettyStackTraceProgram - This object prints a specified program arguments
  /// to the stream as the stack trace when a crash occurs.
  class PrettyStackTraceProgram : public PrettyStackTraceEntry {
    int ArgC;
    const char *const *ArgV;
  public:
    PrettyStackTraceProgram(int argc, const char * const*argv)
      : ArgC(argc), ArgV(argv) {}
    virtual void print(raw_ostream &OS) const;
  };

} // end namespace llvm

#endif
