// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/quarantine/quarantine.h"

#include "build/build_config.h"

#if !defined(OS_WIN) && !defined(OS_APPLE) && !defined(OS_CHROMEOS)

namespace quarantine {

void QuarantineFile(const base::FilePath& file,
                    const GURL& source_url,
                    const GURL& referrer_url,
                    const std::string& client_guid,
                    mojom::Quarantine::QuarantineFileCallback callback) {
  std::move(callback).Run(QuarantineFileResult::OK);
}

}  // namespace quarantine

#endif  // !WIN && !MAC && !CHROMEOS
