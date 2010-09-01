/*
 * Copyright 2010 The Native Client Authors.  All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include <stdio.h>
#include <stdlib.h>

#include <map>

#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/gdb_rsp/abi.h"
#include "native_client/src/trusted/port/platform.h"

namespace port {

typedef std::map<uint64_t, uint8_t> BreakMap_t;
static BreakMap_t s_BreakMap;

bool IPlatform::AddBreakPoint(uint64_t address) {
  BreakMap_t::iterator itr = s_BreakMap.find(address);
  uint8_t BRK = 0xCC;
  uint8_t INS;

  if (itr != s_BreakMap.end()) return false;

  if (IPlatform::GetMemory(address, 1, &INS) == false) return false;
  if (IPlatform::SetMemory(address, 1, &BRK) == false) return false;

  s_BreakMap[address] = INS;
  return true;
}

bool IPlatform::DelBreakPoint(uint64_t address) {
  BreakMap_t::iterator itr = s_BreakMap.find(address);
  uint8_t INS;

  if (itr == s_BreakMap.end()) return false;

  INS = s_BreakMap[address];
  s_BreakMap.erase(itr);

  if (SetMemory(address, 1, &INS) == false) return false;
  return true;
}

//  Log a message
void IPlatform::LogInfo(const char *fmt, ...) {
  va_list argptr;
  va_start(argptr, fmt);

  NaClLogV(LOG_INFO, fmt, argptr);
}

void IPlatform::LogWarning(const char *fmt, ...) {
  va_list argptr;
  va_start(argptr, fmt);

  NaClLogV(LOG_WARNING, fmt, argptr);
}

void IPlatform::LogError(const char *fmt, ...) {
  va_list argptr;
  va_start(argptr, fmt);

  NaClLogV(LOG_FATAL, fmt, argptr);
}

}  // End of port namespace
