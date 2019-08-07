// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_FILEAPI_FILE_SYSTEM_URL_REQUEST_JOB_FACTORY_H_
#define STORAGE_BROWSER_FILEAPI_FILE_SYSTEM_URL_REQUEST_JOB_FACTORY_H_

#include <memory>
#include <string>

#include "net/url_request/url_request_job_factory.h"

#include "base/component_export.h"

namespace storage {

class FileSystemContext;

// |context|'s lifetime should exceed the lifetime of the ProtocolHandler.
// Currently, this is only used by ProfileIOData which owns |context| and the
// ProtocolHandler.
COMPONENT_EXPORT(STORAGE_BROWSER)
std::unique_ptr<net::URLRequestJobFactory::ProtocolHandler>
CreateFileSystemProtocolHandler(const std::string& storage_domain,
                                FileSystemContext* context);

}  // namespace storage

#endif  // STORAGE_BROWSER_FILEAPI_FILE_SYSTEM_URL_REQUEST_JOB_FACTORY_H_
