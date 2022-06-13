// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_APP_CONTENT_MAIN_DELEGATE_H_
#define CONTENT_PUBLIC_APP_CONTENT_MAIN_DELEGATE_H_

#include <memory>
#include <string>
#include <vector>

#include "build/build_config.h"
#include "content/common/content_export.h"
#include "content/public/common/main_function_params.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace variations {
class VariationsIdsProvider;
}

namespace content {

class ContentBrowserClient;
class ContentClient;
class ContentGpuClient;
class ContentRendererClient;
class ContentUtilityClient;
class ZygoteForkDelegate;

class CONTENT_EXPORT ContentMainDelegate {
 public:
  virtual ~ContentMainDelegate() {}

  // Tells the embedder that the absolute basic startup has been done, i.e.
  // it's now safe to create singletons and check the command line. Return true
  // if the process should exit afterwards, and if so, |exit_code| should be
  // set. This is the place for embedder to do the things that must happen at
  // the start. Most of its startup code should be in the methods below.
  virtual bool BasicStartupComplete(int* exit_code);

  // This is where the embedder puts all of its startup code that needs to run
  // before the sandbox is engaged.
  virtual void PreSandboxStartup() {}

  // This is where the embedder can add startup code to run after the sandbox
  // has been initialized.
  virtual void SandboxInitialized(const std::string& process_type) {}

  // Asks the embedder to start a process. The embedder may return the
  // |main_function_params| back to decline the request and kick-off the
  // default behavior or return a non-negative exit code to indicate it handled
  // the request.
  virtual absl::variant<int, MainFunctionParams> RunProcess(
      const std::string& process_type,
      MainFunctionParams main_function_params);

  // Called right before the process exits.
  virtual void ProcessExiting(const std::string& process_type) {}

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
  // Tells the embedder that the zygote process is starting, and allows it to
  // specify one or more zygote delegates if it wishes by storing them in
  // |*delegates|.
  virtual void ZygoteStarting(
      std::vector<std::unique_ptr<ZygoteForkDelegate>>* delegates);

  // Called every time the zygote process forks.
  virtual void ZygoteForked() {}
#endif  // defined(OS_LINUX) || defined(OS_CHROMEOS)

  // Fatal errors during initialization are reported by this function, so that
  // the embedder can implement graceful exit by displaying some message and
  // returning initialization error code. Default behavior is CHECK(false).
  virtual int TerminateForFatalInitializationError();

  // Allows the embedder to prevent locking the scheme registry. The scheme
  // registry is the list of URL schemes we recognize, with some additional
  // information about each scheme such as whether it expects a host. The
  // scheme registry is not thread-safe, so by default it is locked before any
  // threads are created to ensure single-threaded access. An embedder can
  // override this to prevent the scheme registry from being locked during
  // startup, but if they do so then they are responsible for making sure that
  // the registry is only accessed in a thread-safe way, and for calling
  // url::LockSchemeRegistries() when initialization is complete. If possible,
  // prefer registering additional schemes through
  // ContentClient::AddAdditionalSchemes over preventing the scheme registry
  // from being locked.
  virtual bool ShouldLockSchemeRegistry();

  // Allows the embedder to perform platform-specific initialization before
  // BrowserMain() is invoked (i.e. before BrowserMainRunner, BrowserMainLoop,
  // BrowserMainParts, etc. are created).
  virtual void PreBrowserMain() {}

  // Returns true if content should create field trials and initialize the
  // FeatureList instance for this process. Default implementation returns true.
  // Embedders that need to control when and/or how FeatureList should be
  // created should override and return false.
  virtual bool ShouldCreateFeatureList();

  // Creates and returns the VariationsIdsProvider. If null is returned,
  // a VariationsIdsProvider is created with a mode of `kUseSignedInState`.
  // VariationsIdsProvider is a singleton.
  virtual variations::VariationsIdsProvider* CreateVariationsIdsProvider();

  // Allows the embedder to perform initialization once field trials/FeatureList
  // initialization has completed if ShouldCreateFeatureList() returns true.
  // Otherwise, the embedder is responsible for calling this method once feature
  // list initialization is complete. Called in every process.
  virtual void PostFieldTrialInitialization() {}

  // Allows the embedder to perform its own initialization after early content
  // initialization. At this point, it is possible to post to base::ThreadPool
  // or to the main thread loop via base::ThreadTaskRunnerHandle, but the tasks
  // won't run immediately.
  //
  // If ShouldCreateFeatureList() returns true, the field trials and FeatureList
  // have been initialized. Otherwise, the implementation must initialize the
  // field trials and FeatureList and call PostFieldTrialInitialization().
  //
  // |is_running_tests| indicates whether it is running in tests.
  virtual void PostEarlyInitialization(bool is_running_tests) {}

#if defined(OS_WIN)
  // Allows the embedder to indicate that console control events (e.g., Ctrl-C,
  // Ctrl-break, or closure of the console) are to be handled. By default, these
  // events are not handled, leading to process termination. When an embedder
  // returns true to indicate that these events are to be handled, the
  // embedder's ContentBrowserClient::SessionEnding function will be called
  // when a console control event is received. All non-browser processes will
  // swallow the event.
  virtual bool ShouldHandleConsoleControlEvents();
#endif

 protected:
  friend class ContentClientCreator;
  friend class ContentClientInitializer;
  friend class BrowserTestBase;

  // Called once per relevant process type to allow the embedder to customize
  // content. If an embedder wants the default (empty) implementation, don't
  // override this.
  virtual ContentClient* CreateContentClient();
  virtual ContentBrowserClient* CreateContentBrowserClient();
  virtual ContentGpuClient* CreateContentGpuClient();
  virtual ContentRendererClient* CreateContentRendererClient();
  virtual ContentUtilityClient* CreateContentUtilityClient();
};

}  // namespace content

#endif  // CONTENT_PUBLIC_APP_CONTENT_MAIN_DELEGATE_H_
