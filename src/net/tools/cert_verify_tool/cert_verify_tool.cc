// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/message_loop/message_pump_type.h"
#include "base/strings/string_split.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "net/cert/cert_net_fetcher.h"
#include "net/cert/cert_verify_proc.h"
#include "net/cert/cert_verify_proc_builtin.h"
#include "net/cert/crl_set.h"
#include "net/cert/internal/system_trust_store.h"
#include "net/cert_net/cert_net_fetcher_url_request.h"
#include "net/tools/cert_verify_tool/cert_verify_tool_util.h"
#include "net/tools/cert_verify_tool/verify_using_cert_verify_proc.h"
#include "net/tools/cert_verify_tool/verify_using_path_builder.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_context_getter.h"

#if defined(OS_LINUX)
#include "net/proxy_resolution/proxy_config.h"
#include "net/proxy_resolution/proxy_config_service_fixed.h"
#endif

namespace {

std::string GetUserAgent() {
  return "cert_verify_tool/0.1";
}

void SetUpOnNetworkThread(
    std::unique_ptr<net::URLRequestContext>* context,
    scoped_refptr<net::CertNetFetcherURLRequest>* cert_net_fetcher,
    base::WaitableEvent* initialization_complete_event) {
  net::URLRequestContextBuilder url_request_context_builder;
  url_request_context_builder.set_user_agent(GetUserAgent());
#if defined(OS_LINUX)
  // On Linux, use a fixed ProxyConfigService, since the default one
  // depends on glib.
  //
  // TODO(akalin): Remove this once http://crbug.com/146421 is fixed.
  url_request_context_builder.set_proxy_config_service(
      std::make_unique<net::ProxyConfigServiceFixed>(
          net::ProxyConfigWithAnnotation()));
#endif
  *context = url_request_context_builder.Build();

  // TODO(mattm): add command line flag to configure using
  // CertNetFetcher
  *cert_net_fetcher = base::MakeRefCounted<net::CertNetFetcherURLRequest>();
  (*cert_net_fetcher)->SetURLRequestContext(context->get());
  initialization_complete_event->Signal();
}

void ShutdownOnNetworkThread(
    std::unique_ptr<net::URLRequestContext>* context,
    scoped_refptr<net::CertNetFetcherURLRequest>* cert_net_fetcher) {
  (*cert_net_fetcher)->Shutdown();
  cert_net_fetcher->reset();
  context->reset();
}

// Base class to abstract running a particular implementation of certificate
// verification.
class CertVerifyImpl {
 public:
  virtual ~CertVerifyImpl() = default;

  virtual std::string GetName() const = 0;

  // Does certificate verification.
  //
  // Note that |hostname| may be empty to indicate that no name validation is
  // requested, and a null value of |verify_time| means to use the current time.
  virtual bool VerifyCert(const CertInput& target_der_cert,
                          const std::string& hostname,
                          const std::vector<CertInput>& intermediate_der_certs,
                          const std::vector<CertInput>& root_der_certs,
                          base::Time verify_time,
                          net::CRLSet* crl_set,
                          const base::FilePath& dump_prefix_path) = 0;
};

// Runs certificate verification using a particular CertVerifyProc.
class CertVerifyImplUsingProc : public CertVerifyImpl {
 public:
  CertVerifyImplUsingProc(const std::string& name,
                          scoped_refptr<net::CertVerifyProc> proc)
      : name_(name), proc_(std::move(proc)) {}

  std::string GetName() const override { return name_; }

  bool VerifyCert(const CertInput& target_der_cert,
                  const std::string& hostname,
                  const std::vector<CertInput>& intermediate_der_certs,
                  const std::vector<CertInput>& root_der_certs,
                  base::Time verify_time,
                  net::CRLSet* crl_set,
                  const base::FilePath& dump_prefix_path) override {
    if (!verify_time.is_null()) {
      std::cerr << "WARNING: --time is not supported by " << GetName()
                << ", will use current time.\n";
    }

    if (hostname.empty()) {
      std::cerr << "ERROR: --hostname is required for " << GetName()
                << ", skipping\n";
      return true;  // "skipping" is considered a successful return.
    }

    base::FilePath dump_path;
    if (!dump_prefix_path.empty()) {
      dump_path = dump_prefix_path.AddExtension(FILE_PATH_LITERAL(".pem"))
                      .InsertBeforeExtensionASCII("." + GetName());
    }

    return VerifyUsingCertVerifyProc(proc_.get(), target_der_cert, hostname,
                                     intermediate_der_certs, root_der_certs,
                                     crl_set, dump_path);
  }

 private:
  const std::string name_;
  scoped_refptr<net::CertVerifyProc> proc_;
};

// Runs certificate verification using CertPathBuilder.
class CertVerifyImplUsingPathBuilder : public CertVerifyImpl {
 public:
  explicit CertVerifyImplUsingPathBuilder(
      scoped_refptr<net::CertNetFetcher> cert_net_fetcher,
      std::unique_ptr<net::SystemTrustStoreProvider>
          system_trust_store_provider)
      : cert_net_fetcher_(std::move(cert_net_fetcher)),
        system_trust_store_provider_(std::move(system_trust_store_provider)) {}

  std::string GetName() const override { return "CertPathBuilder"; }

  bool VerifyCert(const CertInput& target_der_cert,
                  const std::string& hostname,
                  const std::vector<CertInput>& intermediate_der_certs,
                  const std::vector<CertInput>& root_der_certs,
                  base::Time verify_time,
                  net::CRLSet* crl_set,
                  const base::FilePath& dump_prefix_path) override {
    if (!hostname.empty()) {
      std::cerr << "WARNING: --hostname is not verified with CertPathBuilder\n";
    }

    if (verify_time.is_null()) {
      verify_time = base::Time::Now();
    }

    return VerifyUsingPathBuilder(
        target_der_cert, intermediate_der_certs, root_der_certs, verify_time,
        dump_prefix_path, cert_net_fetcher_,
        system_trust_store_provider_->CreateSystemTrustStore());
  }

 private:
  scoped_refptr<net::CertNetFetcher> cert_net_fetcher_;
  std::unique_ptr<net::SystemTrustStoreProvider> system_trust_store_provider_;
};

class DummySystemTrustStoreProvider : public net::SystemTrustStoreProvider {
 public:
  std::unique_ptr<net::SystemTrustStore> CreateSystemTrustStore() override {
    return net::CreateEmptySystemTrustStore();
  }
};

std::unique_ptr<net::SystemTrustStoreProvider> CreateSystemTrustStoreProvider(
    bool use_system_roots) {
  return use_system_roots ? net::SystemTrustStoreProvider::CreateDefaultForSSL()
                          : std::make_unique<DummySystemTrustStoreProvider>();
}

// Creates an subclass of CertVerifyImpl based on its name, or returns nullptr.
std::unique_ptr<CertVerifyImpl> CreateCertVerifyImplFromName(
    base::StringPiece impl_name,
    scoped_refptr<net::CertNetFetcher> cert_net_fetcher,
    bool use_system_roots) {
#if !(defined(OS_FUCHSIA) || defined(OS_LINUX) || defined(OS_CHROMEOS))
  if (impl_name == "platform") {
    if (!use_system_roots) {
      std::cerr << "WARNING: platform verifier not supported with "
                   "--no-system-roots, skipping.\n";
      return nullptr;
    }

    return std::make_unique<CertVerifyImplUsingProc>(
        "CertVerifyProc (system)", net::CertVerifyProc::CreateSystemVerifyProc(
                                       std::move(cert_net_fetcher)));
  }
#endif

  if (impl_name == "builtin") {
    return std::make_unique<CertVerifyImplUsingProc>(
        "CertVerifyProcBuiltin",
        net::CreateCertVerifyProcBuiltin(
            std::move(cert_net_fetcher),
            CreateSystemTrustStoreProvider(use_system_roots)));
  }

  if (impl_name == "pathbuilder") {
    return std::make_unique<CertVerifyImplUsingPathBuilder>(
        std::move(cert_net_fetcher),
        CreateSystemTrustStoreProvider(use_system_roots));
  }

  std::cerr << "WARNING: Unrecognized impl: " << impl_name << "\n";
  return nullptr;
}

const char kUsage[] =
    " [flags] <target/chain>\n"
    "\n"
    " <target/chain> is a file containing certificates [1]. Minimally it\n"
    " contains the target certificate. Optionally it may subsequently list\n"
    " additional certificates needed to build a chain (this is equivalent to\n"
    " specifying them through --intermediates)\n"
    "\n"
    "Flags:\n"
    "\n"
    " --hostname=<hostname>\n"
    "      The hostname required to match the end-entity certificate.\n"
    "      Required for the CertVerifyProc implementation.\n"
    "\n"
    " --roots=<certs path>\n"
    "      <certs path> is a file containing certificates [1] to interpret as\n"
    "      trust anchors (without any anchor constraints).\n"
    "\n"
    " --no-system-roots\n"
    "      Do not use system provided trust roots, only trust roots specified\n"
    "      by --roots or --trust-last-cert will be used. Only supported by\n"
    "      the builtin and pathbuilter impls.\n"
    "\n"
    " --intermediates=<certs path>\n"
    "      <certs path> is a file containing certificates [1] for use when\n"
    "      path building is looking for intermediates.\n"
    "\n"
    " --impls=<ordered list of implementations>\n"
    "      Ordered list of the verifier implementations to run. If omitted,\n"
    "      will default to: \"platform,builtin,pathbuilder\".\n"
    "      Changing this can lead to different results in cases where the\n"
    "      platform verifier affects global caches (as in the case of NSS).\n"
    "\n"
    " --trust-last-cert\n"
    "      Removes the final intermediate from the chain and instead adds it\n"
    "      as a root. This is useful when providing a <target/chain>\n"
    "      parameter whose final certificate is a trust anchor.\n"
    "\n"
    " --time=<time>\n"
    "      Use <time> instead of the current system time. <time> is\n"
    "      interpreted in local time if a timezone is not specified.\n"
    "      Many common formats are supported, including:\n"
    "        1994-11-15 12:45:26 GMT\n"
    "        Tue, 15 Nov 1994 12:45:26 GMT\n"
    "        Nov 15 12:45:26 1994 GMT\n"
    "\n"
    " --crlset=<crlset path>\n"
    "      <crlset path> is a file containing a serialized CRLSet to use\n"
    "      during revocation checking. For example:\n"
    "        <chrome data dir>/CertificateRevocation/<number>/crl-set\n"
    "\n"
    " --dump=<file prefix>\n"
    "      Dumps the verified chain to PEM files starting with\n"
    "      <file prefix>.\n"
    "\n"
    "\n"
    "[1] A \"file containing certificates\" means a path to a file that can\n"
    "    either be:\n"
    "    * A binary file containing a single DER-encoded RFC 5280 Certificate\n"
    "    * A PEM file containing one or more CERTIFICATE blocks (DER-encoded\n"
    "      RFC 5280 Certificate)\n";

void PrintUsage(const char* argv0) {
  std::cerr << "Usage: " << argv0 << kUsage;

  // TODO(mattm): allow <certs path> to be a directory containing DER/PEM files?
  // TODO(mattm): allow target to specify an HTTPS URL to check the cert of?
  // TODO(mattm): allow target to be a verify_certificate_chain_unittest .test
  // file?
  // TODO(mattm): allow specifying ocsp_response and sct_list inputs as well.
}

}  // namespace

int main(int argc, char** argv) {
  base::AtExitManager at_exit_manager;
  if (!base::CommandLine::Init(argc, argv)) {
    std::cerr << "ERROR in CommandLine::Init\n";
    return 1;
  }
  base::ThreadPoolInstance::CreateAndStartWithDefaultParams("cert_verify_tool");
  base::ScopedClosureRunner cleanup(
      base::BindOnce([] { base::ThreadPoolInstance::Get()->Shutdown(); }));
  base::CommandLine& command_line = *base::CommandLine::ForCurrentProcess();
  logging::LoggingSettings settings;
  settings.logging_dest =
      logging::LOG_TO_SYSTEM_DEBUG_LOG | logging::LOG_TO_STDERR;
  logging::InitLogging(settings);

  base::CommandLine::StringVector args = command_line.GetArgs();
  if (args.size() != 1U || command_line.HasSwitch("help")) {
    PrintUsage(argv[0]);
    return 1;
  }

  std::string hostname = command_line.GetSwitchValueASCII("hostname");

  base::Time verify_time;
  std::string time_flag = command_line.GetSwitchValueASCII("time");
  if (!time_flag.empty()) {
    if (!base::Time::FromString(time_flag.c_str(), &verify_time)) {
      std::cerr << "Error parsing --time flag\n";
      return 1;
    }
  }

  bool use_system_roots = !command_line.HasSwitch("no-system-roots");

  base::FilePath roots_path = command_line.GetSwitchValuePath("roots");
  base::FilePath intermediates_path =
      command_line.GetSwitchValuePath("intermediates");
  base::FilePath target_path = base::FilePath(args[0]);

  base::FilePath crlset_path = command_line.GetSwitchValuePath("crlset");
  scoped_refptr<net::CRLSet> crl_set = net::CRLSet::BuiltinCRLSet();
  if (!crlset_path.empty()) {
    std::string crl_set_bytes;
    if (!ReadFromFile(crlset_path, &crl_set_bytes))
      return 1;
    if (!net::CRLSet::Parse(crl_set_bytes, &crl_set)) {
      std::cerr << "Error parsing CRLSet\n";
      return 1;
    }
  }

  base::FilePath dump_prefix_path = command_line.GetSwitchValuePath("dump");

  std::vector<CertInput> root_der_certs;
  std::vector<CertInput> intermediate_der_certs;
  CertInput target_der_cert;

  if (!roots_path.empty())
    ReadCertificatesFromFile(roots_path, &root_der_certs);
  if (!intermediates_path.empty())
    ReadCertificatesFromFile(intermediates_path, &intermediate_der_certs);

  if (!ReadChainFromFile(target_path, &target_der_cert,
                         &intermediate_der_certs)) {
    std::cerr << "ERROR: Couldn't read certificate chain\n";
    return 1;
  }

  if (target_der_cert.der_cert.empty()) {
    std::cerr << "ERROR: no target cert\n";
    return 1;
  }

  // If --trust-last-cert was specified, move the final intermediate to the
  // roots list.
  if (command_line.HasSwitch("trust-last-cert")) {
    if (intermediate_der_certs.empty()) {
      std::cerr << "ERROR: no intermediate certificates\n";
      return 1;
    }

    root_der_certs.push_back(intermediate_der_certs.back());
    intermediate_der_certs.pop_back();
  }

  // Create a network thread to be used for AIA fetches, and wait for a
  // CertNetFetcher to be constructed on that thread.
  base::Thread::Options options(base::MessagePumpType::IO, 0);
  base::Thread thread("network_thread");
  CHECK(thread.StartWithOptions(options));
  // Owned by this thread, but initialized, used, and shutdown on the network
  // thread.
  std::unique_ptr<net::URLRequestContext> context;
  scoped_refptr<net::CertNetFetcherURLRequest> cert_net_fetcher;
  base::WaitableEvent initialization_complete_event(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  thread.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&SetUpOnNetworkThread, &context, &cert_net_fetcher,
                     &initialization_complete_event));
  initialization_complete_event.Wait();

  std::vector<std::unique_ptr<CertVerifyImpl>> impls;

  // Parse the ordered list of CertVerifyImpl passed via command line flags into
  // |impls|.
  std::string impls_str = command_line.GetSwitchValueASCII("impls");
  if (impls_str.empty()) {
    // Default value.
#if !defined(OS_FUCHSIA)
    impls_str = "platform,";
#endif
    impls_str += "builtin,pathbuilder";
  }

  std::vector<std::string> impl_names = base::SplitString(
      impls_str, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  for (const std::string& impl_name : impl_names) {
    auto verify_impl = CreateCertVerifyImplFromName(impl_name, cert_net_fetcher,
                                                    use_system_roots);
    if (verify_impl)
      impls.push_back(std::move(verify_impl));
  }

  // Sequentially run the chain with each of the selected verifier
  // implementations.
  bool all_impls_success = true;

  for (size_t i = 0; i < impls.size(); ++i) {
    if (i != 0)
      std::cout << "\n";

    std::cout << impls[i]->GetName() << ":\n";
    if (!impls[i]->VerifyCert(target_der_cert, hostname, intermediate_der_certs,
                              root_der_certs, verify_time, crl_set.get(),
                              dump_prefix_path)) {
      all_impls_success = false;
    }
  }

  // Clean up on the network thread and stop it (which waits for the clean up
  // task to run).
  thread.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&ShutdownOnNetworkThread, &context, &cert_net_fetcher));
  thread.Stop();

  return all_impls_success ? 0 : 1;
}
