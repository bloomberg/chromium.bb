// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_CDM_REGISTRY_H_
#define CONTENT_PUBLIC_BROWSER_CDM_REGISTRY_H_

#include <vector>

#include "content/common/content_export.h"

namespace content {

struct CdmInfo;

// Keeps track of the Content Decryption Modules that are available.
class CONTENT_EXPORT CdmRegistry {
 public:
  // Returns the CdmRegistry singleton.
  static CdmRegistry* GetInstance();

  virtual ~CdmRegistry() {}

  // Must be called on the instance to finish initialization.
  virtual void Init() = 0;

  // Registers a CDM with the specified CDM information. The CDM will be
  // inserted at the head of the list so that it can override any older
  // registrations.
  // Note: Since only 1 version of the CDM can be loaded at any given time,
  // it is possible that there will be a mismatch between the functionality
  // reported and what is actually available, if the reported functionality
  // changes between versions. (http://crbug.com/599588)
  virtual void RegisterCdm(const CdmInfo& info) = 0;

  // Returns the list of all registered CDMs and the associated data.
  virtual const std::vector<CdmInfo>& GetAllRegisteredCdms() = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_CDM_REGISTRY_H_
