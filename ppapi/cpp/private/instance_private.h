// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_PRIVATE_INSTANCE_PRIVATE_H_
#define PPAPI_CPP_PRIVATE_INSTANCE_PRIVATE_H_

/**
 * @file
 * Defines the API ...
 *
 * @addtogroup CPP
 * @{
 */

#include "ppapi/c/dev/ppb_console_dev.h"
#include "ppapi/cpp/instance.h"

/** The C++ interface to the Pepper API. */
namespace pp {

class Var;
class VarPrivate;

class InstancePrivate : public Instance {
 public:
  explicit InstancePrivate(PP_Instance instance);
  virtual ~InstancePrivate();

  // @{
  /// @name PPP_Instance_Private methods for the plugin to override:

  /// See PPP_Instance_Private.GetInstanceObject.
  virtual Var GetInstanceObject();

  // @}

  // @{
  /// @name PPB_Instance_Private methods for querying the browser:

  /// See PPB_Instance_Private.GetWindowObject.
  VarPrivate GetWindowObject();

  /// See PPB_Instance_Private.GetOwnerElementObject.
  VarPrivate GetOwnerElementObject();

  /// See PPB_Instance.ExecuteScript.
  VarPrivate ExecuteScript(const Var& script, Var* exception = NULL);

  // @}

  // @{
  /// @name PPB_Console_Dev methods for logging to the console:

  /// See PPB_Console_Dev.Log.
  void LogToConsole(PP_LogLevel_Dev level, const Var& value);

  /// See PPB_Console_Dev.LogWithSource.
  void LogToConsoleWithSource(PP_LogLevel_Dev level,
                              const Var& source,
                              const Var& value);

  // @}
};

}  // namespace pp

/**
 * @}
 * End addtogroup CPP
 */
#endif  // PPAPI_CPP_PRIVATE_INSTANCE_PRIVATE_H_
