/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "src/profiling/memory/client.h"

#include <inttypes.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>
#include <unwindstack/MachineArm.h>
#include <unwindstack/MachineArm64.h>
#include <unwindstack/MachineMips.h>
#include <unwindstack/MachineMips64.h>
#include <unwindstack/MachineX86.h>
#include <unwindstack/MachineX86_64.h>
#include <unwindstack/Regs.h>
#include <unwindstack/RegsGetLocal.h>

#include <atomic>
#include <new>

#include "perfetto/base/logging.h"
#include "perfetto/base/scoped_file.h"
#include "perfetto/base/thread_utils.h"
#include "perfetto/base/time.h"
#include "perfetto/base/unix_socket.h"
#include "perfetto/base/utils.h"
#include "src/profiling/memory/sampler.h"
#include "src/profiling/memory/scoped_spinlock.h"
#include "src/profiling/memory/wire_protocol.h"

namespace perfetto {
namespace profiling {
namespace {

const char kSingleByte[1] = {'x'};
constexpr std::chrono::seconds kLockTimeout{1};
constexpr auto kResendBackoffUs = 100;

inline bool IsMainThread() {
  return getpid() == base::GetThreadId();
}

// TODO(b/117203899): Remove this after making bionic implementation safe to
// use.
char* FindMainThreadStack() {
  base::ScopedFstream maps(fopen("/proc/self/maps", "r"));
  if (!maps) {
    return nullptr;
  }
  while (!feof(*maps)) {
    char line[1024];
    char* data = fgets(line, sizeof(line), *maps);
    if (data != nullptr && strstr(data, "[stack]")) {
      char* sep = strstr(data, "-");
      if (sep == nullptr)
        continue;
      sep++;
      return reinterpret_cast<char*>(strtoll(sep, nullptr, 16));
    }
  }
  return nullptr;
}

int UnsetDumpable(int) {
  prctl(PR_SET_DUMPABLE, 0);
  return 0;
}

}  // namespace

const char* GetThreadStackBase() {
  pthread_attr_t attr;
  if (pthread_getattr_np(pthread_self(), &attr) != 0)
    return nullptr;
  base::ScopedResource<pthread_attr_t*, pthread_attr_destroy, nullptr> cleanup(
      &attr);

  char* stackaddr;
  size_t stacksize;
  if (pthread_attr_getstack(&attr, reinterpret_cast<void**>(&stackaddr),
                            &stacksize) != 0)
    return nullptr;
  return stackaddr + stacksize;
}

// static
base::Optional<base::UnixSocketRaw> Client::ConnectToHeapprofd(
    const std::string& sock_name) {
  auto sock = base::UnixSocketRaw::CreateMayFail(base::SockType::kStream);
  if (!sock || !sock.Connect(sock_name)) {
    PERFETTO_PLOG("Failed to connect to %s", sock_name.c_str());
    return base::nullopt;
  }
  if (!sock.SetTxTimeout(kClientSockTimeoutMs)) {
    PERFETTO_PLOG("Failed to set send timeout for %s", sock_name.c_str());
    return base::nullopt;
  }
  if (!sock.SetRxTimeout(kClientSockTimeoutMs)) {
    PERFETTO_PLOG("Failed to set receive timeout for %s", sock_name.c_str());
    return base::nullopt;
  }
  return std::move(sock);
}

// static
std::shared_ptr<Client> Client::CreateAndHandshake(
    base::UnixSocketRaw sock,
    UnhookedAllocator<Client> unhooked_allocator) {
  if (!sock) {
    PERFETTO_DFATAL_OR_ELOG("Socket not connected.");
    return nullptr;
  }

  PERFETTO_DCHECK(sock.IsBlocking());

  // We might be running in a process that is not dumpable (such as app
  // processes on user builds), in which case the /proc/self/mem will be chown'd
  // to root:root, and will not be accessible even to the process itself (see
  // man 5 proc). In such situations, temporarily mark the process dumpable to
  // be able to open the files, unsetting dumpability immediately afterwards.
  int orig_dumpable = prctl(PR_GET_DUMPABLE);

  enum { kNop, kDoUnset };
  base::ScopedResource<int, UnsetDumpable, kNop, false> unset_dumpable(kNop);
  if (orig_dumpable == 0) {
    unset_dumpable.reset(kDoUnset);
    prctl(PR_SET_DUMPABLE, 1);
  }

  base::ScopedFile maps(base::OpenFile("/proc/self/maps", O_RDONLY));
  if (!maps) {
    PERFETTO_DFATAL_OR_ELOG("Failed to open /proc/self/maps");
    return nullptr;
  }
  base::ScopedFile mem(base::OpenFile("/proc/self/mem", O_RDONLY));
  if (!mem) {
    PERFETTO_DFATAL_OR_ELOG("Failed to open /proc/self/mem");
    return nullptr;
  }
  base::ScopedFile pagemap(base::OpenFile("/proc/self/pagemap", O_RDONLY));
  if (!pagemap) {
    PERFETTO_DFATAL_OR_ELOG("Failed to open /proc/self/pagemap");
    return nullptr;
  }

  // Restore original dumpability value if we overrode it.
  unset_dumpable.reset();

  int fds[kHandshakeSize];
  fds[kHandshakeMaps] = *maps;
  fds[kHandshakeMem] = *mem;
  fds[kHandshakePageMap] = *pagemap;

  // Send an empty record to transfer fds for /proc/self/maps and
  // /proc/self/mem.
  if (sock.Send(kSingleByte, sizeof(kSingleByte), fds, kHandshakeSize) !=
      sizeof(kSingleByte)) {
    PERFETTO_DFATAL_OR_ELOG("Failed to send file descriptors.");
    return nullptr;
  }

  ClientConfiguration client_config;
  base::ScopedFile shmem_fd;
  size_t recv = 0;
  while (recv < sizeof(client_config)) {
    size_t num_fds = 0;
    base::ScopedFile* fd = nullptr;
    if (!shmem_fd) {
      num_fds = 1;
      fd = &shmem_fd;
    }
    ssize_t rd = sock.Receive(reinterpret_cast<char*>(&client_config) + recv,
                              sizeof(client_config) - recv, fd, num_fds);
    if (rd == -1) {
      PERFETTO_PLOG("Failed to receive ClientConfiguration.");
      return nullptr;
    }
    if (rd == 0) {
      PERFETTO_LOG("Server disconnected while sending ClientConfiguration.");
      return nullptr;
    }
    recv += static_cast<size_t>(rd);
  }

  if (!shmem_fd) {
    PERFETTO_DFATAL_OR_ELOG("Did not receive shmem fd.");
    return nullptr;
  }

  auto shmem = SharedRingBuffer::Attach(std::move(shmem_fd));
  if (!shmem || !shmem->is_valid()) {
    PERFETTO_DFATAL_OR_ELOG("Failed to attach to shmem.");
    return nullptr;
  }

  PERFETTO_DCHECK(client_config.interval >= 1);
  // TODO(fmayer): Always make this nonblocking.
  // This is so that without block_client, we get the old behaviour that rate
  // limits using the blocking socket. We do not want to change that for Q.
  sock.SetBlocking(!client_config.block_client);
  Sampler sampler{client_config.interval};
  // note: the shared_ptr will retain a copy of the unhooked_allocator
  return std::allocate_shared<Client>(unhooked_allocator, std::move(sock),
                                      client_config, std::move(shmem.value()),
                                      std::move(sampler), getpid(),
                                      FindMainThreadStack());
}

Client::Client(base::UnixSocketRaw sock,
               ClientConfiguration client_config,
               SharedRingBuffer shmem,
               Sampler sampler,
               pid_t pid_at_creation,
               const char* main_thread_stack_base)
    : client_config_(client_config),
      sampler_(std::move(sampler)),
      sock_(std::move(sock)),
      main_thread_stack_base_(main_thread_stack_base),
      shmem_(std::move(shmem)),
      pid_at_creation_(pid_at_creation) {}

const char* Client::GetStackBase() {
  if (IsMainThread()) {
    if (!main_thread_stack_base_)
      // Because pthread_attr_getstack reads and parses /proc/self/maps and
      // /proc/self/stat, we have to cache the result here.
      main_thread_stack_base_ = GetThreadStackBase();
    return main_thread_stack_base_;
  }
  return GetThreadStackBase();
}

// The stack grows towards numerically smaller addresses, so the stack layout
// of main calling malloc is as follows.
//
//               +------------+
//               |SendWireMsg |
// stacktop +--> +------------+ 0x1000
//               |RecordMalloc|    +
//               +------------+    |
//               | malloc     |    |
//               +------------+    |
//               |  main      |    v
// stackbase +-> +------------+ 0xffff
bool Client::RecordMalloc(uint64_t alloc_size,
                          uint64_t total_size,
                          uint64_t alloc_address) {
  if (PERFETTO_UNLIKELY(getpid() != pid_at_creation_)) {
    PERFETTO_LOG("Detected post-fork child situation, stopping profiling.");
    return false;
  }

  AllocMetadata metadata;
  const char* stackbase = GetStackBase();
  const char* stacktop = reinterpret_cast<char*>(__builtin_frame_address(0));
  unwindstack::AsmGetRegs(metadata.register_data);

  if (PERFETTO_UNLIKELY(stackbase < stacktop)) {
    PERFETTO_DFATAL_OR_ELOG("Stackbase >= stacktop.");
    return false;
  }

  uint64_t stack_size = static_cast<uint64_t>(stackbase - stacktop);
  metadata.total_size = total_size;
  metadata.alloc_size = alloc_size;
  metadata.alloc_address = alloc_address;
  metadata.stack_pointer = reinterpret_cast<uint64_t>(stacktop);
  metadata.stack_pointer_offset = sizeof(AllocMetadata);
  metadata.arch = unwindstack::Regs::CurrentArch();
  metadata.sequence_number =
      1 + sequence_number_.fetch_add(1, std::memory_order_acq_rel);

  struct timespec ts;
  if (clock_gettime(CLOCK_MONOTONIC_COARSE, &ts) == 0) {
    metadata.clock_monotonic_coarse_timestamp =
        static_cast<uint64_t>(base::FromPosixTimespec(ts).count());
  } else {
    metadata.clock_monotonic_coarse_timestamp = 0;
  }

  WireMessage msg{};
  msg.record_type = RecordType::Malloc;
  msg.alloc_header = &metadata;
  msg.payload = const_cast<char*>(stacktop);
  msg.payload_size = static_cast<size_t>(stack_size);

  if (!SendWireMessageWithRetriesIfBlocking(msg))
    return false;

  return SendControlSocketByte();
}

bool Client::SendWireMessageWithRetriesIfBlocking(const WireMessage& msg) {
  for (;;) {
    if (PERFETTO_LIKELY(SendWireMessage(&shmem_, msg)))
      return true;
    // retry if in blocking mode and still connected
    if (client_config_.block_client && base::IsAgain(errno) && IsConnected()) {
      usleep(kResendBackoffUs);
      continue;
    }
    PERFETTO_PLOG("Failed to write to shared ring buffer. Disconnecting.");
    return false;
  }
}

bool Client::RecordFree(const uint64_t alloc_address) {
  uint64_t sequence_number =
      1 + sequence_number_.fetch_add(1, std::memory_order_acq_rel);

  std::unique_lock<std::timed_mutex> l(free_batch_lock_, kLockTimeout);
  if (!l.owns_lock())
    return false;
  if (free_batch_.num_entries == kFreeBatchSize) {
    if (!FlushFreesLocked())
      return false;
    // Flushed the contents of the buffer, reset it for reuse.
    free_batch_.num_entries = 0;
  }
  FreeBatchEntry& current_entry =
      free_batch_.entries[free_batch_.num_entries++];
  current_entry.sequence_number = sequence_number;
  current_entry.addr = alloc_address;
  return true;
}

bool Client::FlushFreesLocked() {
  if (PERFETTO_UNLIKELY(getpid() != pid_at_creation_)) {
    PERFETTO_LOG("Detected post-fork child situation, stopping profiling.");
    return false;
  }

  WireMessage msg = {};
  msg.record_type = RecordType::Free;
  msg.free_header = &free_batch_;
  struct timespec ts;
  if (clock_gettime(CLOCK_MONOTONIC_COARSE, &ts) == 0) {
    free_batch_.clock_monotonic_coarse_timestamp =
        static_cast<uint64_t>(base::FromPosixTimespec(ts).count());
  } else {
    free_batch_.clock_monotonic_coarse_timestamp = 0;
  }

  if (!SendWireMessageWithRetriesIfBlocking(msg))
    return false;
  return SendControlSocketByte();
}

bool Client::IsConnected() {
  PERFETTO_DCHECK(!sock_.IsBlocking());
  char buf[1];
  ssize_t recv_bytes = sock_.Receive(buf, sizeof(buf), nullptr, 0);
  if (recv_bytes == 0)
    return false;
  // This is not supposed to happen because currently heapprofd does not send
  // data to the client. Here for generality's sake.
  if (recv_bytes > 0)
    return true;
  return base::IsAgain(errno);
}

bool Client::SendControlSocketByte() {
  // TODO(fmayer): Fix the special casing that only block_client uses a
  // nonblocking socket.
  if (sock_.Send(kSingleByte, sizeof(kSingleByte)) == -1 &&
      (!client_config_.block_client || !base::IsAgain(errno))) {
    PERFETTO_PLOG("Failed to send control socket byte.");
    return false;
  }
  return true;
}

}  // namespace profiling
}  // namespace perfetto
