// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_SERVICE_WORKER_WORKER_ID_SET_H_
#define EXTENSIONS_BROWSER_SERVICE_WORKER_WORKER_ID_SET_H_

#include <set>
#include <vector>

#include "base/macros.h"
#include "extensions/browser/service_worker/worker_id.h"
#include "extensions/common/extension_id.h"

// Set of WorkersIds that provides faster retrieval/removal of workers by
// extension id, render process id etc.
namespace extensions {

class WorkerIdSet {
 public:
  WorkerIdSet();
  ~WorkerIdSet();

  void Add(const WorkerId& worker_id);
  bool Remove(const WorkerId& worker_id);
  void RemoveAllForExtension(const ExtensionId& extension_id);
  bool Contains(const WorkerId& worker_id) const;
  std::vector<WorkerId> GetAllForExtension(const ExtensionId& extension_id,
                                           int render_process_id) const;

  std::vector<WorkerId> GetAllForTesting() const;
  size_t count_for_testing() const { return workers_.size(); }

 private:
  std::set<WorkerId> workers_;

  DISALLOW_COPY_AND_ASSIGN(WorkerIdSet);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_SERVICE_WORKER_WORKER_ID_SET_H_
