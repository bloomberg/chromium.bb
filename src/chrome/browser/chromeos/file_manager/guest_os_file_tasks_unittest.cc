// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_manager/guest_os_file_tasks.h"

#include "base/files/file_path.h"
#include "base/values.h"
#include "chrome/browser/chromeos/crostini/crostini_pref_names.h"
#include "chrome/browser/chromeos/guest_os/guest_os_pref_names.h"
#include "chrome/browser/chromeos/guest_os/guest_os_registry_service.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/entry_info.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace file_manager {
namespace file_tasks {
namespace {

static constexpr auto VM_TERMINA =
    guest_os::GuestOsRegistryService::VmType::ApplicationList_VmType_TERMINA;
static constexpr auto PLUGIN_VM =
    guest_os::GuestOsRegistryService::VmType::ApplicationList_VmType_PLUGIN_VM;

}  // namespace

class CrostiniFileTasksTest : public testing::Test {
 public:
  CrostiniFileTasksTest() = default;

 protected:
  void AddApp(const std::string& id,
              const std::string& name,
              const std::vector<std::string>& mimes,
              const std::vector<std::string>& extensions,
              guest_os::GuestOsRegistryService::VmType vm_type) {
    // crostini.registry {<id>: {container_name: "penguin", name: {"": <name>},
    //                           mime_types: [<mime>,], vm_name: "termina"}}
    DictionaryPrefUpdate update(profile_.GetPrefs(),
                                guest_os::prefs::kGuestOsRegistry);
    base::DictionaryValue* registry = update.Get();
    base::Value app(base::Value::Type::DICTIONARY);
    app.SetKey("container_name", base::Value("penguin"));
    base::Value mime_list(base::Value::Type::LIST);
    for (const auto& mime : mimes)
      mime_list.Append(mime);
    app.SetKey("mime_types", std::move(mime_list));
    base::Value extension_list(base::Value::Type::LIST);
    for (const auto& extension : extensions)
      extension_list.Append(extension);
    app.SetKey("extensions", std::move(extension_list));
    base::Value name_dict(base::Value::Type::DICTIONARY);
    name_dict.SetKey("", base::Value(name));
    app.SetKey("name", std::move(name_dict));
    app.SetKey("vm_name", base::Value("termina"));
    app.SetIntKey("vm_type", static_cast<int>(vm_type));
    registry->SetKey(id, std::move(app));
  }

  void AddEntry(const std::string& path, const std::string& mime) {
    entries_.push_back(
        extensions::EntryInfo(base::FilePath(path), mime, false));
  }

  void AddMime(const std::string& file_ext, const std::string& mime) {
    // crostini.mime_types {<termina/penguin/<file_ext>:
    // {container_name: "penguin", mime_type: <mime>, vm_name: "termina"}}
    DictionaryPrefUpdate update(profile_.GetPrefs(),
                                crostini::prefs::kCrostiniMimeTypes);
    base::DictionaryValue* mimes = update.Get();
    base::Value mime_dict(base::Value::Type::DICTIONARY);
    mime_dict.SetKey("container_name", base::Value("penguin"));
    mime_dict.SetKey("mime_type", base::Value(mime));
    mime_dict.SetKey("vm_name", base::Value("termina"));
    mimes->SetKey("termina/penguin/" + file_ext, std::move(mime_dict));
  }

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  std::vector<extensions::EntryInfo> entries_;
  std::vector<std::string> app_ids_;
  std::vector<std::string> app_names_;
  std::vector<guest_os::GuestOsRegistryService::VmType> app_vm_types_;

  DISALLOW_COPY_AND_ASSIGN(CrostiniFileTasksTest);
};

TEST_F(CrostiniFileTasksTest, NoApps) {
  AddApp("app1", "name1", {"test/mime1"}, {}, VM_TERMINA);
  AddEntry("entry.txt", "test/mime2");
  FindGuestOsApps(&profile_, entries_, &app_ids_, &app_names_, &app_vm_types_);
  EXPECT_THAT(app_ids_, testing::IsEmpty());
  EXPECT_THAT(app_names_, testing::IsEmpty());
  EXPECT_THAT(app_vm_types_, testing::IsEmpty());
}

TEST_F(CrostiniFileTasksTest, Termina_AppRegistered) {
  AddApp("app1", "name1", {"test/mime1"}, {}, VM_TERMINA);
  AddEntry("entry.txt", "test/mime1");
  FindGuestOsApps(&profile_, entries_, &app_ids_, &app_names_, &app_vm_types_);
  EXPECT_THAT(app_ids_, testing::ElementsAre("app1"));
  EXPECT_THAT(app_names_, testing::ElementsAre("name1"));
  EXPECT_THAT(app_vm_types_, testing::ElementsAre(VM_TERMINA));
}

TEST_F(CrostiniFileTasksTest, PluginVm_AppRegistered) {
  AddApp("app1", "name1", {}, {"txt"}, PLUGIN_VM);
  AddEntry("entry.txt", "test/mime1");
  FindGuestOsApps(&profile_, entries_, &app_ids_, &app_names_, &app_vm_types_);
  EXPECT_THAT(app_ids_, testing::ElementsAre("app1"));
  EXPECT_THAT(app_names_, testing::ElementsAre("name1"));
  EXPECT_THAT(app_vm_types_, testing::ElementsAre(PLUGIN_VM));
}

TEST_F(CrostiniFileTasksTest, Termina_NotAllEntries) {
  AddApp("app1", "name1", {"test/mime1"}, {}, VM_TERMINA);
  AddApp("app2", "name2", {"test/mime2"}, {}, VM_TERMINA);
  AddEntry("entry1.txt", "test/mime1");
  AddEntry("entry2.txt", "test/mime2");
  FindGuestOsApps(&profile_, entries_, &app_ids_, &app_names_, &app_vm_types_);
  EXPECT_THAT(app_ids_, testing::IsEmpty());
  EXPECT_THAT(app_names_, testing::IsEmpty());
  EXPECT_THAT(app_vm_types_, testing::IsEmpty());
}

TEST_F(CrostiniFileTasksTest, PluginVm_NotAllEntries) {
  AddApp("app1", "name1", {}, {"txt"}, PLUGIN_VM);
  AddApp("app2", "name2", {}, {"jpg"}, PLUGIN_VM);
  AddEntry("entry1.txt", "test/mime1");
  AddEntry("entry2.jpg", "test/mime2");
  FindGuestOsApps(&profile_, entries_, &app_ids_, &app_names_, &app_vm_types_);
  EXPECT_THAT(app_ids_, testing::IsEmpty());
  EXPECT_THAT(app_names_, testing::IsEmpty());
  EXPECT_THAT(app_vm_types_, testing::IsEmpty());
}

TEST_F(CrostiniFileTasksTest, Termina_MultipleAppsRegistered) {
  AddApp("app1", "name1", {"test/mime1"}, {}, VM_TERMINA);
  AddApp("app2", "name2", {"test/mime1"}, {}, VM_TERMINA);
  AddEntry("entry.txt", "test/mime1");
  FindGuestOsApps(&profile_, entries_, &app_ids_, &app_names_, &app_vm_types_);
  EXPECT_THAT(app_ids_, testing::ElementsAre("app1", "app2"));
  EXPECT_THAT(app_names_, testing::ElementsAre("name1", "name2"));
  EXPECT_THAT(app_vm_types_, testing::ElementsAre(VM_TERMINA, VM_TERMINA));
}

TEST_F(CrostiniFileTasksTest, PluginVm_MultipleAppsRegistered) {
  AddApp("app1", "name1", {}, {"txt"}, PLUGIN_VM);
  AddApp("app2", "name2", {}, {"txt"}, PLUGIN_VM);
  AddEntry("entry.txt", "test/mime1");
  FindGuestOsApps(&profile_, entries_, &app_ids_, &app_names_, &app_vm_types_);
  EXPECT_THAT(app_ids_, testing::ElementsAre("app1", "app2"));
  EXPECT_THAT(app_names_, testing::ElementsAre("name1", "name2"));
  EXPECT_THAT(app_vm_types_, testing::ElementsAre(PLUGIN_VM, PLUGIN_VM));
}

TEST_F(CrostiniFileTasksTest, MultipleAppsFromMultipleVmsRegistered) {
  AddApp("app1", "name1", {"test/mime1"}, {}, VM_TERMINA);
  AddApp("app2", "name2", {}, {"txt"}, PLUGIN_VM);
  AddEntry("entry.txt", "test/mime1");
  FindGuestOsApps(&profile_, entries_, &app_ids_, &app_names_, &app_vm_types_);
  EXPECT_THAT(app_ids_, testing::ElementsAre("app1", "app2"));
  EXPECT_THAT(app_names_, testing::ElementsAre("name1", "name2"));
  EXPECT_THAT(app_vm_types_, testing::ElementsAre(VM_TERMINA, PLUGIN_VM));
}

TEST_F(CrostiniFileTasksTest, AppRegisteredForTextPlain) {
  AddApp("app1", "name1", {"text/plain"}, {}, VM_TERMINA);
  AddEntry("entry.js", "text/javascript");
  FindGuestOsApps(&profile_, entries_, &app_ids_, &app_names_, &app_vm_types_);
  EXPECT_THAT(app_ids_, testing::ElementsAre("app1"));
  EXPECT_THAT(app_names_, testing::ElementsAre("name1"));
  EXPECT_THAT(app_vm_types_, testing::ElementsAre(VM_TERMINA));
}

TEST_F(CrostiniFileTasksTest, MimeServiceForTextPlain) {
  AddApp("app1", "name1", {"test/mime1"}, {}, VM_TERMINA);
  AddEntry("entry.unknown", "text/plain");
  AddMime("unknown", "test/mime1");
  FindGuestOsApps(&profile_, entries_, &app_ids_, &app_names_, &app_vm_types_);
  EXPECT_THAT(app_ids_, testing::ElementsAre("app1"));
  EXPECT_THAT(app_names_, testing::ElementsAre("name1"));
  EXPECT_THAT(app_vm_types_, testing::ElementsAre(VM_TERMINA));
}

TEST_F(CrostiniFileTasksTest, MimeServiceForApplicationOctetStream) {
  AddApp("app1", "name1", {"test/mime1"}, {}, VM_TERMINA);
  AddEntry("entry.unknown", "application/octet-stream");
  AddMime("unknown", "test/mime1");
  FindGuestOsApps(&profile_, entries_, &app_ids_, &app_names_, &app_vm_types_);
  EXPECT_THAT(app_ids_, testing::ElementsAre("app1"));
  EXPECT_THAT(app_names_, testing::ElementsAre("name1"));
  EXPECT_THAT(app_vm_types_, testing::ElementsAre(VM_TERMINA));
}

}  // namespace file_tasks
}  // namespace file_manager
