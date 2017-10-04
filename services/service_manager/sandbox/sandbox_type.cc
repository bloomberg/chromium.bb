// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/sandbox/sandbox_type.h"

#include <string>

#include "services/service_manager/sandbox/switches.h"

namespace service_manager {

void SetCommandLineFlagsForSandboxType(base::CommandLine* command_line,
                                       SandboxType sandbox_type) {
  switch (sandbox_type) {
    case SANDBOX_TYPE_NO_SANDBOX:
      command_line->AppendSwitch(switches::kNoSandbox);
      break;
    case SANDBOX_TYPE_RENDERER:
      DCHECK(command_line->GetSwitchValueASCII(switches::kProcessType) ==
             switches::kRendererProcess);
      break;
    case SANDBOX_TYPE_UTILITY:
      DCHECK(command_line->GetSwitchValueASCII(switches::kProcessType) ==
             switches::kUtilityProcess);
      DCHECK(!command_line->HasSwitch(switches::kServiceSandboxType));
      command_line->AppendSwitchASCII(switches::kServiceSandboxType,
                                      switches::kUtilitySandbox);
      break;
    case SANDBOX_TYPE_GPU:
      DCHECK(command_line->GetSwitchValueASCII(switches::kProcessType) ==
             switches::kGpuProcess);
      break;
    case SANDBOX_TYPE_PPAPI:
      if (command_line->GetSwitchValueASCII(switches::kProcessType) ==
          switches::kUtilityProcess) {
        command_line->AppendSwitchASCII(switches::kServiceSandboxType,
                                        switches::kPpapiSandbox);
      } else {
        DCHECK(command_line->GetSwitchValueASCII(switches::kProcessType) ==
               switches::kPpapiPluginProcess);
      }
      break;
    case SANDBOX_TYPE_NETWORK:
      DCHECK(command_line->GetSwitchValueASCII(switches::kProcessType) ==
             switches::kUtilityProcess);
      DCHECK(!command_line->HasSwitch(switches::kServiceSandboxType));
      command_line->AppendSwitchASCII(switches::kServiceSandboxType,
                                      switches::kNetworkSandbox);
      break;
    case SANDBOX_TYPE_CDM:
      DCHECK(command_line->GetSwitchValueASCII(switches::kProcessType) ==
             switches::kUtilityProcess);
      DCHECK(!command_line->HasSwitch(switches::kServiceSandboxType));
      command_line->AppendSwitchASCII(switches::kServiceSandboxType,
                                      switches::kCdmSandbox);
      break;
    default:
      break;
  }
}

SandboxType SandboxTypeFromCommandLine(const base::CommandLine& command_line) {
  if (command_line.HasSwitch(switches::kNoSandbox))
    return SANDBOX_TYPE_NO_SANDBOX;

  std::string process_type =
      command_line.GetSwitchValueASCII(switches::kProcessType);
  if (process_type.empty())
    return SANDBOX_TYPE_NO_SANDBOX;

  if (process_type == switches::kRendererProcess)
    return SANDBOX_TYPE_RENDERER;

  if (process_type == switches::kUtilityProcess) {
    return UtilitySandboxTypeFromString(
        command_line.GetSwitchValueASCII(switches::kServiceSandboxType));
  }
  if (process_type == switches::kGpuProcess) {
    if (command_line.HasSwitch(switches::kDisableGpuSandbox))
      return SANDBOX_TYPE_NO_SANDBOX;
    return SANDBOX_TYPE_GPU;
  }
  if (process_type == switches::kPpapiBrokerProcess)
    return SANDBOX_TYPE_NO_SANDBOX;

  if (process_type == switches::kPpapiPluginProcess)
    return SANDBOX_TYPE_PPAPI;

  // This is a process which we don't know about.
  return SANDBOX_TYPE_INVALID;
}

SandboxType UtilitySandboxTypeFromString(const std::string& sandbox_string) {
  if (sandbox_string == switches::kNoneSandbox)
    return SANDBOX_TYPE_NO_SANDBOX;
  if (sandbox_string == switches::kNetworkSandbox)
    return SANDBOX_TYPE_NETWORK;
  if (sandbox_string == switches::kPpapiSandbox)
    return SANDBOX_TYPE_PPAPI;
  if (sandbox_string == switches::kCdmSandbox)
    return SANDBOX_TYPE_CDM;
  return SANDBOX_TYPE_UTILITY;
}

}  // namespace service_manager
