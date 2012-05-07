// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_FILE_SYSTEM_URL_REQUEST_JOB_FACTORY_H_
#define WEBKIT_FILEAPI_FILE_SYSTEM_URL_REQUEST_JOB_FACTORY_H_

#include "net/url_request/url_request_job_factory.h"

namespace base {
class MessageLoopProxy;
}  // namespace base

namespace fileapi {

class FileSystemContext;

// |context|'s lifetime should exceed the lifetime of the ProtocolHandler.
// Currently, this is only used by ProfileIOData which owns |context| and the
// ProtocolHandler.
net::URLRequestJobFactory::ProtocolHandler*
CreateFileSystemProtocolHandler(FileSystemContext* context);

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_FILE_SYSTEM_URL_REQUEST_JOB_FACTORY_H_
