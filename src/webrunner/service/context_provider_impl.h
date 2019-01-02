// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBRUNNER_SERVICE_CONTEXT_PROVIDER_IMPL_H_
#define WEBRUNNER_SERVICE_CONTEXT_PROVIDER_IMPL_H_

#include <lib/fidl/cpp/binding_set.h>
#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "chromium/web/cpp/fidl.h"
#include "webrunner/common/webrunner_export.h"

namespace base {
class CommandLine;
struct LaunchOptions;
class Process;
}  // namespace base

namespace webrunner {

class WEBRUNNER_EXPORT ContextProviderImpl
    : public chromium::web::ContextProvider {
 public:
  ContextProviderImpl();
  ~ContextProviderImpl() override;

  // Creates a ContextProviderImpl that shares its /tmp directory with its child
  // processes. This is useful for GTest processes, which depend on a shared
  // tmpdir for storing startup flags and retrieving test result files.
  static std::unique_ptr<ContextProviderImpl> CreateForTest();

  // Binds |this| object instance to |request|.
  // The service will persist and continue to serve other channels in the event
  // that a bound channel is dropped.
  void Bind(fidl::InterfaceRequest<chromium::web::ContextProvider> request);

  // chromium::web::ContextProvider implementation.
  void Create(chromium::web::CreateContextParams params,
              ::fidl::InterfaceRequest<chromium::web::Context> context_request)
      override;

 private:
  using LaunchContextProcessCallback = base::RepeatingCallback<base::Process(
      const base::CommandLine& command,
      const base::LaunchOptions& options)>;

  friend class ContextProviderImplTest;

  explicit ContextProviderImpl(bool use_shared_tmp);

  // Overrides the default child process launching logic to call |launch|
  // instead.
  void SetLaunchCallbackForTests(const LaunchContextProcessCallback& launch);

  // Spawns a Context child process.
  LaunchContextProcessCallback launch_;

  // If set, then the ContextProvider will share /tmp with its child processes.
  bool use_shared_tmp_ = true;

  fidl::BindingSet<chromium::web::ContextProvider> bindings_;

  DISALLOW_COPY_AND_ASSIGN(ContextProviderImpl);
};

}  // namespace webrunner

#endif  // WEBRUNNER_SERVICE_CONTEXT_PROVIDER_IMPL_H_
