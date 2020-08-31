// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "base/files/file_util.h"
#include "base/path_service.h"
#include "chrome/browser/extensions/chrome_content_verifier_delegate.h"
#include "chrome/browser/extensions/extension_service_test_with_install.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/testing_profile.h"
#include "extensions/browser/content_verifier/test_utils.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/info_map.h"
#include "extensions/common/file_util.h"

namespace extensions {

namespace {

constexpr char kCaseSensitiveManifestPathsCrx[] =
    "content_verifier/case_sensitive_manifest_paths.crx";

std::set<base::FilePath> ToFilePaths(const std::set<std::string>& paths) {
  std::set<base::FilePath> file_paths;
  for (const auto& path : paths)
    file_paths.insert(base::FilePath().AppendASCII(path));
  return file_paths;
}

bool IsSuperset(const std::set<base::FilePath>& container,
                const std::set<base::FilePath>& candidates) {
  std::vector<base::FilePath> difference;
  std::set_difference(candidates.begin(), candidates.end(), container.begin(),
                      container.end(), std::back_inserter(difference));
  return difference.empty();
}

}  // namespace

// Tests are run with //chrome layer so that manifest's //chrome specific bits
// (e.g. browser images, default_icon in actions) are present.
class ChromeContentVerifierTest : public ExtensionServiceTestWithInstall {
 public:
  void SetUp() override {
    ExtensionServiceTestWithInstall::SetUp();

    // Note: we need a separate TestingProfile (other than our base class)
    // because we need it to build |content_verifier_| below in SetUp().
    testing_profile_ = TestingProfile::Builder().Build();

    // Set up content verification.
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    command_line->AppendSwitchASCII(
        switches::kExtensionContentVerification,
        switches::kExtensionContentVerificationEnforce);
    auto delegate =
        std::make_unique<ChromeContentVerifierDelegate>(browser_context());
    delegate_raw_ = delegate.get();
    content_verifier_ = base::MakeRefCounted<ContentVerifier>(
        browser_context(), std::move(delegate));
    info_map()->SetContentVerifier(content_verifier_.get());
    content_verifier_->Start();
  }

  void TearDown() override {
    content_verifier_->Shutdown();
    ExtensionServiceTestWithInstall::TearDown();
  }

  testing::AssertionResult InstallExtension(const std::string& crx_path_str) {
    if (extension_) {
      return testing::AssertionFailure()
             << "Only one extension is allowed to be installed in this test. "
             << "Error while installing crx from: " << crx_path_str;
    }
    InitializeEmptyExtensionService();
    base::FilePath data_dir;
    if (!base::PathService::Get(chrome::DIR_TEST_DATA, &data_dir))
      return testing::AssertionFailure() << "DIR_TEST_DATA not found";
    base::FilePath crx_full_path =
        data_dir.AppendASCII("extensions").AppendASCII(crx_path_str);
    extension_ = InstallCRX(crx_full_path, INSTALL_NEW);
    if (!extension_)
      return testing::AssertionFailure()
             << "Failed to install extension at " << crx_full_path;
    return testing::AssertionSuccess();
  }

  void AddExtensionToContentVerifier(
      const scoped_refptr<const Extension>& extension,
      VerifierObserver* verifier_observer) {
    info_map()->AddExtension(extension.get(), base::Time::Now(), false, false);
    EXPECT_TRUE(
        ExtensionRegistry::Get(browser_context())->AddEnabled(extension));
    ExtensionRegistry::Get(browser_context())->TriggerOnLoaded(extension.get());

    // Ensure that content verifier has checked hashes from |extension|.
    EXPECT_EQ(ChromeContentVerifierDelegate::VerifierSourceType::SIGNED_HASHES,
              delegate_raw_->GetVerifierSourceType(*extension));

    verifier_observer->EnsureFetchCompleted(extension->id());
  }

  scoped_refptr<ContentVerifier>& content_verifier() {
    return content_verifier_;
  }

  const scoped_refptr<const Extension>& extension() { return extension_; }

  bool ShouldVerifyAnyPaths(
      const std::set<base::FilePath>& relative_unix_paths) const {
    return content_verifier_->ShouldVerifyAnyPathsForTesting(
        extension_->id(), extension_->path(), relative_unix_paths);
  }

 private:
  InfoMap* info_map() {
    return ExtensionSystem::Get(browser_context())->info_map();
  }

  content::BrowserContext* browser_context() { return testing_profile_.get(); }

  scoped_refptr<const Extension> extension_;

  // Owned by |content_verifier_|.
  ChromeContentVerifierDelegate* delegate_raw_ = nullptr;

  scoped_refptr<ContentVerifier> content_verifier_;
  std::unique_ptr<TestingProfile> testing_profile_;
};

// Tests that an extension with mixed case resources specified in manifest.json
// (messages, browser images, browserAction.default_icon) loads correctly.
TEST_F(ChromeContentVerifierTest, CaseSensitivityInManifestPaths) {
  VerifierObserver verifier_observer;
  ASSERT_TRUE(InstallExtension(kCaseSensitiveManifestPathsCrx));

  // Make sure computed_hashes.json does not exist as this test relies on its
  // generation to discover hash_mismatch_unix_paths().
  ASSERT_FALSE(
      base::PathExists(file_util::GetComputedHashesPath(extension()->path())));

  AddExtensionToContentVerifier(extension(), &verifier_observer);
  ASSERT_TRUE(
      base::PathExists(file_util::GetComputedHashesPath(extension()->path())));

  // Known paths that are transcoded in |extension| crx.
  std::set<std::string> transcoded_paths = {"_locales/de_AT/messages.json",
                                            "_locales/en_GB/messages.json",
                                            "H.png", "g.png", "i.png"};
  // Ensure we've seen known paths as hash-mismatch on FetchComplete.
  EXPECT_TRUE(IsSuperset(verifier_observer.hash_mismatch_unix_paths(),
                         ToFilePaths(transcoded_paths)));
  // Sanity check: ensure they are explicitly excluded from verification.
  EXPECT_FALSE(ShouldVerifyAnyPaths(ToFilePaths({"_locales/de_AT/messages.json",
                                                 "_locales/en_GB/messages.json",
                                                 "H.png", "g.png", "i.png"})));

  // Make sure we haven't seen ContentVerifier::VerifyFailed
  EXPECT_FALSE(verifier_observer.did_hash_mismatch());

  // Ensure transcoded paths are handled correctly with different case in
  // case-insensitive OS. They should still be excluded from verification (i.e.
  // ShouldVerifyAnyPaths should return false for them).
  if (!content_verifier_utils::IsFileAccessCaseSensitive()) {
    EXPECT_FALSE(ShouldVerifyAnyPaths(ToFilePaths(
        {"_locales/de_at/messages.json", "_locales/en_gb/messages.json",
         "h.png", "G.png", "I.png"})));
  }

  // Ensure transcoded paths are handled correctly with dot-space suffix added
  // to them in OS that ignores dot-space suffix (win). They should still be
  // excluded from verification (i.e. ShouldVerifyAnyPaths should return false
  // for them).
  if (content_verifier_utils::IsDotSpaceFilenameSuffixIgnored()) {
    EXPECT_FALSE(ShouldVerifyAnyPaths(ToFilePaths(
        {"_locales/de_AT/messages.json.", "_locales/en_GB/messages.json ",
         "H.png .", "g.png ..", "i.png.."})));

    // Ensure the same with different case filenames.
    if (!content_verifier_utils::IsFileAccessCaseSensitive()) {
      EXPECT_FALSE(ShouldVerifyAnyPaths(ToFilePaths(
          {"_locales/de_at/messages.json.", "_locales/en_gb/messages.json ",
           "h.png .", "G.png ..", "I.png.."})));
    }
  }
}

// Tests that tampered resources cause verification failure due to hash mismatch
// during OnExtensionLoaded.
TEST_F(ChromeContentVerifierTest, VerifyFailedOnLoad) {
  VerifierObserver verifier_observer;
  ASSERT_TRUE(InstallExtension(kCaseSensitiveManifestPathsCrx));

  // Before ContentVerifier sees |extension|, tamper with a JS file.
  {
    constexpr char kTamperedContent[] = "// Evil content";
    base::FilePath background_script_path =
        extension()->path().AppendASCII("d.js");
    ASSERT_EQ(static_cast<int>(sizeof(kTamperedContent)),
              base::WriteFile(background_script_path, kTamperedContent,
                              sizeof(kTamperedContent)));
  }

  AddExtensionToContentVerifier(extension(), &verifier_observer);

  // Expect a hash mismatch for tampered d.js file.
  EXPECT_TRUE(verifier_observer.did_hash_mismatch());
}

}  // namespace extensions
