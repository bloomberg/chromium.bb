// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/patch/in_process_patcher.h"

#include <utility>

#include "base/callback.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "courgette/courgette.h"
#include "courgette/third_party/bsdiff/bsdiff.h"

namespace update_client {

namespace {

class InProcessPatcher : public Patcher {
 public:
  InProcessPatcher() = default;

  void PatchBsdiff(const base::FilePath& input_path,
                   const base::FilePath& patch_path,
                   const base::FilePath& output_path,
                   PatchCompleteCallback callback) const override {
    base::File input_file(input_path,
                          base::File::FLAG_OPEN | base::File::FLAG_READ);
    base::File patch_file(patch_path,
                          base::File::FLAG_OPEN | base::File::FLAG_READ);
    base::File output_file(output_path,
                           base::File::FLAG_CREATE | base::File::FLAG_WRITE |
                               base::File::FLAG_WIN_EXCLUSIVE_WRITE);
    if (!input_file.IsValid() || !patch_file.IsValid() ||
        !output_file.IsValid()) {
      std::move(callback).Run(-1);
      return;
    }
    std::move(callback).Run(bsdiff::ApplyBinaryPatch(
        std::move(input_file), std::move(patch_file), std::move(output_file)));
  }

  void PatchCourgette(const base::FilePath& input_path,
                      const base::FilePath& patch_path,
                      const base::FilePath& output_path,
                      PatchCompleteCallback callback) const override {
    base::File input_file(input_path,
                          base::File::FLAG_OPEN | base::File::FLAG_READ);
    base::File patch_file(patch_path,
                          base::File::FLAG_OPEN | base::File::FLAG_READ);
    base::File output_file(output_path,
                           base::File::FLAG_CREATE | base::File::FLAG_WRITE |
                               base::File::FLAG_WIN_EXCLUSIVE_WRITE);
    if (!input_file.IsValid() || !patch_file.IsValid() ||
        !output_file.IsValid()) {
      std::move(callback).Run(-1);
      return;
    }
    std::move(callback).Run(courgette::ApplyEnsemblePatch(
        std::move(input_file), std::move(patch_file), std::move(output_file)));
  }

 protected:
  ~InProcessPatcher() override = default;
};

}  // namespace

InProcessPatcherFactory::InProcessPatcherFactory() = default;

scoped_refptr<Patcher> InProcessPatcherFactory::Create() const {
  return base::MakeRefCounted<InProcessPatcher>();
}

InProcessPatcherFactory::~InProcessPatcherFactory() = default;

}  // namespace update_client
