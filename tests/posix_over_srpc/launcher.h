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

typedef std::map<int, void*> PtrByHandleMap;
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

  bool PtrByHandle(int child_id, int handle, void** pptr);
  int NewPtrHandle(int child_id, void* ptr);

  void CollectDesc(int child_id, void* desc, enum CollectTag tag);
  nacl::DescWrapperFactory* DescFactory();
  void CloseUnusedDescs(int child_id);

 private:
  nacl::string sel_ldr_dir_;
  std::vector<nacl::string> sel_ldr_args_;

  // For every spawned nacl module we store instance of SelLdrLauncher and table
  // to map pointers. All the methods which work with opaque entities like
  // struct DIR use this pointer table. In nacl module pointer is just an int
  // value which is mapped to real address of opaque structure in this launcher.
  std::vector<nacl::SelLdrLauncher*> launchers_;
  std::vector<PtrByHandleMap*> ptr_by_handle_maps_;
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
