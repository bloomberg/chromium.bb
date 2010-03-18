// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NDEBUG

#include "debug.h"

namespace playground {

bool Debug::enabled_;
int  Debug::numSyscallNames_;
const char **Debug::syscallNames_;
std::map<int, std::string> Debug::syscallNamesMap_;

Debug Debug::debug_;

Debug::Debug() {
  // Logging is disabled by default, but can be turned on by setting an
  // appropriate environment variable. Initialize this code from a global
  // constructor, so that it runs before the sandbox is turned on.
  enabled_ = !!getenv("SECCOMP_SANDBOX_DEBUGGING");

  // Read names of system calls from header files, if available. Symbolic
  // names make debugging so much nicer.
  if (enabled_) {
    static const char *filenames[] = {
      #if __WORDSIZE == 64
      "/usr/include/asm/unistd_64.h",
      #elif __WORDSIZE == 32
      "/usr/include/asm/unistd_32.h",
      #endif
      "/usr/include/asm/unistd.h",
      NULL };
    numSyscallNames_ = 0;
    for (const char **fn = filenames; *fn; ++fn) {
      FILE *fp = fopen(*fn, "r");
      if (fp) {
        std::string baseName;
        int         baseNum = -1;
        char buf[80];
        while (fgets(buf, sizeof(buf), fp)) {
          // Check if the line starts with "#define"
          static const char* whitespace = " \t\r\n";
          char *token, *save;
          token = strtok_r(buf, whitespace, &save);
          if (token && !strcmp(token, "#define")) {

            // Only parse identifiers that start with "__NR_"
            token = strtok_r(NULL, whitespace, &save);
            if (token) {
              if (strncmp(token, "__NR_", 5)) {
                continue;
              }
              std::string syscallName(token + 5);

              // Parse the value of the symbol. Try to be forgiving in what
              // we accept, as the file format might change over time.
              token = strtok_r(NULL, "\r\n", &save);
              if (token) {
                // Some values are defined relative to previous values, we
                // detect these examples by finding an earlier symbol name
                // followed by a '+' plus character.
                bool isRelative = false;
                char *base = strstr(token, baseName.c_str());
                if (baseNum >= 0 && base) {
                  base += baseName.length();
                  while (*base == ' ' || *base == '\t') {
                    ++base;
                  }
                  if (*base == '+') {
                    isRelative = true;
                    token = base;
                  }
                }

                // Skip any characters that are not part of the syscall number.
                while (*token < '0' || *token > '9') {
                  token++;
                }

                // If we now have a valid datum, enter it into our map.
                if (*token) {
                  int sysnum = atoi(token);

                  // Deal with symbols that are defined relative to earlier
                  // ones.
                  if (isRelative) {
                    sysnum += baseNum;
                  } else {
                    baseNum  = sysnum;
                    baseName = syscallName;
                  }

                  // Keep track of the highest syscall number that we know
                  // about.
                  if (sysnum >= numSyscallNames_) {
                    numSyscallNames_ = sysnum + 1;
                  }

                  syscallNamesMap_[sysnum] = syscallName;
                }
              }
            }
          }
        }
        fclose(fp);
        break;
      }
    }
    if (numSyscallNames_) {
      // We cannot make system calls at the time, when we are looking up
      // the names. So, copy them into a data structure that can be
      // accessed without having to allocated memory (i.e. no more STL).
      syscallNames_ = reinterpret_cast<const char **>(
          calloc(sizeof(char *), numSyscallNames_));
      for (std::map<int, std::string>::const_iterator iter =
               syscallNamesMap_.begin();
           iter != syscallNamesMap_.end();
           ++iter) {
        syscallNames_[iter->first] = iter->second.c_str();
      }
    }
  }
}

bool Debug::enter() {
  // Increment the recursion level in TLS storage. This allows us to
  // make system calls from within our debugging functions, without triggering
  // additional debugging output.
  //
  // This function can be called from both the sandboxed process and from the
  // trusted process. Only the sandboxed process needs to worry about
  // recursively calling system calls. The trusted process doesn't intercept
  // system calls and thus doesn't have this problem. It also doesn't have
  // a TLS. We explicitly set the segment selector to zero, when in the
  // trusted process, so that we can avoid tracking recursion levels.
  int level;
  #if defined(__x86_64__)
  asm volatile("mov  %%gs, %0\n"
               "test %0, %0\n"
               "jz   1f\n"
               "movl %%gs:0x1050-0xD8, %0\n"
               "incl %%gs:0x1050-0xD8\n"
             "1:\n"
               : "=r"(level)
               :
               : "memory");
  #elif defined(__i386__)
  asm volatile("mov  %%fs, %0\n"
               "test %0, %0\n"
               "jz   1f\n"
               "movl %%fs:0x1034-0x54, %0\n"
               "incl %%fs:0x1034-0x54\n"
             "1:\n"
               : "=r"(level)
               :
               : "memory");
  #else
  #error "Unsupported target platform"
  #endif
  return !level;
}

bool Debug::leave() {
  // Decrement the recursion level in TLS storage. This allows us to
  // make system calls from within our debugging functions, without triggering
  // additional debugging output.
  //
  // This function can be called from both the sandboxed process and from the
  // trusted process. Only the sandboxed process needs to worry about
  // recursively calling system calls. The trusted process doesn't intercept
  // system calls and thus doesn't have this problem. It also doesn't have
  // a TLS. We explicitly set the segment selector to zero, when in the
  // trusted process, so that we can avoid tracking recursion levels.
  int level;
  #if defined(__x86_64__)
  asm volatile("mov  %%gs, %0\n"
               "test %0, %0\n"
               "jz   1f\n"
               "decl %%gs:0x1050-0xD8\n"
               "movl %%gs:0x1050-0xD8, %0\n"
             "1:\n"
               : "=r"(level)
               :
               : "memory");
  #elif defined(__i386__)
  asm volatile("mov  %%fs, %0\n"
               "test %0, %0\n"
               "jz   1f\n"
               "decl %%fs:0x1034-0x54\n"
               "movl %%fs:0x1034-0x54, %0\n"
             "1:\n"
               : "=r"(level)
               :
               : "memory");
  #else
  #error Unsupported target platform
  #endif
  return !level;
}

void Debug::_message(const char* msg) {
  if (enabled_) {
    Sandbox::SysCalls sys;
    size_t len = strlen(msg);
    if (len && msg[len-1] != '\n') {
      // Write operations should be atomic, so that we don't interleave
      // messages from multiple threads. Append a newline, if it is not
      // already there.
      char copy[len + 1];
      memcpy(copy, msg, len);
      copy[len] = '\n';
      Sandbox::write(sys, 2, copy, len + 1);
    } else {
      Sandbox::write(sys, 2, msg, len);
    }
  }
}

void Debug::message(const char* msg) {
  if (enabled_) {
    if (enter()) {
      _message(msg);
    }
    leave();
  }
}

void Debug::gettimeofday(long long* tm) {
  if (tm) {
    struct timeval tv;
    #if defined(__i386__)
    // Zero out the lastSyscallNum, so that we don't try to coalesce
    // calls to gettimeofday(). For debugging purposes, we need the
    // exact time.
    asm volatile("movl $0, %fs:0x102C-0x54");
    #elif !defined(__x86_64__)
    #error Unsupported target platform
    #endif
    ::gettimeofday(&tv, NULL);
    *tm = 1000ULL*1000ULL*static_cast<unsigned>(tv.tv_sec) +
          static_cast<unsigned>(tv.tv_usec);
  }
}

void Debug::syscall(long long* tm, int sysnum, const char* msg, int call) {
  // This function gets called from the system call wrapper. Avoid calling
  // any library functions that themselves need system calls.
  if (enabled_) {
    if (enter() || !tm) {
      gettimeofday(tm);

      const char *sysname = NULL;
      if (sysnum >= 0 && sysnum < numSyscallNames_) {
        sysname = syscallNames_[sysnum];
      }
      static const char kUnnamedMessage[] = "Unnamed syscall #";
      char unnamed[40];
      if (!sysname) {
        memcpy(unnamed, kUnnamedMessage, sizeof(kUnnamedMessage) - 1);
        itoa(unnamed + sizeof(kUnnamedMessage) - 1, sysnum);
        sysname = unnamed;
      }
      #if defined(__NR_socketcall) || defined(__NR_ipc)
      char extra[40];
      *extra = '\000';
      #if defined(__NR_socketcall)
      if (sysnum == __NR_socketcall) {
        static const char* socketcall_name[] = {
          0, "socket", "bind", "connect", "listen", "accept", "getsockname",
          "getpeername", "socketpair", "send", "recv", "sendto","recvfrom",
          "shutdown", "setsockopt", "getsockopt", "sendmsg", "recvmsg",
          "accept4"
        };
        if (call >= 1 &&
            call < (int)(sizeof(socketcall_name)/sizeof(char *))) {
          strcat(strcpy(extra, " "), socketcall_name[call]);
        } else {
          itoa(strcpy(extra, " #") + 2, call);
        }
      }
      #endif
      #if defined(__NR_ipc)
      if (sysnum == __NR_ipc) {
        static const char* ipc_name[] = {
          0, "semop", "semget", "semctl", "semtimedop", 0, 0, 0, 0, 0, 0,
          "msgsnd", "msgrcv", "msgget", "msgctl", 0, 0, 0, 0, 0, 0,
          "shmat", "shmdt", "shmget", "shmctl" };
        if (call >= 1 && call < (int)(sizeof(ipc_name)/sizeof(char *)) &&
            ipc_name[call]) {
          strcat(strcpy(extra, " "), ipc_name[call]);
        } else {
          itoa(strcpy(extra, " #") + 2, call);
        }
      }
      #endif
      #else
      static const char extra[1] = { 0 };
      #endif
      char buf[strlen(sysname) + strlen(extra) + (msg ? strlen(msg) : 0) + 4];
      strcat(strcat(strcat(strcat(strcpy(buf, sysname), extra), ": "),
                    msg ? msg : ""), "\n");
      _message(buf);
    }
    leave();
  }
}

char* Debug::itoa(char* s, int n) {
  // Remember return value
  char *ret   = s;

  // Insert sign for negative numbers
  if (n < 0) {
    *s++      = '-';
    n         = -n;
  }

  // Convert to decimal (in reverse order)
  char *start = s;
  do {
    *s++      = '0' + (n % 10);
    n        /= 10;
  } while (n);
  *s--        = '\000';

  // Reverse order of digits
  while (start < s) {
    char ch   = *s;
    *s--      = *start;
    *start++  = ch;
  }

  return ret;
}

void Debug::elapsed(long long tm, int sysnum, int call) {
  if (enabled_) {
    if (enter()) {
      // Compute the time that has passed since the system call started.
      long long delta;
      gettimeofday(&delta);
      delta -= tm;

      // Format "Elapsed time: %d.%03dms" without using sprintf().
      char buf[80];
      itoa(strrchr(strcpy(buf, "Elapsed time: "), '\000'), delta/1000);
      delta %= 1000;
      strcat(buf, delta < 100 ? delta < 10 ? ".00" : ".0" : ".");
      itoa(strrchr(buf, '\000'), delta);
      strcat(buf, "ms");

      // Print system call name and elapsed time.
      syscall(NULL, sysnum, buf, call);
    }
    leave();
  }
}

} // namespace

#endif // NDEBUG
