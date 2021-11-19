// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/first_run_dialog.h"

#include <string>

#include "base/bind.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/first_run/first_run_dialog.h"
#include "chrome/browser/metrics/metrics_reporting_state.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/crash/core/app/breakpad_linux.h"
#include "components/crash/core/app/crashpad.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/link.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "components/metrics/structured/neutrino_logging.h"  // nogncheck
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace {

#if !defined(OS_MAC)
void InitCrashReporterIfEnabled(bool enabled) {
  if (!crash_reporter::IsCrashpadEnabled() && enabled)
    breakpad::InitCrashReporter(std::string());
}
#endif

}  // namespace

namespace first_run {

void ShowFirstRunDialog(Profile* profile) {
#if defined(OS_MAC)
  if (base::FeatureList::IsEnabled(features::kViewsFirstRunDialog))
    ShowFirstRunDialogViews(profile);
  else
    ShowFirstRunDialogCocoa(profile);
#else
  ShowFirstRunDialogViews(profile);
#endif
}

void ShowFirstRunDialogViews(Profile* profile) {
  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  FirstRunDialog::Show(
      base::BindRepeating(&platform_util::OpenExternal,
                          base::Unretained(profile),
                          GURL(chrome::kLearnMoreReportingURL)),
      run_loop.QuitClosure());
  run_loop.Run();
}

}  // namespace first_run

// static
void FirstRunDialog::Show(base::RepeatingClosure learn_more_callback,
                          base::RepeatingClosure quit_runloop) {
  FirstRunDialog* dialog = new FirstRunDialog(std::move(learn_more_callback),
                                              std::move(quit_runloop));
  views::DialogDelegate::CreateDialogWidget(dialog, NULL, NULL)->Show();
}

FirstRunDialog::FirstRunDialog(base::RepeatingClosure learn_more_callback,
                               base::RepeatingClosure quit_runloop)
    : quit_runloop_(quit_runloop) {
  SetTitle(l10n_util::GetStringUTF16(IDS_FIRST_RUN_DIALOG_WINDOW_TITLE));
  SetButtons(ui::DIALOG_BUTTON_OK);
  SetExtraView(
      std::make_unique<views::Link>(l10n_util::GetStringUTF16(IDS_LEARN_MORE)))
      ->SetCallback(std::move(learn_more_callback));

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      ChromeLayoutProvider::Get()->GetDialogInsetsForContentType(
          views::DialogContentType::kControl,
          views::DialogContentType::kControl),
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_RELATED_CONTROL_VERTICAL)));

  make_default_ = AddChildView(std::make_unique<views::Checkbox>(
      l10n_util::GetStringUTF16(IDS_FR_CUSTOMIZE_DEFAULT_BROWSER)));
  make_default_->SetChecked(true);

  report_crashes_ = AddChildView(std::make_unique<views::Checkbox>(
      l10n_util::GetStringUTF16(IDS_FR_ENABLE_LOGGING)));
  // Having this box checked means the user has to opt-out of metrics recording.
  report_crashes_->SetChecked(!first_run::IsMetricsReportingOptIn());

  chrome::RecordDialogCreation(chrome::DialogIdentifier::FIRST_RUN_DIALOG);
}

FirstRunDialog::~FirstRunDialog() {
}

void FirstRunDialog::Done() {
  CHECK(!quit_runloop_.is_null());
  quit_runloop_.Run();
}

bool FirstRunDialog::Accept() {
  GetWidget()->Hide();

#if defined(OS_MAC)
  ChangeMetricsReportingState(report_crashes_->GetChecked());
#else
#if BUILDFLAG(IS_CHROMEOS_ASH)
  metrics::structured::NeutrinoDevicesLog(
      metrics::structured::NeutrinoDevicesLocation::kFirstRunDialog);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  ChangeMetricsReportingStateWithReply(
      report_crashes_->GetChecked(),
      base::BindOnce(&InitCrashReporterIfEnabled));
#endif

  if (make_default_->GetChecked())
    shell_integration::SetAsDefaultBrowser();

  Done();
  return true;
}

void FirstRunDialog::WindowClosing() {
  first_run::SetShouldShowWelcomePage();
  Done();
}

BEGIN_METADATA(FirstRunDialog, views::DialogDelegateView)
END_METADATA
