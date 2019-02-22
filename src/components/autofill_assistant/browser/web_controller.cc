// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/web_controller.h"

#include <utility>

#include "base/callback.h"
#include "base/logging.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

namespace autofill_assistant {
using autofill::ContentAutofillDriver;

namespace {
const char* const kScrollIntoViewScript =
    "function(node) {\
    const rect = node.getBoundingClientRect();\
    if (rect.height < window.innerHeight) {\
      window.scrollBy({top: rect.top - window.innerHeight * 0.25, \
        behavior: 'smooth'});\
    } else {\
      window.scrollBy({top: rect.top, behavior: 'smooth'});\
    }\
    node.scrollIntoViewIfNeeded();\
  }";

// Javascript to select a value from a select box. Also fires a "change" event
// to trigger any listeners. Changing the index directly does not trigger this.
const char* const kSelectOptionScript =
    R"(function(value) {
      const uppercaseValue = value.toUpperCase();
      var found = false;
      for (var i = 0; i < this.options.length; ++i) {
        const label = this.options[i].label.toUpperCase();
        if (label.length > 0 && label.startsWith(uppercaseValue)) {
          this.options.selectedIndex = i;
          found = true;
          break;
        }
      }
      if (!found) {
        return false;
      }
      const e = document.createEvent('HTMLEvents');
      e.initEvent('change', true, true);
      this.dispatchEvent(e);
      return true;
    })";

// Javascript to highlight an element.
const char* const kHighlightElementScript =
    R"(function() {
      this.style.boxShadow = '0px 0px 0px 3px white, ' +
          '0px 0px 0px 6px rgb(66, 133, 244)';
      return true;
    })";

// Javascript code to retrieve the 'value' attribute of a node.
const char* const kGetValueAttributeScript =
    "function () { return this.value; }";

// Javascript code to set the 'value' attribute of a node.
const char* const kSetValueAttributeScript =
    "function (value) { this.value = value; }";

// Javascript code to get the outerHTML of a node.
// TODO(crbug.com/806868): Investigate if using DOM.GetOuterHtml would be a
// better solution than injecting Javascript code.
const char* const kGetOuterHtmlScript =
    "function () { return this.outerHTML; }";

}  // namespace

// static
std::unique_ptr<WebController> WebController::CreateForWebContents(
    content::WebContents* web_contents) {
  return std::make_unique<WebController>(
      web_contents,
      std::make_unique<DevtoolsClient>(
          content::DevToolsAgentHost::GetOrCreateFor(web_contents)));
}

WebController::WebController(content::WebContents* web_contents,
                             std::unique_ptr<DevtoolsClient> devtools_client)
    : web_contents_(web_contents),
      devtools_client_(std::move(devtools_client)),
      weak_ptr_factory_(this) {}

WebController::~WebController() {}

WebController::FillFormInputData::FillFormInputData() {}

WebController::FillFormInputData::~FillFormInputData() {}

const GURL& WebController::GetUrl() {
  return web_contents_->GetLastCommittedURL();
}

void WebController::LoadURL(const GURL& url) {
  web_contents_->GetController().LoadURLWithParams(
      content::NavigationController::LoadURLParams(url));
}

void WebController::ClickElement(const std::vector<std::string>& selectors,
                                 base::OnceCallback<void(bool)> callback) {
  DCHECK(!selectors.empty());
  FindElement(selectors, base::BindOnce(&WebController::OnFindElementForClick,
                                        weak_ptr_factory_.GetWeakPtr(),
                                        std::move(callback)));
}

void WebController::OnFindElementForClick(
    base::OnceCallback<void(bool)> callback,
    std::unique_ptr<FindElementResult> result) {
  if (result->object_id.empty()) {
    DLOG(ERROR) << "Failed to find the element to click.";
    OnResult(false, std::move(callback));
    return;
  }

  ClickObject(result->object_id, std::move(callback));
}

void WebController::ClickObject(const std::string& object_id,
                                base::OnceCallback<void(bool)> callback) {
  std::vector<std::unique_ptr<runtime::CallArgument>> argument;
  argument.emplace_back(
      runtime::CallArgument::Builder().SetObjectId(object_id).Build());
  devtools_client_->GetRuntime()->Enable();
  devtools_client_->GetRuntime()->CallFunctionOn(
      runtime::CallFunctionOnParams::Builder()
          .SetObjectId(object_id)
          .SetArguments(std::move(argument))
          .SetFunctionDeclaration(std::string(kScrollIntoViewScript))
          .SetReturnByValue(true)
          .Build(),
      base::BindOnce(&WebController::OnScrollIntoView,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     object_id));
}

void WebController::OnScrollIntoView(
    base::OnceCallback<void(bool)> callback,
    std::string object_id,
    std::unique_ptr<runtime::CallFunctionOnResult> result) {
  devtools_client_->GetRuntime()->Disable();
  if (!result || result->HasExceptionDetails()) {
    DLOG(ERROR) << "Failed to scroll the element.";
    OnResult(false, std::move(callback));
    return;
  }

  devtools_client_->GetDOM()->GetBoxModel(
      dom::GetBoxModelParams::Builder().SetObjectId(object_id).Build(),
      base::BindOnce(&WebController::OnGetBoxModel,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WebController::OnGetBoxModel(
    base::OnceCallback<void(bool)> callback,
    std::unique_ptr<dom::GetBoxModelResult> result) {
  if (!result || !result->GetModel() || !result->GetModel()->GetContent()) {
    DLOG(ERROR) << "Failed to get box model.";
    OnResult(false, std::move(callback));
    return;
  }

  // Click at the center of the element.
  const std::vector<double>* content_box = result->GetModel()->GetContent();
  DCHECK_EQ(content_box->size(), 8u);
  double x = ((*content_box)[0] + (*content_box)[2]) * 0.5;
  double y = ((*content_box)[3] + (*content_box)[5]) * 0.5;
  devtools_client_->GetInput()->DispatchMouseEvent(
      input::DispatchMouseEventParams::Builder()
          .SetX(x)
          .SetY(y)
          .SetClickCount(1)
          .SetButton(input::DispatchMouseEventButton::LEFT)
          .SetType(input::DispatchMouseEventType::MOUSE_PRESSED)
          .Build(),
      base::BindOnce(&WebController::OnDispatchPressMoustEvent,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback), x,
                     y));
}

void WebController::OnDispatchPressMoustEvent(
    base::OnceCallback<void(bool)> callback,
    double x,
    double y,
    std::unique_ptr<input::DispatchMouseEventResult> result) {
  devtools_client_->GetInput()->DispatchMouseEvent(
      input::DispatchMouseEventParams::Builder()
          .SetX(x)
          .SetY(y)
          .SetClickCount(1)
          .SetButton(input::DispatchMouseEventButton::LEFT)
          .SetType(input::DispatchMouseEventType::MOUSE_RELEASED)
          .Build(),
      base::BindOnce(&WebController::OnDispatchReleaseMoustEvent,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WebController::OnDispatchReleaseMoustEvent(
    base::OnceCallback<void(bool)> callback,
    std::unique_ptr<input::DispatchMouseEventResult> result) {
  OnResult(true, std::move(callback));
}

void WebController::ElementExists(const std::vector<std::string>& selectors,
                                  base::OnceCallback<void(bool)> callback) {
  DCHECK(!selectors.empty());
  FindElement(selectors, base::BindOnce(&WebController::OnFindElementForExist,
                                        weak_ptr_factory_.GetWeakPtr(),
                                        std::move(callback)));
}

void WebController::OnFindElementForExist(
    base::OnceCallback<void(bool)> callback,
    std::unique_ptr<FindElementResult> result) {
  OnResult(!result->object_id.empty(), std::move(callback));
}

void WebController::FindElement(const std::vector<std::string>& selectors,
                                FindElementCallback callback) {
  devtools_client_->GetDOM()->Enable();
  devtools_client_->GetDOM()->GetDocument(base::BindOnce(
      &WebController::OnGetDocument, weak_ptr_factory_.GetWeakPtr(), selectors,
      std::move(callback)));
}

void WebController::OnGetDocument(
    const std::vector<std::string>& selectors,
    FindElementCallback callback,
    std::unique_ptr<dom::GetDocumentResult> result) {
  std::unique_ptr<FindElementResult> element_result =
      std::make_unique<FindElementResult>();
  element_result->container_frame_host = web_contents_->GetMainFrame();
  element_result->container_frame_selector_index = 0;
  element_result->object_id = "";
  RecursiveFindElement(result->GetRoot()->GetNodeId(), 0, selectors,
                       std::move(element_result), std::move(callback));
}

void WebController::RecursiveFindElement(
    int node_id,
    size_t index,
    const std::vector<std::string>& selectors,
    std::unique_ptr<FindElementResult> element_result,
    FindElementCallback callback) {
  devtools_client_->GetDOM()->QuerySelectorAll(
      node_id, selectors[index],
      base::BindOnce(&WebController::OnQuerySelectorAll,
                     weak_ptr_factory_.GetWeakPtr(), index, selectors,
                     std::move(element_result), std::move(callback)));
}

void WebController::OnQuerySelectorAll(
    size_t index,
    const std::vector<std::string>& selectors,
    std::unique_ptr<FindElementResult> element_result,
    FindElementCallback callback,
    std::unique_ptr<dom::QuerySelectorAllResult> result) {
  if (!result || !result->GetNodeIds() || result->GetNodeIds()->empty()) {
    std::move(callback).Run(std::move(element_result));
    return;
  }

  if (result->GetNodeIds()->size() != 1) {
    DLOG(ERROR) << "Have " << result->GetNodeIds()->size()
                << " elements exist.";
    std::move(callback).Run(std::move(element_result));
    return;
  }

  // Resolve and return object id of the element.
  if (selectors.size() == index + 1) {
    devtools_client_->GetDOM()->ResolveNode(
        dom::ResolveNodeParams::Builder()
            .SetNodeId(result->GetNodeIds()->front())
            .Build(),
        base::BindOnce(&WebController::OnResolveNode,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(element_result), std::move(callback)));
    return;
  }

  devtools_client_->GetDOM()->DescribeNode(
      dom::DescribeNodeParams::Builder()
          .SetNodeId(result->GetNodeIds()->front())
          .Build(),
      base::BindOnce(&WebController::OnDescribeNode,
                     weak_ptr_factory_.GetWeakPtr(),
                     result->GetNodeIds()->front(), index, selectors,
                     std::move(element_result), std::move(callback)));
}

void WebController::OnResolveNode(
    std::unique_ptr<FindElementResult> element_result,
    FindElementCallback callback,
    std::unique_ptr<dom::ResolveNodeResult> result) {
  if (!result || !result->GetObject() ||
      result->GetObject()->GetObjectId().empty()) {
    DLOG(ERROR) << "Failed to resolve object id from node id.";
    std::move(callback).Run(std::move(element_result));
    return;
  }

  element_result->object_id = result->GetObject()->GetObjectId();
  std::move(callback).Run(std::move(element_result));
}

void WebController::OnDescribeNode(
    int node_id,
    size_t index,
    const std::vector<std::string>& selectors,
    std::unique_ptr<FindElementResult> element_result,
    FindElementCallback callback,
    std::unique_ptr<dom::DescribeNodeResult> result) {
  if (!result || !result->GetNode()) {
    DLOG(ERROR) << "Failed to describe the node.";
    std::move(callback).Run(std::move(element_result));
    return;
  }

  auto* node = result->GetNode();
  std::vector<int> backend_ids;
  if (node->HasContentDocument()) {
    backend_ids.emplace_back(node->GetContentDocument()->GetBackendNodeId());

    element_result->container_frame_selector_index = index;

    // Find out the corresponding render frame host through document url and
    // name.
    // TODO(crbug.com/806868): Use more attributes to find out the render frame
    // host if name and document url are not enough to uniquely identify it.
    std::string frame_name;
    if (node->HasAttributes()) {
      const std::vector<std::string>* attributes = node->GetAttributes();
      for (size_t i = 0; i < attributes->size();) {
        if ((*attributes)[i] == "name") {
          frame_name = (*attributes)[i + 1];
          break;
        }
        // Jump two positions since attribute name and value are always paired.
        i = i + 2;
      }
    }
    element_result->container_frame_host = FindCorrespondingRenderFrameHost(
        frame_name, node->GetContentDocument()->GetDocumentURL());
    if (!element_result->container_frame_host) {
      DLOG(ERROR) << "Failed to find corresponding owner frame.";
      std::move(callback).Run(std::move(element_result));
      return;
    }
  } else if (node->HasFrameId()) {
    // TODO(crbug.com/806868): Support out-of-process iframe.
    DLOG(WARNING) << "The element is inside an OOPIF.";
    std::move(callback).Run(std::move(element_result));
    return;
  }

  if (node->HasShadowRoots()) {
    // TODO(crbug.com/806868): Support multiple shadow roots.
    backend_ids.emplace_back(
        node->GetShadowRoots()->front()->GetBackendNodeId());
  }

  if (!backend_ids.empty()) {
    devtools_client_->GetDOM()
        ->GetExperimental()
        ->PushNodesByBackendIdsToFrontend(
            dom::PushNodesByBackendIdsToFrontendParams::Builder()
                .SetBackendNodeIds(backend_ids)
                .Build(),
            base::BindOnce(&WebController::OnPushNodesByBackendIds,
                           weak_ptr_factory_.GetWeakPtr(), index, selectors,
                           std::move(element_result), std::move(callback)));
    return;
  }

  RecursiveFindElement(node_id, ++index, selectors, std::move(element_result),
                       std::move(callback));
}

content::RenderFrameHost* WebController::FindCorrespondingRenderFrameHost(
    std::string name,
    std::string document_url) {
  content::RenderFrameHost* ret_frame = nullptr;
  for (auto* frame : web_contents_->GetAllFrames()) {
    if (frame->GetFrameName() == name &&
        frame->GetLastCommittedURL().spec() == document_url) {
      DCHECK(!ret_frame);
      ret_frame = frame;
    }
  }

  return ret_frame;
}

void WebController::OnPushNodesByBackendIds(
    size_t index,
    const std::vector<std::string>& selectors,
    std::unique_ptr<FindElementResult> element_result,
    FindElementCallback callback,
    std::unique_ptr<dom::PushNodesByBackendIdsToFrontendResult> result) {
  DCHECK(result->GetNodeIds()->size() == 1);
  RecursiveFindElement(result->GetNodeIds()->front(), ++index, selectors,
                       std::move(element_result), std::move(callback));
}

void WebController::OnResult(bool result,
                             base::OnceCallback<void(bool)> callback) {
  devtools_client_->GetDOM()->Disable();
  std::move(callback).Run(result);
}

void WebController::OnResult(
    bool exists,
    const std::string& value,
    base::OnceCallback<void(bool, const std::string&)> callback) {
  devtools_client_->GetDOM()->Disable();
  std::move(callback).Run(exists, value);
}

void WebController::OnFindElementForFocusElement(
    base::OnceCallback<void(bool)> callback,
    std::unique_ptr<FindElementResult> element_result) {
  if (element_result->object_id.empty()) {
    DLOG(ERROR) << "Failed to find the element to focus on.";
    OnResult(false, std::move(callback));
    return;
  }

  std::vector<std::unique_ptr<runtime::CallArgument>> argument;
  argument.emplace_back(runtime::CallArgument::Builder()
                            .SetObjectId(element_result->object_id)
                            .Build());
  devtools_client_->GetRuntime()->Enable();
  devtools_client_->GetRuntime()->CallFunctionOn(
      runtime::CallFunctionOnParams::Builder()
          .SetObjectId(element_result->object_id)
          .SetArguments(std::move(argument))
          .SetFunctionDeclaration(std::string(kScrollIntoViewScript))
          .SetReturnByValue(true)
          .Build(),
      base::BindOnce(&WebController::OnFocusElement,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WebController::OnFocusElement(
    base::OnceCallback<void(bool)> callback,
    std::unique_ptr<runtime::CallFunctionOnResult> result) {
  devtools_client_->GetRuntime()->Disable();
  if (!result || result->HasExceptionDetails()) {
    DLOG(ERROR) << "Failed to focus on element.";
    OnResult(false, std::move(callback));
    return;
  }
  OnResult(true, std::move(callback));
}

void WebController::FillAddressForm(const std::string& guid,
                                    const std::vector<std::string>& selectors,
                                    base::OnceCallback<void(bool)> callback) {
  auto data_to_autofill = std::make_unique<FillFormInputData>();
  data_to_autofill->autofill_data_guid = guid;
  FindElement(selectors,
              base::BindOnce(&WebController::OnFindElementForFillingForm,
                             weak_ptr_factory_.GetWeakPtr(),
                             std::move(data_to_autofill), selectors,
                             std::move(callback)));
}

void WebController::OnFindElementForFillingForm(
    std::unique_ptr<FillFormInputData> data_to_autofill,
    const std::vector<std::string>& selectors,
    base::OnceCallback<void(bool)> callback,
    std::unique_ptr<FindElementResult> element_result) {
  if (element_result->object_id.empty()) {
    DLOG(ERROR) << "Failed to find the element for filling the form.";
    OnResult(false, std::move(callback));
    return;
  }

  ContentAutofillDriver* driver = ContentAutofillDriver::GetForRenderFrameHost(
      element_result->container_frame_host);
  DCHECK(!selectors.empty());
  // TODO(crbug.com/806868): Figure out whether there are cases where we need
  // more than one selector, and come up with a solution that can figure out the
  // right number of selectors to include.
  driver->GetAutofillAgent()->GetElementFormAndFieldData(
      std::vector<std::string>(1, selectors.back()),
      base::BindOnce(&WebController::OnGetFormAndFieldDataForFillingForm,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(data_to_autofill), std::move(callback),
                     element_result->container_frame_host));
}

void WebController::OnGetFormAndFieldDataForFillingForm(
    std::unique_ptr<FillFormInputData> data_to_autofill,
    base::OnceCallback<void(bool)> callback,
    content::RenderFrameHost* container_frame_host,
    const autofill::FormData& form_data,
    const autofill::FormFieldData& form_field) {
  if (form_data.fields.empty()) {
    DLOG(ERROR) << "Failed to get form data to fill form.";
    OnResult(false, std::move(callback));
    return;
  }

  ContentAutofillDriver* driver =
      ContentAutofillDriver::GetForRenderFrameHost(container_frame_host);
  if (!driver) {
    DLOG(ERROR) << "Failed to get the autofill driver.";
    OnResult(false, std::move(callback));
    return;
  }

  if (data_to_autofill->card) {
    driver->autofill_manager()->FillCreditCardForm(
        autofill::kNoQueryId, form_data, form_field, *data_to_autofill->card,
        data_to_autofill->cvc);
  } else {
    driver->autofill_manager()->FillProfileForm(
        data_to_autofill->autofill_data_guid, form_data, form_field);
  }

  OnResult(true, std::move(callback));
}

void WebController::FillCardForm(std::unique_ptr<autofill::CreditCard> card,
                                 const base::string16& cvc,
                                 const std::vector<std::string>& selectors,
                                 base::OnceCallback<void(bool)> callback) {
  auto data_to_autofill = std::make_unique<FillFormInputData>();
  data_to_autofill->card = std::move(card);
  data_to_autofill->cvc = cvc;
  FindElement(selectors,
              base::BindOnce(&WebController::OnFindElementForFillingForm,
                             weak_ptr_factory_.GetWeakPtr(),
                             std::move(data_to_autofill), selectors,
                             std::move(callback)));
}

void WebController::SelectOption(const std::vector<std::string>& selectors,
                                 const std::string& selected_option,
                                 base::OnceCallback<void(bool)> callback) {
  FindElement(selectors,
              base::BindOnce(&WebController::OnFindElementForSelectOption,
                             weak_ptr_factory_.GetWeakPtr(), selected_option,
                             std::move(callback)));
}

void WebController::OnFindElementForSelectOption(
    const std::string& selected_option,
    base::OnceCallback<void(bool)> callback,
    std::unique_ptr<FindElementResult> element_result) {
  const std::string object_id = element_result->object_id;
  if (object_id.empty()) {
    DLOG(ERROR) << "Failed to find the element to select an option.";
    OnResult(false, std::move(callback));
    return;
  }

  std::vector<std::unique_ptr<runtime::CallArgument>> argument;
  argument.emplace_back(
      runtime::CallArgument::Builder()
          .SetValue(base::Value::ToUniquePtrValue(base::Value(selected_option)))
          .Build());
  devtools_client_->GetRuntime()->Enable();
  devtools_client_->GetRuntime()->CallFunctionOn(
      runtime::CallFunctionOnParams::Builder()
          .SetObjectId(object_id)
          .SetArguments(std::move(argument))
          .SetFunctionDeclaration(std::string(kSelectOptionScript))
          .SetReturnByValue(true)
          .Build(),
      base::BindOnce(&WebController::OnSelectOption,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WebController::OnSelectOption(
    base::OnceCallback<void(bool)> callback,
    std::unique_ptr<runtime::CallFunctionOnResult> result) {
  devtools_client_->GetRuntime()->Disable();
  if (!result || result->HasExceptionDetails()) {
    DLOG(ERROR) << "Failed to select option.";
    OnResult(false, std::move(callback));
    return;
  }

  // Read the result returned from Javascript code.
  DCHECK(result->GetResult()->GetValue()->is_bool());
  OnResult(result->GetResult()->GetValue()->GetBool(), std::move(callback));
}

void WebController::HighlightElement(const std::vector<std::string>& selectors,
                                     base::OnceCallback<void(bool)> callback) {
  FindElement(
      selectors,
      base::BindOnce(&WebController::OnFindElementForHighlightElement,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WebController::OnFindElementForHighlightElement(
    base::OnceCallback<void(bool)> callback,
    std::unique_ptr<FindElementResult> element_result) {
  const std::string object_id = element_result->object_id;
  if (object_id.empty()) {
    DLOG(ERROR) << "Failed to find the element to highlight.";
    OnResult(false, std::move(callback));
    return;
  }

  std::vector<std::unique_ptr<runtime::CallArgument>> argument;
  argument.emplace_back(
      runtime::CallArgument::Builder().SetObjectId(object_id).Build());
  devtools_client_->GetRuntime()->Enable();
  devtools_client_->GetRuntime()->CallFunctionOn(
      runtime::CallFunctionOnParams::Builder()
          .SetObjectId(object_id)
          .SetArguments(std::move(argument))
          .SetFunctionDeclaration(std::string(kHighlightElementScript))
          .SetReturnByValue(true)
          .Build(),
      base::BindOnce(&WebController::OnHighlightElement,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WebController::OnHighlightElement(
    base::OnceCallback<void(bool)> callback,
    std::unique_ptr<runtime::CallFunctionOnResult> result) {
  devtools_client_->GetRuntime()->Disable();
  if (!result || result->HasExceptionDetails()) {
    DLOG(ERROR) << "Failed to highlight element.";
    OnResult(false, std::move(callback));
    return;
  }
  // Read the result returned from Javascript code.
  DCHECK(result->GetResult()->GetValue()->is_bool());
  OnResult(result->GetResult()->GetValue()->GetBool(), std::move(callback));
}

void WebController::FocusElement(const std::vector<std::string>& selectors,
                                 base::OnceCallback<void(bool)> callback) {
  DCHECK(!selectors.empty());
  FindElement(
      selectors,
      base::BindOnce(&WebController::OnFindElementForFocusElement,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WebController::GetFieldValue(
    const std::vector<std::string>& selectors,
    base::OnceCallback<void(bool, const std::string&)> callback) {
  FindElement(
      selectors,
      base::BindOnce(&WebController::OnFindElementForGetFieldValue,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WebController::OnFindElementForGetFieldValue(
    base::OnceCallback<void(bool, const std::string&)> callback,
    std::unique_ptr<FindElementResult> element_result) {
  const std::string object_id = element_result->object_id;
  if (object_id.empty()) {
    OnResult(/* exists= */ false, "", std::move(callback));
    return;
  }

  devtools_client_->GetRuntime()->Enable();
  devtools_client_->GetRuntime()->CallFunctionOn(
      runtime::CallFunctionOnParams::Builder()
          .SetObjectId(object_id)
          .SetFunctionDeclaration(std::string(kGetValueAttributeScript))
          .SetReturnByValue(true)
          .Build(),
      base::BindOnce(&WebController::OnGetValueAttribute,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WebController::OnGetValueAttribute(
    base::OnceCallback<void(bool, const std::string&)> callback,
    std::unique_ptr<runtime::CallFunctionOnResult> result) {
  devtools_client_->GetRuntime()->Disable();
  if (!result || result->HasExceptionDetails()) {
    OnResult(/* exists= */ true, "", std::move(callback));
    return;
  }

  // Read the result returned from Javascript code.
  DCHECK(result->GetResult()->GetValue()->is_string());
  OnResult(/* exists= */ true, result->GetResult()->GetValue()->GetString(),
           std::move(callback));
}

void WebController::SetFieldValue(const std::vector<std::string>& selectors,
                                  const std::string& value,
                                  base::OnceCallback<void(bool)> callback) {
  FindElement(selectors,
              base::BindOnce(&WebController::OnFindElementForSetFieldValue,
                             weak_ptr_factory_.GetWeakPtr(), value,
                             std::move(callback)));
}

void WebController::OnFindElementForSetFieldValue(
    const std::string& value,
    base::OnceCallback<void(bool)> callback,
    std::unique_ptr<FindElementResult> element_result) {
  const std::string object_id = element_result->object_id;
  if (object_id.empty()) {
    OnResult(false, std::move(callback));
    return;
  }

  std::vector<std::unique_ptr<runtime::CallArgument>> argument;
  argument.emplace_back(
      runtime::CallArgument::Builder()
          .SetValue(base::Value::ToUniquePtrValue(base::Value(value)))
          .Build());
  devtools_client_->GetRuntime()->Enable();
  devtools_client_->GetRuntime()->CallFunctionOn(
      runtime::CallFunctionOnParams::Builder()
          .SetObjectId(object_id)
          .SetArguments(std::move(argument))
          .SetFunctionDeclaration(std::string(kSetValueAttributeScript))
          .Build(),
      base::BindOnce(&WebController::OnSetValueAttribute,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WebController::OnSetValueAttribute(
    base::OnceCallback<void(bool)> callback,
    std::unique_ptr<runtime::CallFunctionOnResult> result) {
  devtools_client_->GetRuntime()->Disable();
  OnResult(result && !result->HasExceptionDetails(), std::move(callback));
}

void WebController::GetOuterHtml(
    const std::vector<std::string>& selectors,
    base::OnceCallback<void(bool, const std::string&)> callback) {
  FindElement(
      selectors,
      base::BindOnce(&WebController::OnFindElementForGetOuterHtml,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WebController::OnFindElementForGetOuterHtml(
    base::OnceCallback<void(bool, const std::string&)> callback,
    std::unique_ptr<FindElementResult> element_result) {
  const std::string object_id = element_result->object_id;
  if (object_id.empty()) {
    OnResult(false, "", std::move(callback));
    return;
  }

  devtools_client_->GetRuntime()->Enable();
  devtools_client_->GetRuntime()->CallFunctionOn(
      runtime::CallFunctionOnParams::Builder()
          .SetObjectId(object_id)
          .SetFunctionDeclaration(std::string(kGetOuterHtmlScript))
          .SetReturnByValue(true)
          .Build(),
      base::BindOnce(&WebController::OnGetOuterHtml,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WebController::OnGetOuterHtml(
    base::OnceCallback<void(bool, const std::string&)> callback,
    std::unique_ptr<runtime::CallFunctionOnResult> result) {
  devtools_client_->GetRuntime()->Disable();
  if (!result || result->HasExceptionDetails()) {
    OnResult(false, "", std::move(callback));
    return;
  }

  // Read the result returned from Javascript code.
  DCHECK(result->GetResult()->GetValue()->is_string());
  OnResult(true, result->GetResult()->GetValue()->GetString(),
           std::move(callback));
}

}  // namespace autofill_assistant
