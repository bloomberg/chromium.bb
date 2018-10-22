// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "content/browser/code_cache/generated_code_cache_context.h"
#include "base/files/file_path.h"
#include "content/browser/code_cache/generated_code_cache.h"
#include "content/public/browser/browser_thread.h"

namespace content {

GeneratedCodeCacheContext::GeneratedCodeCacheContext() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

void GeneratedCodeCacheContext::Initialize(const base::FilePath& path,
                                           int max_bytes) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(&GeneratedCodeCacheContext::InitializeOnIO, this, path,
                     max_bytes));
}

void GeneratedCodeCacheContext::InitializeOnIO(const base::FilePath& path,
                                               int max_bytes) {
  generated_code_cache_.reset(new GeneratedCodeCache(path, max_bytes));
}

GeneratedCodeCache* GeneratedCodeCacheContext::generated_code_cache() const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return generated_code_cache_.get();
}

GeneratedCodeCacheContext::~GeneratedCodeCacheContext() = default;

}  // namespace content
