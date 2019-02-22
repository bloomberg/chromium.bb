// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/controller.h"

#include <utility>

#include "base/task/post_task.h"
#include "components/autofill_assistant/browser/protocol_utils.h"
#include "components/autofill_assistant/browser/ui_controller.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace autofill_assistant {

namespace {

// Time between two periodic script checks.
static constexpr base::TimeDelta kPeriodicScriptCheckInterval =
    base::TimeDelta::FromSeconds(2);

// Number of script checks to run after a call to StartPeriodicScriptChecks.
static constexpr int kPeriodicScriptCheckCount = 10;

}  // namespace

// static
void Controller::CreateAndStartForWebContents(
    content::WebContents* web_contents,
    std::unique_ptr<Client> client,
    std::unique_ptr<std::map<std::string, std::string>> parameters) {
  // Get the key early since |client| will be invalidated when moved below.
  const std::string api_key = client->GetApiKey();
  GURL server_url(client->GetServerUrl());
  DCHECK(server_url.is_valid());
  new Controller(web_contents, std::move(client),
                 WebController::CreateForWebContents(web_contents),
                 std::make_unique<Service>(api_key, server_url,
                                           web_contents->GetBrowserContext()),
                 std::move(parameters));
}

Service* Controller::GetService() {
  return service_.get();
}

UiController* Controller::GetUiController() {
  return client_->GetUiController();
}

WebController* Controller::GetWebController() {
  return web_controller_.get();
}

ClientMemory* Controller::GetClientMemory() {
  return memory_.get();
}

const std::map<std::string, std::string>& Controller::GetParameters() {
  return *parameters_;
}

autofill::PersonalDataManager* Controller::GetPersonalDataManager() {
  return client_->GetPersonalDataManager();
}

content::WebContents* Controller::GetWebContents() {
  return web_contents();
}

Controller::Controller(
    content::WebContents* web_contents,
    std::unique_ptr<Client> client,
    std::unique_ptr<WebController> web_controller,
    std::unique_ptr<Service> service,
    std::unique_ptr<std::map<std::string, std::string>> parameters)
    : content::WebContentsObserver(web_contents),
      client_(std::move(client)),
      web_controller_(std::move(web_controller)),
      service_(std::move(service)),
      script_tracker_(std::make_unique<ScriptTracker>(/* delegate= */ this,
                                                      /* listener= */ this)),
      parameters_(std::move(parameters)),
      memory_(std::make_unique<ClientMemory>()),
      allow_autostart_(true),
      periodic_script_check_scheduled_(false),
      periodic_script_check_count_(false),
      clear_web_contents_delegate_(false),
      weak_ptr_factory_(this) {
  DCHECK(parameters_);

  // Only set the controller as the delegate if web_contents does not yet have
  // one.
  // TODO(crbug.com/806868): Find a better way to get a loading progress instead
  // of using the controller as a web_contents delegate. It may interfere with
  // an already existing delegate.
  if (web_contents->GetDelegate() == nullptr) {
    clear_web_contents_delegate_ = true;
    web_contents->SetDelegate(this);
  }

  GetUiController()->SetUiDelegate(this);
  if (!web_contents->IsLoading()) {
    GetOrCheckScripts(web_contents->GetLastCommittedURL());
  }
}

Controller::~Controller() {
  if (clear_web_contents_delegate_) {
    web_contents()->SetDelegate(nullptr);
  }
}

void Controller::GetOrCheckScripts(const GURL& url) {
  if (script_tracker_->running())
    return;

  if (script_domain_ != url.host()) {
    StopPeriodicScriptChecks();
    script_domain_ = url.host();
    service_->GetScriptsForUrl(
        url, *parameters_,
        base::BindOnce(&Controller::OnGetScripts, base::Unretained(this), url));
  } else {
    script_tracker_->CheckScripts(kPeriodicScriptCheckInterval);
    StartPeriodicScriptChecks();
  }
}

void Controller::StartPeriodicScriptChecks() {
  periodic_script_check_count_ = kPeriodicScriptCheckCount;
  // If periodic checks are running, setting periodic_script_check_count_ keeps
  // them running longer.
  if (periodic_script_check_scheduled_)
    return;
  periodic_script_check_scheduled_ = true;
  base::PostDelayedTaskWithTraits(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(&Controller::OnPeriodicScriptCheck,
                     weak_ptr_factory_.GetWeakPtr()),
      kPeriodicScriptCheckInterval);
}

void Controller::StopPeriodicScriptChecks() {
  periodic_script_check_count_ = 0;
}

void Controller::OnPeriodicScriptCheck() {
  if (periodic_script_check_count_ <= 0) {
    DCHECK_EQ(0, periodic_script_check_count_);
    periodic_script_check_scheduled_ = false;
    return;
  }
  periodic_script_check_count_--;
  script_tracker_->CheckScripts(kPeriodicScriptCheckInterval);
  base::PostDelayedTaskWithTraits(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(&Controller::OnPeriodicScriptCheck,
                     weak_ptr_factory_.GetWeakPtr()),
      kPeriodicScriptCheckInterval);
}

void Controller::OnGetScripts(const GURL& url,
                              bool result,
                              const std::string& response) {
  // If the domain of the current URL changed since the request was sent, the
  // response is not relevant anymore and can be safely discarded.
  if (url.host() != script_domain_)
    return;

  if (!result) {
    LOG(ERROR) << "Failed to get assistant scripts for URL " << url.spec();
    // TODO(crbug.com/806868): Terminate Autofill Assistant.
    return;
  }

  std::vector<std::unique_ptr<Script>> scripts;
  bool parse_result = ProtocolUtils::ParseScripts(response, &scripts);
  DCHECK(parse_result);
  script_tracker_->SetScripts(std::move(scripts));
  script_tracker_->CheckScripts(kPeriodicScriptCheckInterval);
  StartPeriodicScriptChecks();
}

void Controller::OnScriptExecuted(const std::string& script_path,
                                  ScriptExecutor::Result result) {
  GetUiController()->HideOverlay();
  if (!result.success) {
    LOG(ERROR) << "Failed to execute script " << script_path;
    // TODO(crbug.com/806868): Handle script execution failure.
  }

  switch (result.at_end) {
    case ScriptExecutor::SHUTDOWN:
      GetUiController()->Shutdown();  // indirectly deletes this
      return;

    case ScriptExecutor::RESTART:
      script_tracker_ = std::make_unique<ScriptTracker>(/* delegate= */ this,
                                                        /* listener= */ this);
      memory_ = std::make_unique<ClientMemory>();
      script_domain_ = "";
      break;

    case ScriptExecutor::CONTINUE:
      break;

    default:
      DLOG(ERROR) << "Unexpected value for at_end: " << result.at_end;
      break;
  }
  GetOrCheckScripts(web_contents()->GetLastCommittedURL());
}

void Controller::OnClickOverlay() {
  GetUiController()->HideOverlay();
  // TODO(crbug.com/806868): Stop executing scripts.
}

void Controller::OnScriptSelected(const std::string& script_path) {
  DCHECK(!script_path.empty());

  script_tracker_->ClearRunnableScripts();
  GetUiController()->ShowOverlay();
  allow_autostart_ = false;  // Only ever autostart the very first script.
  StopPeriodicScriptChecks();
  script_tracker_->ExecuteScript(
      script_path, base::BindOnce(&Controller::OnScriptExecuted,
                                  // script_tracker_ is owned by Controller.
                                  base::Unretained(this), script_path));
}

void Controller::OnDestroy() {
  delete this;
}

void Controller::DidGetUserInteraction(const blink::WebInputEvent::Type type) {
  switch (type) {
    case blink::WebInputEvent::kTouchStart:
    case blink::WebInputEvent::kGestureTapDown:
      // Disable autostart after interaction with the web page.
      allow_autostart_ = false;

      if (!script_tracker_->running()) {
        script_tracker_->CheckScripts(kPeriodicScriptCheckInterval);
        StartPeriodicScriptChecks();
      }
      break;

    default:
      break;
  }
}

void Controller::OnRunnableScriptsChanged(
    const std::vector<ScriptHandle>& runnable_scripts) {
  // Script selection is disabled when a script is already running. We will
  // check again and maybe update when the current script has finished.
  if (script_tracker_->running())
    return;

  // Under specific conditions, we can directly run a script without first
  // displaying it. This is meant to work only at the very beginning, when no
  // scripts have run, there has been no interaction with the webpage and only
  // if there's exactly one runnable autostartable script.
  if (allow_autostart_) {
    int autostart_count = 0;
    std::string autostart_path;
    for (const auto& script : runnable_scripts) {
      if (script.autostart) {
        autostart_count++;
        autostart_path = script.path;
      }
    }
    if (autostart_count == 1) {
      OnScriptSelected(autostart_path);
      return;
    }
  }

  // Clear the status message or show the specified prompt.
  std::string prompt;
  for (const auto& script : runnable_scripts) {
    // runnable_scripts is ordered by priority.
    if (!script.initial_prompt.empty()) {
      prompt = script.initial_prompt;
      break;
    }
  }
  GetUiController()->ShowStatusMessage(prompt);

  // Update the set of scripts.
  GetUiController()->UpdateScripts(runnable_scripts);
}

void Controller::DocumentAvailableInMainFrame() {
  GetOrCheckScripts(web_contents()->GetLastCommittedURL());
}

void Controller::DidFinishLoad(content::RenderFrameHost* render_frame_host,
                               const GURL& validated_url) {
  // validated_url might not be the page URL. Ignore it and always check the
  // last committed url.
  // Note that we also check for scripts in LoadProgressChanged below. This is
  // the last attempt and occurs later than a load progress of 1.0.
  GetOrCheckScripts(web_contents()->GetLastCommittedURL());
}

void Controller::WebContentsDestroyed() {
  OnDestroy();
}

void Controller::LoadProgressChanged(content::WebContents* source,
                                     double progress) {
  int percent = 100 * progress;
  // We wait for a page to be at least 40 percent loaded. Then a new
  // precondition check is started every additional 20 percent.
  if (percent >= 40 && percent % 20 == 0) {
    DCHECK(web_contents()->GetLastCommittedURL().is_valid());
    // In order to show available scripts as early as possible we start checking
    // preconditions when the page has not yet fully loaded. This can lead to
    // the behavior where scripts are being added sequentially instead of all
    // at the same time. Also, depending on the progress values, we may never
    // actually get here. In that case the only check will happen in
    // DidFinishLoad.
    GetOrCheckScripts(web_contents()->GetLastCommittedURL());
  }
}

}  // namespace autofill_assistant
