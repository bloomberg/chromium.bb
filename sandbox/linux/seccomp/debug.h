#ifndef DEBUG_H__
#define DEBUG_H__

#include <map>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <string.h>

#include "sandbox_impl.h"

namespace playground {

class Debug {
 public:
  // If debugging is enabled, write a message to stderr.
  static void message(const char* msg)
  #ifndef NDEBUG
  asm("playground$debugMessage");
  #else
  { }
  #endif

  // If debugging is enabled, write the name of the syscall and an optional
  // message to stderr.
  static void syscall(int sysnum, const char* msg, int call = -1)
  #ifndef NDEBUG
  ;
  #else
  { }
  #endif

  // Check whether debugging is enabled.
  static bool isEnabled() {
    #ifndef NDEBUG
    return enabled_;
    #else
    return false;
    #endif
  }

 private:
  #ifndef NDEBUG
  Debug();
  static char* itoa(char* s, int n);

  static Debug debug_;

  static bool  enabled_;
  static int  numSyscallNames_;
  static const char **syscallNames_;
  static std::map<int, std::string> syscallNamesMap_;
  #endif
};

} // namespace

#endif // DEBUG_H__
