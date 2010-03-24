/*
 * Copyright 2009 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

// Test that SIGPIPE is not raised when using nacl::SendDatagram or
// nacl::SendDatagramTo when the peer has been closed for various
// flavors of sockets.

#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <vector>

#include "native_client/src/include/checked_cast.h"
#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/nacl_string.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/include/portability_process.h"
#include "native_client/src/shared/imc/nacl_imc.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/shared/platform/nacl_sync.h"
#include "native_client/src/shared/platform/nacl_threads.h"


namespace {

bool gSleepBeforeReceive(false);
std::vector<int> gTestSequence;

/*
 * PickRandomSocketAddress: choses a random socket address.
 *
 * NB: this uses rand() and thus is not thread-safe.  However, this is
 * only used in the main thread.
 */
void PickRandomSocketAddress(nacl::SocketAddress *addr) {
  static const char alphabet[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
#if !NACL_WINDOWS
      "abcdefghijklmnopqrstuvwxyz"
#endif
      "0123456789";
  static bool seeded = false;  // not thread safe

  if (!(sizeof alphabet - 1 == 36 ||
        sizeof alphabet - 1 == 62)) {
    printf("Alphabet size error\n");
    abort();
  }
  if (!seeded) {
    srand(GETPID());
    seeded = 1;
  }
  for (int i = 0; i < nacl::kPathMax - 1; ++i) {
    addr->path[i] = alphabet[rand() % (sizeof alphabet - 1)];
  }
  addr->path[nacl::kPathMax - 1] = '\0';
  printf("PickRandomSocketAddress: returning %s\n", addr->path);
}


void MyPerror(nacl::string s) {
  char error_msg[512];
  int err = errno;
  if (0 == nacl::GetLastErrorString(error_msg, sizeof error_msg)) {
    printf("%s: %s\n", s.c_str(), error_msg);
  } else {
    printf("%s: errno %d\n", s.c_str(), err);
  }
  errno = err;
}


void SplitString(std::vector<nacl::string> *result, nacl::string s, char sep) {
  nacl::string::size_type start;
  nacl::string::size_type sep_pos;

  for (start = 0;
       nacl::string::npos != (sep_pos = s.find(sep, start));
       start = sep_pos + 1) {
    result->push_back(s.substr(start, sep_pos - start));
  }
  if (start < s.length()) {
    result->push_back(s.substr(start));
  }
}

void ApplyInt(std::vector<int> *result, std::vector<nacl::string> const &vs) {
  for (std::vector<nacl::string>::const_iterator it = vs.begin();
       vs.end() != it;
       ++it) {
    result->push_back(strtol((*it).c_str(), static_cast<char **>(0), 0));
  }
}

struct TestState {
  nacl::SocketAddress cli_addr;
  nacl::SocketAddress srv_addr;
  nacl::Handle cli_sock;
  nacl::Handle srv_sock;
  nacl::Handle pair[2];

  bool new_sock_only;  // false to run only the single thread/socket
                       // tests, true to do the complement
  int repetitions;
  int outer_rep;

  std::vector<int> *test_sequence;

  NaClMutex mu;
  NaClCondVar cv;
  int errors;
  int cur_test;

  int msg_len;
  char msg_buffer[512];

  TestState(std::vector<int> *, bool nso, int reps, int outer_rep);
  int Init();
  ~TestState();
};

TestState::TestState(std::vector<int> *seqp, bool nso, int reps,
                     int out_rep)
    : cli_sock(nacl::kInvalidHandle),
      srv_sock(nacl::kInvalidHandle),
      new_sock_only(nso),
      repetitions(reps),
      outer_rep(out_rep),
      test_sequence(seqp),
      errors(-1),
      cur_test(-1) {
  pair[0] = nacl::kInvalidHandle;
  pair[1] = nacl::kInvalidHandle;
  (void) NaClMutexCtor(&mu);
  (void) NaClCondVarCtor(&cv);
}

int TestState::Init() {
  PickRandomSocketAddress(&cli_addr);
  cli_sock = nacl::BoundSocket(&cli_addr);
  if (nacl::kInvalidHandle == cli_sock) {
    MyPerror("BoundSocket");
    printf("ERROR: No client socket\n");
    return 1;
  }
  PickRandomSocketAddress(&srv_addr);
  srv_sock = nacl::BoundSocket(&srv_addr);
  if (nacl::kInvalidHandle == srv_sock) {
    MyPerror("BoundSocket");
    printf("ERROR: No server socket\n");
    return 1;
  }

  if (-1 == nacl::SocketPair(pair)) {
    MyPerror("SocketPair");
    printf("ERROR: no socket pair\n");
    return 1;
  }

  strncpy(msg_buffer, "hello world\n", sizeof msg_buffer);
  msg_buffer[sizeof msg_buffer - 1] = '\0';
  msg_len = nacl::assert_cast<int>(strlen(msg_buffer));

  printf("cli_sock %d, srv_sock %d, pair[0] %d, pair[1] %d\n",
         cli_sock, srv_sock, pair[0], pair[1]);

  return 0;
}


TestState::~TestState() {
  if (nacl::kInvalidHandle != cli_sock) {
    printf("nacl::Close(%d)\n", cli_sock);
    (void) nacl::Close(cli_sock);
  }
  if (nacl::kInvalidHandle != srv_sock) {
    printf("nacl::Close(%d)\n", srv_sock);
    (void) nacl::Close(srv_sock);
  }
  for (int i = 0; i < 2; ++i) {
    if (nacl::kInvalidHandle != pair[i]) {
      printf("nacl::Close(%d)\n", pair[i]);
      (void) nacl::Close(pair[i]);
    }
  }
  (void) NaClCondVarDtor(&cv);
  (void) NaClMutexDtor(&mu);
}


int SendDescriptor(TestState *tsp, int mode) {
  int errors(0);
  nacl::MessageHeader hdr;
  nacl::Handle xfer[2];
  hdr.iov = NULL;
  hdr.iov_length = 0;
  hdr.handle_count = 1;
  hdr.handles = xfer;
      // &tsp->pair[0];   // bug w/ OSX, kernel oops
      // &tsp->cli_sock;  // bug w/ bound sockets, eager nacl::Close unlink
  if (-1 == nacl::SocketPair(xfer)) {  // an otherwise unused nacl::Handle.
    ++errors;
    printf("SendDescriptr: could not create (unused) nacl::SocketPair\n");
    return errors;
  }
  if (-1 == nacl::Close(xfer[1])) {
    ++errors;
    printf("SendDescriptor: could not nacl::Close the unused"
           " end of SocketPair\n");
    return errors;
  }
  hdr.flags = 0;
  printf("Sending a descriptor, mode %d\n", mode);
  int result(-1);
  nacl::string op;
  switch (mode) {
    case 0: {
      op = "SendDatagramTo";
      result = nacl::SendDatagramTo(tsp->cli_sock, &hdr, 0, &tsp->srv_addr);
      break;
    }
    case 1: {
      op = "SendDatagram";
      result = nacl::SendDatagram(tsp->pair[1], &hdr, 0);
      break;
    }
    default: {
      printf("ERROR: illegal test mode\n");
      ++errors;
      break;
    }
  }
  if (-1 == result) {
    MyPerror("SendDescriptor, " + op);
    printf("ERROR: SendDescriptor: %s failed.\n", op.c_str());
    ++errors;
  } else if (0 != result) {
    printf("ERROR: SendDescriptor: %s returned %d, expected 0.\n",
           op.c_str(),
           result);
    ++errors;
  } else {
    printf("SendDescriptor: OK\n");
  }
  nacl::Close(xfer[0]);
  return errors;
}


int ReceiveDescriptor(TestState *tsp, int mode) {
  int errors(0);
  nacl::MessageHeader hdr;
  nacl::IOVec vec;
  char buffer[512];
  nacl::Handle handle[8];
  int nbytes(-1);
  printf("ReceiverThread: receive a handle, mode %d\n", mode);
  vec.base = buffer;
  vec.length = sizeof buffer;
  hdr.iov = &vec;
  hdr.iov_length = 1;
  hdr.handles = handle;
  hdr.handle_count = NACL_ARRAY_SIZE(handle);
  hdr.flags = 0;
  switch (mode) {
    case 0: {
      nbytes = nacl::ReceiveDatagram(tsp->srv_sock, &hdr, 0);
      break;
    }
    case 1: {
      nbytes = nacl::ReceiveDatagram(tsp->pair[0], &hdr, 0);
      break;
    }
    default: {
      printf("ERROR: illegal test mode\n");
      ++errors;
      break;
    }
  }
  if (-1 == nbytes) {
    MyPerror("ReceiverThread, ReceiveDatagram");
    printf("ERROR: ReceiveDatagram failed, did not receive any handles.\n");
    ++errors;
  } else {
    if (0 != nbytes) {
      printf("ERROR: ReceiveDatagram should have received zero bytes"
             " of data, got %d.\n", nbytes);
      printf("ERROR: received \"%.*s\"\n", nbytes, buffer);
      ++errors;
    }
    if (1 != hdr.handle_count) {
      printf("ERROR: Did not receive a single handle.\n");
      ++errors;
    }

    if (NACL_ARRAY_SIZE(handle) < hdr.handle_count) {
      printf("ERROR: Too many handles: %"NACL_PRIu32, hdr.handle_count);
      return ++errors;
    }
    for (size_t i = 0; i < hdr.handle_count; ++i) {
      if (hdr.handles[i] == tsp->srv_sock) {
        printf("ERROR: received handle is same as srv_sock!\n");
        ++errors;
        continue;
      }
      if (hdr.handles[i] == tsp->cli_sock) {
        printf("ERROR: received handle is same as cli_sock!\n");
        ++errors;
        continue;
      }
      if (hdr.handles[i] == tsp->pair[0]) {
        printf("ERROR: received handle is same as pair[0]!\n");
        ++errors;
        continue;
      }
      if (hdr.handles[i] == tsp->pair[1]) {
        printf("ERROR: received handle is same as pair[1]!\n");
        ++errors;
        continue;
      }
      printf("close(%d)\n", hdr.handles[i]);
      if (-1 == nacl::Close(hdr.handles[i])) {
        MyPerror("ReceiverThread, Close");
        printf("ERROR: Close on received handle failed\n");
        ++errors;
      }
    }
  }
  if (0 == errors) {
    printf("ReceiverThread, receive handle: OK\n");
  } else {
    printf("ReceiverThread, receive handle: FAILED\n");
  }
  return errors;
}


int SendData(TestState *tsp, int mode) {
  int errors(0);
  nacl::MessageHeader hdr;
  nacl::IOVec vec;
  int nbytes(-1);
  vec.base = tsp->msg_buffer;
  vec.length = tsp->msg_len;
  hdr.iov = &vec;
  hdr.iov_length = 1;
  hdr.handle_count = 0;
  hdr.flags = 0;
  nacl::string op;

  printf("Sending data, mode %d\n", mode);
  switch (mode) {
    case 0: {
      op = "SendDatagramTo";
      nbytes = nacl::SendDatagramTo(tsp->cli_sock, &hdr, 0, &tsp->srv_addr);
      break;
    }
    case 1: {
      op = "SendDatagram";
      nbytes = nacl::SendDatagram(tsp->pair[1], &hdr, 0);
    } break;
    default: {
      printf("ERROR: Illegal test mode\n");
      ++errors;
      break;
    }
  }
  if (-1 == nbytes) {
    MyPerror("send thread " + op);
    printf("Send thread, %s failed\n", op.c_str());
  }
  if (nbytes != tsp->msg_len) {
    printf("ERROR: send thread, send data: %s did not send"
           " all bytes.  Tried to send %d, but only %d actually sent.\n",
           op.c_str(), tsp->msg_len, nbytes);
    if (0 == mode) {
      printf("sock addr %s\n", tsp->srv_addr.path);
    }
    ++errors;
  } else {
    printf("Send thread, send data: OK\n");
  }
  return errors;
}

int ReceiveData(TestState *tsp, int mode) {
  int errors(0);
  nacl::MessageHeader hdr;
  nacl::IOVec vec;

  nacl::Handle handle[8];
  int nbytes(-1);
  printf("ReceiverThread: receive data, mode %d\n", mode);
  char recv_buf[1024];
  memset(recv_buf, 0, sizeof recv_buf);
  vec.base = recv_buf;
  vec.length = sizeof recv_buf;
  hdr.iov = &vec;
  hdr.iov_length = 1;
  hdr.handles = handle;
  hdr.handle_count = NACL_ARRAY_SIZE(handle);
  hdr.flags = 0;
  switch (mode) {
    case 0: {
      nbytes = nacl::ReceiveDatagram(tsp->srv_sock, &hdr, 0);
      break;
    }
    case 1: {
      nbytes = nacl::ReceiveDatagram(tsp->pair[0], &hdr, 0);
      break;
    }
    default: {
      printf("ERROR: illegal test mode\n");
      ++errors;
      break;
    }
  }
  if (-1 == nbytes) {
    MyPerror("ReceiverThread, ReceiveDatagram");
    printf("ERROR: ReceiveDatagram failed, did not receive anything.\n");
    ++errors;
  } else {
    if (nbytes != tsp->msg_len) {
      MyPerror("ReceiveDatagram");
      printf("ERROR: ReceiveDatagram did not receive all bytes."
             "  Buffer %"NACL_PRIdS", expected %d, got %d bytes.\n",
             sizeof recv_buf, tsp->msg_len, nbytes);
      ++errors;
    }
    if (0 != strcmp(recv_buf, tsp->msg_buffer)) {
      printf("Received %s, sent %s\n", recv_buf, tsp->msg_buffer);
      ++errors;
    }
    if (0 != hdr.handle_count) {
      printf("ERROR: Did not receive zero handles.\n");
      ++errors;
    }
    if (NACL_ARRAY_SIZE(handle) < hdr.handle_count) {
      printf("Too many handles: %"NACL_PRIu32, hdr.handle_count);
      return ++errors;
    }
    for (size_t i = 0; i < hdr.handle_count; ++i) {
      if (hdr.handles[i] == tsp->srv_sock) {
        printf("ERROR: received handle is same as srv_sock!\n");
        ++errors;
        continue;
      }
      if (hdr.handles[i] == tsp->cli_sock) {
        printf("ERROR: received handle is same as cli_sock!\n");
        ++errors;
        continue;
      }
      if (hdr.handles[i] == tsp->pair[0]) {
        printf("ERROR: received handle is same as pair[0]!\n");
        ++errors;
        continue;
      }
      if (hdr.handles[i] == tsp->pair[1]) {
        printf("ERROR: received handle is same as pair[1]!\n");
        ++errors;
        continue;
      }
      printf("close(%d)\n", hdr.handles[i]);
      if (-1 == nacl::Close(hdr.handles[i])) {
        MyPerror("ReceiverThread, Close");
        printf("ERROR: Close on received handle failed\n");
        ++errors;
      }
    }
  }
  if (0 == errors) {
    printf("ReceiverThread, receive data: OK\n");
  } else {
    printf("ReceiverThread, receive data: FAILED\n");
  }
  return errors;
}

int SendDataNoPeer(TestState *tsp, int mode) {
  int errors(0);
  nacl::MessageHeader hdr;
  nacl::IOVec vec;
  int nbytes(-1);
  vec.base = tsp->msg_buffer;
  vec.length = tsp->msg_len;
  hdr.iov = &vec;
  hdr.iov_length = 1;
  hdr.handle_count = 0;
  hdr.flags = 0;
  nacl::string op;
  switch (mode) {
    case 0: {
      op = "SendDatagramTo";
      nbytes = nacl::SendDatagramTo(tsp->cli_sock, &hdr, 0, &tsp->srv_addr);
      break;
    }
    case 1: {
      op = "SendDatagram";
      nbytes = nacl::SendDatagram(tsp->pair[1], &hdr, 0);
      break;
    }
    default: {
      printf("ERROR: illegal test mode\n");
      ++errors;
      break;
    }
  }

#if NACL_WINDOWS
  if (-1 != nbytes
      || (0 == mode && ERROR_FILE_NOT_FOUND != GetLastError())
      || (1 == mode && ERROR_NO_DATA != GetLastError())) {
    MyPerror(op);
    printf("ERROR: no peer, nbytes %d, GetLastError => %d\n",
           nbytes,
           GetLastError());
    ++errors;
  } else {
    printf("OK\n");
  }
#else
  if (-1 != nbytes
      || (0 == mode && ECONNREFUSED != errno)
      || (1 == mode && EPIPE != errno)) {
    MyPerror(op);
    printf("ERROR: no peer, nbytes %d, errno %d\n", nbytes, errno);
    ++errors;
  } else {
    printf("OK\n");
  }
#endif
  return errors;
}

struct TestFn {
  char const *name;
  int (*sender)(TestState *, int);
  int (*receiver)(TestState *, int);
  int mode;        // currently, only 0,1 for using bound socket and socketpair
  bool new_socks;  // run test w/ per-test sockets
  bool flakey;       // known-to-be-flakey test (known IMC implementation bug)
} test_fn[] = { {
    "Send one descriptor via bound socket, shared socket",
    SendDescriptor,
    ReceiveDescriptor,
    0,
    false,
    false,
  }, {
    "Send some data via bound socket, shared socket",
    SendData,
    ReceiveData,
    0,
    false,
#if NACL_WINDOWS
    true,  // known not to be flakey
#else
    false,
#endif
  }, {
    "Send one descriptor via socket pair, shared socket",
    SendDescriptor,
    ReceiveDescriptor,
    1,
    false,
    false,
  }, {
    "Send some data via socket pair, shared socket",
    SendData,
    ReceiveData,
    1,
    false,
    false,
  }, {
    "Send one descriptor via bound socket, per test socket",
    SendDescriptor,
    ReceiveDescriptor,
    0,
    true,
    false,
  }, {
    "Send some data via bound socket, per test socket",
    SendData,
    ReceiveData,
    0,
    true,
#if NACL_WINDOWS
    true,  // known not to work: first msg must be a desc xfer
#else
    false,
#endif
  }, {
    "Send one descriptor via socket pair, per test socket",
    SendDescriptor,
    ReceiveDescriptor,
    1,
    true,
    false,
  }, {
    "Send some data via socket pair, per test socket",
    SendData,
    ReceiveData,
    1,
    true,
    false,
  },
  // add more tests here
};

void WINAPI PeerThread(void *state) {
  int errors(0);
  TestState *tsp = reinterpret_cast<TestState *>(state);

  for (int i = 0; i < tsp->repetitions; ++i) {
    if (-1 == tsp->outer_rep)
      printf("\n======== PEER THREAD, REPETITION %d ========\n", i);
    else
      printf("\n======== INDEPENDENT PEER THREAD, REPETITION %d ========\n",
             tsp->outer_rep);
    for (std::vector<int>::const_iterator it = tsp->test_sequence->begin();
         tsp->test_sequence->end() != it;
         ++it) {
      int test = *it;

      if (test_fn[test].new_socks != tsp->new_sock_only) {
        printf("PeerThread: new_socks mismatch, skipping\n");
        continue;
      }

      printf("PeerThread: Locking for test %d to start\n", test);
      NaClMutexLock(&tsp->mu);
      while (tsp->cur_test != test) {
        printf("PeerThread: waiting for test %d to start\n", test);
        printf("tsp->cur_test %d\n", tsp->cur_test);
        NaClCondVarWait(&tsp->cv, &tsp->mu);
      }
      NaClMutexUnlock(&tsp->mu);

      printf("PeerThread: START test %d, %s\n", test, test_fn[test].name);
      errors += test_fn[test].sender(tsp, test_fn[test].mode);
      printf("PeerThread: END test %d, %s\n", test, test_fn[test].name);

      printf("PeerThread: Locking for test %d to end\n", test);
      NaClMutexLock(&tsp->mu);
      tsp->cur_test = -1;
      NaClCondVarSignal(&tsp->cv);
      NaClMutexUnlock(&tsp->mu);
    }
  }
  if (-1 == tsp->outer_rep)
    printf("\n======== EXITING PEER THREAD ========\n");
  else
    printf("\n======== EXITING INDEPENDENT PEER THREAD"
           " ========\n");

  printf("%sPEER THREAD EXITING, LOCKING\n",
         (-1 == tsp->outer_rep) ? "" : "INDEPENDENT ");
  (void) NaClMutexLock(&tsp->mu);
  tsp->errors = errors;
  printf("%sPEER THREAD EXITING, SIGNALING\n",
         (-1 == tsp->outer_rep) ? "" : "INDEPENDENT ");
  fflush(NULL);
  (void) NaClCondVarSignal(&tsp->cv);
  (void) NaClMutexUnlock(&tsp->mu);
}


int TestNaClSocket(int rep_count) {
  int errors = 0;
  TestState tstate(&gTestSequence, false, rep_count, -1);

  errors += tstate.Init();
  if (0 != errors) return errors;

  // The Windows IMC implementation deadlocks if SendDatagramTo is
  // invoked while nobody is doing a ReceiveDatagram.  Thus, we spawn
  // a receiver thread.

  printf("Starting receiver thread.\n");
  struct NaClThread thr;
  (void) NaClThreadCtor(&thr, PeerThread, static_cast<void *>(&tstate),
                        128*1024);

  for (int rep = 0; rep < rep_count; ++rep) {
    printf("\n======== MAIN THREAD, START REPETITION %d ========\n", rep);
    for (std::vector<int>::const_iterator it = gTestSequence.begin();
         gTestSequence.end() != it;
         ++it) {
      int test = *it;
      printf("MainThread: test %d, %s\n", test, test_fn[test].name);
      fflush(NULL);

      if (gSleepBeforeReceive) {
        printf("Sleeping.\n");
        fflush(NULL);
#if NACL_WINDOWS
        Sleep(1000);
#else
        sleep(1);
#endif
      }

      if (!test_fn[test].new_socks) {
        printf("Locking to start test %d\n", test);
        NaClMutexLock(&tstate.mu);
        tstate.cur_test = test;
        NaClCondVarSignal(&tstate.cv);
        NaClMutexUnlock(&tstate.mu);
        printf("Signaled test %d start\n", test);

        errors += test_fn[test].receiver(&tstate, test_fn[test].mode);

        printf("Locking to wait for test %d end\n", test);
        NaClMutexLock(&tstate.mu);
        while (-1 != tstate.cur_test) {
          printf("Waiting for test %d to be finished\n", test);
          printf("tstate.cur_test %d\n", tstate.cur_test);
          NaClCondVarWait(&tstate.cv, &tstate.mu);
        }
        NaClMutexUnlock(&tstate.mu);
      } else {
        printf("test %d requests independent socket/thread\n", test);
        std::vector<int> seq;
        seq.push_back(test);
        TestState private_sock(&seq, true, 1, rep);
        int private_errors(private_sock.Init());
        if (0 != private_errors) {
          printf("Could not create/initialize TestState.\n");
          errors += private_errors;
          continue;
        }
        struct NaClThread private_thr;
        (void) NaClThreadCtor(&private_thr, PeerThread,
                              static_cast<void *>(&private_sock), 128*1024);

        printf("Locking to start test %d\n", test);
        NaClMutexLock(&private_sock.mu);
        private_sock.cur_test = test;
        NaClCondVarSignal(&private_sock.cv);
        NaClMutexUnlock(&private_sock.mu);
        printf("Signaled test %d start\n", test);

        errors += test_fn[test].receiver(&private_sock, test_fn[test].mode);

        printf("Locking to wait for test %d end\n", test);
        NaClMutexLock(&private_sock.mu);
        while (-1 != private_sock.cur_test) {
          printf("Waiting for test %d to be finished\n", test);
          printf("private_sock %d\n", private_sock.cur_test);
          NaClCondVarWait(&private_sock.cv, &private_sock.mu);
        }
        NaClMutexUnlock(&private_sock.mu);

        fflush(NULL);
        (void) NaClMutexLock(&private_sock.mu);
        while (-1 == private_sock.errors) {
          (void) NaClCondVarWait(&private_sock.cv, &private_sock.mu);
        }
        (void) NaClMutexUnlock(&private_sock.mu);
        errors += private_sock.errors;
        if (private_sock.outer_rep != rep) {
          printf("Threads out of sync!?!\n");
          abort();
        }
      }
    }
  }


  printf("MainThread: Waiting for receiver thread to exit.\n");
  fflush(NULL);
  (void) NaClMutexLock(&tstate.mu);
  while (-1 == tstate.errors) {
    (void) NaClCondVarWait(&tstate.cv, &tstate.mu);
  }
  (void) NaClMutexUnlock(&tstate.mu);
  NaClThreadDtor(&thr);
  errors += tstate.errors;

  // now close server side and attempt to send again.
  printf("nacl::Close(%d)\n", tstate.srv_sock);
  (void) nacl::Close(tstate.srv_sock);
  tstate.srv_sock = nacl::kInvalidHandle;
  printf("nacl::Close(%d)\n", tstate.pair[0]);
  (void) nacl::Close(tstate.pair[0]);
  tstate.pair[0] = nacl::kInvalidHandle;

  printf("Sending a datagram to an address with closed bound socket.\n");
  SendDataNoPeer(&tstate, 0);
  printf("Sending a datagram to a socketpair where peer was closed.\n");
  SendDataNoPeer(&tstate, 1);

  return errors;
}

void ListTests() {
  for (size_t ix = 0; ix < NACL_ARRAY_SIZE(test_fn); ++ix) {
    printf("%3"NACL_PRIdS": %s\n", ix, test_fn[ix].name);
    if (test_fn[ix].flakey) {
      printf(" NB: known to be flakey on this platform\n");
    }
  }
}

}  // anonymous namespace


int main(int ac,
         char **av) {
  int errors(0);
  int opt;
  int rep_count(100);

  char obuf[BUFSIZ];

  setvbuf(stdout, obuf, _IOLBF, sizeof obuf);

  NaClLogModuleInit();

  while (-1 != (opt = getopt(ac, av, "lr:st:"))) {
    switch (opt) {
      case 'l': {
        ListTests();
        return 0;
      }
      case 'r': {
        rep_count = strtol(optarg, static_cast<char **>(NULL), 0);
        break;
      }
      case 's': {
        gSleepBeforeReceive = true;
        break;
      }
      case 't': {
        std::vector<nacl::string> vs;
        SplitString(&vs, optarg, ',');
        ApplyInt(&gTestSequence, vs);
        break;
      }
      default: {
        fprintf(stderr,
                "Usage: sigpipe_test [-l] [-r reps] [-s] [-t tests]\n"
                " -l list available tests\n"
                " -r run test sequence reps number of times\n"
                " -s sleep between receiving handle and data\n"
                " -t test sequence, a comma separated integers specifying\n"
                "    which tests to run and in what order\n");
        exit(1);
      }
    }
  }
  if (gTestSequence.empty()) {
    for (size_t i = 0; i < NACL_ARRAY_SIZE(test_fn); ++i) {
      if (!test_fn[i].flakey) {
        gTestSequence.push_back(nacl::assert_cast<int>(i));
      }
    }
  }
  printf("Test sequence:\n");
  for (std::vector<int>::const_iterator it = gTestSequence.begin();
       gTestSequence.end() != it;
       ++it) {
    if ((unsigned) *it >= NACL_ARRAY_SIZE(test_fn)) {
      fprintf(stderr, "No test %d\n", *it);
      ++errors;
    } else {
      printf(" %d: %s\n", *it, test_fn[*it].name);
    }
  }

  if (errors) {
    printf("Test specification error.\n");
    return errors;
  }
#if NACL_WINDOWS
  if (0 != gTestSequence[0]) {
    printf("WARNING: it is known that the Windows IMC implementation\n"
           "fails if the first operation is not send descriptor\n");
    fflush(NULL);
  }
#endif

  errors += TestNaClSocket(rep_count);

  printf("%s\n", (errors == 0) ? "PASSED" : "FAILED");

  NaClLogModuleFini();

  return errors;
}
