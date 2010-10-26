/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include <string.h>
#include <stdlib.h>

#include "native_client/src/include/nacl_base.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/service_runtime/nacl_signal.h"
#include "native_client/src/trusted/service_runtime/sel_ldr.h"

#ifdef WIN32
#include <io.h>
#define write _write
#else
#include <unistd.h>
#endif

#define MAX_NACL_HANDLERS 16

struct NaClApp *g_SignalNAP;

struct NaClSignalNode {
  struct NaClSignalNode *next;
  NaClSignalHandler func;
  int id;
};


static struct NaClSignalNode *s_FirstHandler = NULL;
static struct NaClSignalNode *s_FreeList = NULL;
static struct NaClSignalNode s_SignalNodes[MAX_NACL_HANDLERS];

ssize_t NaClSignalErrorMessage(const char *msg) {
  /*
   * We cannot use NaClLog() in the context of a signal handler: it is
   * too complex.  However, write() is signal-safe.
   */
  size_t len_t = strlen(msg);
  int len = (int) len_t;

  /*
   * Write uses int not size_t, so we may wrap the length and/or
   * generate a negative value.  Only print if it matches.
   */
  if ((len > 0) && (len_t == (size_t) len)) {
    return (ssize_t) write(2, msg, len);
  }

  return 0;
}

enum NaClSignalResult NaClSignalHandleNone(int in_untrusted_code,
                                           int signal,
                                           void *ctx) {
  UNREFERENCED_PARAMETER(in_untrusted_code);
  UNREFERENCED_PARAMETER(signal);
  UNREFERENCED_PARAMETER(ctx);

  /* Don't do anything, just pass it to the OS. */
  return NACL_SIGNAL_SKIP;
}

enum NaClSignalResult NaClSignalHandleAll(int in_untrusted_code,
                                          int signal,
                                          void *ctx) {
  UNREFERENCED_PARAMETER(ctx);

  /*
   * Return an 8 bit error code which is -signal to
   * simulate normal OS behavior
   */
  if (in_untrusted_code) {
    NaClSignalErrorMessage("** Fault in NaCl untrusted code\n");
    _exit((-signal) & 0xFF);
  }
  else {
    NaClSignalErrorMessage("** Fault in NaCl trusted code\n");
    _exit((-signal) & 0xFF);
  }
  return NACL_SIGNAL_RETURN;
}

enum NaClSignalResult NaClSignalHandleUntrusted(int in_untrusted_code,
                                                int signal,
                                                void *ctx) {
  UNREFERENCED_PARAMETER(ctx);

  /*
   * Return an 8 bit error code which is -signal to
   * simulate normal OS behavior
   */
  if (in_untrusted_code) {
    NaClSignalErrorMessage("** Fault in NaCl untrusted code\n");
    _exit((-signal) & 0xFF);
  }
  else {
    /* Print a message and try the next handler. */
    NaClSignalErrorMessage("** Fault in NaCl trusted code\n");
  }
  return NACL_SIGNAL_SEARCH;
}


int NaClSignalHandlerAdd(NaClSignalHandler func) {
  int id = 0;

  CHECK(func != NULL);

  /* If we have room... */
  if (s_FreeList) {
    /* Update the free list. */
    struct NaClSignalNode *add = s_FreeList;
    s_FreeList = add->next;

    /* Construct the node. */
    add->func = func;
    add->next = s_FirstHandler;

    /* Add node to the head. */
    s_FirstHandler = add;
    id = add->id;
  }

  return id;
}


int NaClSignalHandlerRemove(int id) {
  /* The first node pointer is the first "next" pointer. */
  struct NaClSignalNode **ppNode = &s_FirstHandler;

  /* While the "next" pointer is valid, process what it points to. */
  while (*ppNode) {
    /* If the next item has a matching ID */
    if ((*ppNode)->id == id) {
      /* then we will free that item. */
      struct NaClSignalNode *freeNode = *ppNode;

      /* First, skip past it. */
      *ppNode = (*ppNode)->next;

      /* Then add this node to the head of the free list. */
      freeNode->next = s_FreeList;
      s_FreeList = freeNode;
      return 1;
    }
    ppNode = &(*ppNode)->next;
  }

  return 0;
}

enum NaClSignalResult NaClSignalHandlerFind(int in_untrusted_code,
                                            int signal,
                                            void *ctx) {
  enum NaClSignalResult result = NACL_SIGNAL_SEARCH;
  struct NaClSignalNode *pNode;

  /* Iterate through handlers */
  pNode = s_FirstHandler;
  while (pNode) {
    result = pNode->func(in_untrusted_code, signal, ctx);

    /* If we are not asking for the search to continue... */
    if (NACL_SIGNAL_SEARCH != result) break;

    pNode = pNode->next;
  }

  return result;
}

void NaClSignalHandlerInit() {
  int a;

  /* Build the free list */
  for (a = 0; a < MAX_NACL_HANDLERS; a++) {
    s_SignalNodes[a].next = s_FreeList;
    s_SignalNodes[a].id = a + 1;
    s_FreeList = &s_SignalNodes[a];
  }

  NaClSignalHandlerInitPlatform();
  NaClSignalHandlerAdd(NaClSignalHandleAll);
  if (getenv("NACL_CRASH_TEST") != NULL) {
    NaClSignalErrorMessage("[CRASH_TEST] Causing crash in NaCl "
                           "trusted code...\n");
    *(int *) 0 = 0;
  }
}

void NaClSignalHandlerFini() {
  /* We try to lock, but since we are shutting down, we ignore failures. */
  NaClSignalHandlerFiniPlatform();
}

void NaClSignalRegisterApp(struct NaClApp *nap) {
  CHECK(g_SignalNAP == NULL);
  g_SignalNAP = nap;
}


