/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#ifndef _TESTS_POSIX_OVER_SRPC_LAUNCHER_H_
#define _TESTS_POSIX_OVER_SRPC_LAUNCHER_H_

#include <list>
#include <map>
#include <vector>
#include <queue>

#include "native_client/src/trusted/desc/nacl_desc_wrapper.h"
#include "native_client/src/trusted/nonnacl_util/sel_ldr_launcher.h"

enum CollectTag {
  IO_DESC,
  DESC_WRAPPER
};

typedef union HandledValue {
  void* ptrval;
  int ival;
  HandledValue() {}
  HandledValue(int a) : ival(a) {}
  HandledValue(void* a) : ptrval(a) {}
} HandledValue;

typedef std::map<int, HandledValue> ValueByHandleMap;
typedef std::pair<void*, enum CollectTag> CollectPair;
typedef std::queue<CollectPair> DescCollector;

class PosixOverSrpcLauncher {
 public:
  PosixOverSrpcLauncher();
  virtual ~PosixOverSrpcLauncher();
  // Executes nacl module. This method is not blocking. When it returns nacl
  // module can still do something.
  bool SpawnNaClModule(const nacl::string& nacl_module,
                       const std::vector<nacl::string>& app_argv);
  void IncreaseNumberOfChildren();
  void DecreaseNumberOfChildren();

  bool ValueByHandle(int child_id, int handle, HandledValue* pval);
  int NewHandle(int child_id, HandledValue val);

  void CollectDesc(int child_id, void* desc, enum CollectTag tag);
  void CloseUnusedDescs(int child_id);
  nacl::DescWrapperFactory* DescFactory();

 private:
  nacl::string sel_ldr_dir_;
  std::vector<nacl::string> sel_ldr_args_;

  // For every spawned nacl module we store instance of SelLdrLauncher and table
  // to map pointers and socket file descriptors. All the methods which work
  // with opaque entities like struct DIR use this table. In nacl module pointer
  // is just an int value which is mapped to real address of opaque structure in
  // this launcher.
  std::vector<nacl::SelLdrLauncher*> launchers_;
  std::vector<ValueByHandleMap*> value_by_handle_maps_;
  std::vector<int> last_used_handles_;

  // Also for every nacl module there is a collector for unused NaClDescs which
  // have to be closed.
  std::vector<DescCollector*> desc_collectors_;

  pthread_mutex_t data_mu_;
  pthread_cond_t all_children_died_cv_;
  int nchildren_;
  nacl::DescWrapperFactory factory_;
};

#endif  // _TESTS_POSIX_OVER_SRPC_LAUNCHER_H_
