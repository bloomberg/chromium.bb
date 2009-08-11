#include <errno.h>
#include <fcntl.h>
#include <iostream>
#include <linux/unistd.h>
#include <signal.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "library.h"
#include "maps.h"
#include "sandbox_impl.h"

namespace playground {

Maps::Maps(const std::string& maps_file) :
    maps_file_(maps_file),
    begin_iter_(this, true, false),
    end_iter_(this, false, true),
    pid_(-1),
    vsyscall_(0) {
  memset(fds_, -1, sizeof(fds_));
  int fd = open(maps_file.c_str(), O_RDONLY);
  Sandbox::SysCalls sys;
  if (fd >= 0) {
    char buf[256] = { 0 };
    int len = 0, rc = 1;
    bool long_line = false;
    do {
      if (rc > 0) {
        rc = Sandbox::read(sys, fd, buf + len, sizeof(buf) - len - 1);
        if (rc > 0) {
          len += rc;
        }
      }
      char *ptr = buf;
      if (!long_line) {
        long_line = true;
        unsigned long start = strtoul(ptr, &ptr, 16);
        unsigned long stop = strtoul(ptr + 1, &ptr, 16);
        while (*ptr == ' ' || *ptr == '\t') ++ptr;
        char *perm_ptr = ptr;
        while (*ptr && *ptr != ' ' && *ptr != '\t') ++ptr;
        std::string perm(perm_ptr, ptr - perm_ptr);
        unsigned long offset = strtoul(ptr, &ptr, 16);
        while (*ptr == ' ' || *ptr == '\t') ++ptr;
        char *id_ptr = ptr;
        while (*ptr && *ptr != ' ' && *ptr != '\t') ++ptr;
        while (*ptr == ' ' || *ptr == '\t') ++ptr;
        while (*ptr && *ptr != ' ' && *ptr != '\t') ++ptr;
        std::string id(id_ptr, ptr - id_ptr);
        while (*ptr == ' ' || *ptr == '\t') ++ptr;
        char *library_ptr = ptr;
        while (*ptr && *ptr != ' ' && *ptr != '\t' && *ptr != '\n') ++ptr;
        std::string library(library_ptr, ptr - library_ptr);
        bool isVDSO = false;
        if (library == "[vdso]") {
          // /proc/self/maps has a misleading file offset in the [vdso] entry.
          // Override it with a sane value.
          offset = 0;
          isVDSO = true;
        } else if (library == "[vsyscall]") {
          vsyscall_ = reinterpret_cast<char *>(start);
        } else if (library.empty() || library[0] == '[') {
          goto skip_entry;
        }
        int prot = 0;
        if (perm.find('r') != std::string::npos) {
          prot |= PROT_READ;
        }
        if (perm.find('w') != std::string::npos) {
          prot |= PROT_WRITE;
        }
        if (perm.find('x') != std::string::npos) {
          prot |= PROT_EXEC;
        }
        if ((prot & (PROT_EXEC | PROT_READ)) == 0) {
          goto skip_entry;
        }
        libs_[id + ' ' + library].addMemoryRange(
            reinterpret_cast<void *>(start),
            reinterpret_cast<void *>(stop),
            Elf_Addr(offset),
            prot, isVDSO);
      }
   skip_entry:
      for (;;) {
        if (!*ptr || *ptr++ == '\n') {
          long_line = false;
          memmove(buf, ptr, len - (ptr - buf));
          memset(buf + len - (ptr - buf), 0, ptr - buf);
          len -= (ptr - buf);
          break;
        }
      }
    } while (len || long_line);
    NOINTR_SYS(close(fd));

    // The runtime loader clobbers some of the data that we want to read,
    // when it relocates objects. As we cannot trust the filename that we
    // obtained from /proc/self/maps, we instead fork() a child process and
    // use mremap() to uncover the obscured data.
    int tmp_fds[4];
    pipe(tmp_fds);
    pipe(tmp_fds + 2);
    pid_ = fork();
    if (pid_ >= 0) {
      // Set up read and write file descriptors for exchanging data
      // between parent and child.
      fds_[ !pid_] = tmp_fds[     !pid_];
      fds_[!!pid_] = tmp_fds[2 + !!pid_];
      NOINTR_SYS(close(  tmp_fds[    !!pid_]));
      NOINTR_SYS(close(  tmp_fds[2 +  !pid_]));

      for (LibraryMap::iterator iter = libs_.begin(); iter != libs_.end(); ){
        Library* lib = &iter->second;
        if (pid_) {
          lib->recoverOriginalDataParent(this);
        } else {
          lib->recoverOriginalDataChild(strrchr(iter->first.c_str(), ' ') + 1);
        }
        if (pid_ && !lib->parseElf()) {
          libs_.erase(iter++);
        } else {
          ++iter;
        }
      }

      // Handle requests sent from the parent to the child
      if (!pid_) {
        Request req;
        for (;;) {
          if (Sandbox::read(sys, fds_[0], &req, sizeof(Request)) !=
              sizeof(Request)) {
            _exit(0);
          }
          switch (req.type) {
            case Request::REQ_GET:
              {
                char *buf = new char[req.length];
                if (!req.library->get(req.offset, buf, req.length)) {
                  req.length = -1;
                  Sandbox::write(sys, fds_[1], &req.length,sizeof(req.length));
                } else {
                  Sandbox::write(sys, fds_[1], &req.length,sizeof(req.length));
                  Sandbox::write(sys, fds_[1], buf, req.length);
                }
                delete[] buf;
              }
              break;
            case Request::REQ_GET_STR:
              {
                std::string s = req.library->get(req.offset);
                req.length = s.length();
                Sandbox::write(sys, fds_[1], &req.length, sizeof(req.length));
                Sandbox::write(sys, fds_[1], s.c_str(), req.length);
              }
              break;
          }
        }
      }
    } else {
      for (int i = 0; i < 4; i++) {
        NOINTR_SYS(close(tmp_fds[i]));
      }
    }
  }
}

Maps::~Maps() {
  Sandbox::SysCalls sys;
  sys.kill(pid_, SIGKILL);
  sys.waitpid(pid_, NULL, 0);
}

char *Maps::forwardGetRequest(Library *library, Elf_Addr offset,
                              char *buf, size_t length) const {
  Request req(Request::REQ_GET, library, offset, length);
  Sandbox::SysCalls sys;
  if (Sandbox::write(sys, fds_[1], &req, sizeof(Request)) != sizeof(Request) ||
      Sandbox::read(sys, fds_[0], &req.length, sizeof(req.length)) !=
      sizeof(req.length) ||
      req.length == -1 ||
      Sandbox::read(sys, fds_[0], buf, length) != (ssize_t)length) {
    memset(buf, 0, length);
    return NULL;
  }
  return buf;
}

std::string Maps::forwardGetRequest(Library *library,
                                    Elf_Addr offset) const {
  Request req(Request::REQ_GET_STR, library, offset, -1);
  Sandbox::SysCalls sys;
  if (Sandbox::write(sys, fds_[1], &req, sizeof(Request)) != sizeof(Request) ||
      Sandbox::read(sys, fds_[0], &req.length, sizeof(req.length)) !=
      sizeof(req.length)) {
    return "";
  }
  char *buf = new char[req.length];
  if (Sandbox::read(sys, fds_[0], buf, req.length) != (ssize_t)req.length) {
    delete[] buf;
    return "";
  }
  std::string s(buf, req.length);
  delete[] buf;
  return s;
}

Maps::Iterator::Iterator(Maps* maps, bool at_beginning, bool at_end)
    : maps_(maps),
      at_beginning_(at_beginning),
      at_end_(at_end) {
}

Maps::LibraryMap::iterator& Maps::Iterator::getIterator() const {
  if (at_beginning_) {
    iter_ = maps_->libs_.begin();
  } else if (at_end_) {
    iter_ = maps_->libs_.end();
  }
  return iter_;
}

Maps::Iterator Maps::Iterator::begin() {
  return maps_->begin_iter_;
}

Maps::Iterator Maps::Iterator::end() {
  return maps_->end_iter_;
}

Maps::Iterator& Maps::Iterator::operator++() {
  getIterator().operator++();
  at_beginning_ = false;
  return *this;
}

Maps::Iterator Maps::Iterator::operator++(int i) {
  getIterator().operator++(i);
  at_beginning_ = false;
  return *this;
}

Library* Maps::Iterator::operator*() const {
  return &getIterator().operator*().second;
}

bool Maps::Iterator::operator==(const Maps::Iterator& iter) const {
  return getIterator().operator==(iter.getIterator());
}

bool Maps::Iterator::operator!=(const Maps::Iterator& iter) const {
  return !operator==(iter);
}

std::string Maps::Iterator::name() const {
  return getIterator()->first;
}

char* Maps::allocNearAddr(char* addr, size_t size, int prot) const {
  // We try to allocate memory within 1.5GB of a target address. This means,
  // we will be able to perform relative 32bit jumps from the target address.
  size = (size + 4095) & ~4095;
  Sandbox::SysCalls sys;
  int fd = sys.open(maps_file_.c_str(), O_RDONLY, 0);
  if (fd < 0) {
    return NULL;
  }

  char buf[256] = { 0 };
  int len = 0, rc = 1;
  bool long_line = false;
  unsigned long gap_start = 0x10000;
  char *new_addr;
  do {
    if (rc > 0) {
      do {
        rc = Sandbox::read(sys, fd, buf + len, sizeof(buf) - len - 1);
        if (rc > 0) {
          len += rc;
        }
      } while (rc > 0 && len < (int)sizeof(buf) - 1);
    }
    char *ptr = buf;
    if (!long_line) {
      long_line = true;
      unsigned long start = strtoul(ptr, &ptr, 16);
      unsigned long stop = strtoul(ptr + 1, &ptr, 16);
      if (start - gap_start >= size) {
        if (reinterpret_cast<long>(addr) - static_cast<long>(start) >= 0) {
          if (reinterpret_cast<long>(addr) - (start - size) < (1536 << 20)) {
            new_addr = reinterpret_cast<char *>(sys.MMAP
                           (reinterpret_cast<void *>(start - size), size, prot,
                            MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED, -1, 0));
            if (new_addr != MAP_FAILED) {
              goto done;
            }
          }
        } else if (gap_start + size - reinterpret_cast<long>(addr) <
                   (1536 << 20)) {
          new_addr = reinterpret_cast<char *>(sys.MMAP
                         (reinterpret_cast<void *>(gap_start), size, prot,
                          MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED, -1 ,0));
          if (new_addr != MAP_FAILED) {
            goto done;
          }
        }
      }
      gap_start = stop;
    }
    for (;;) {
      if (!*ptr || *ptr++ == '\n') {
        long_line = false;
        memmove(buf, ptr, len - (ptr - buf));
        memset(buf + len - (ptr - buf), 0, ptr - buf);
        len -= (ptr - buf);
        break;
      }
    }
  } while (len || long_line);
  new_addr = NULL;
done:
  sys.close(fd);
  return new_addr;
}

} // namespace
