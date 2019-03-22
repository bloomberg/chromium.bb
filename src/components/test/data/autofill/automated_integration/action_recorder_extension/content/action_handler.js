// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

(function() {
function buildXPathForElement(target) {
  function build(target) {
    let node = target;
    let descendentXPath = '';

    while (node !== null) {
      if (node.nodeType === Node.DOCUMENT_NODE) {
        return descendentXPath;
      }

      let result = prependParentNodeToXPath(node, descendentXPath);
      if (result.isUnique) {
        return result.xPath;
      }

      descendentXPath = result.xPath;
      node = node.parentNode;
    }

    throw (
        'Failed to build a unique XPath! The target node isn\'t ' +
        'attached to the document!');
  }

  function prependParentNodeToXPath(node, xPath) {
    // First, test if simply constructing an XPath using the element's
    // local name results in a unique XPath.
    let nodeXPath = buildXPathForSingleNode(node);
    let testXPath = `//${nodeXPath}${xPath}`;
    let numMatches = countNumberOfMatches(testXPath);
    if (numMatches === 1) {
      return {isUnique: true, xPath: testXPath};
    }

    // Build a list of potential classifiers using
    // * The element's explicit attributes.
    // * The element's text content, if the element contains text.
    let classifiers = [];
    let attributes = [];
    let attrRawValues = [];
    // Populate the attribute list with the element's explicit attributes.
    for (let index = 0; index < node.attributes.length; index++) {
      const attr = node.attributes[index].name;
      if (attr === 'style') {
        // Skip styles. 'style' is mutable.
        continue;
      } else if (attr === 'class') {
        // Disabled. Class list is simply too mutable, especially in the
        // case where an element changes class on hover or on focus.
        continue;
      } else if (
          attr === 'autofill-prediction' || attr === 'field_signature' ||
          attr === 'pm_parser_annotation' || attr === 'title') {
        // These attributes are inserted by Chrome.
        // Since Chrome sets these attributes, these attributes may change
        // from build to build. Skip these attributes.
        continue;
      } else {
        attributes.push(`@${attr}`);
        attrRawValues.push(node.attributes[index].value);
      }
    }
    // Add the element's text content to the attribute list.
    switch (node.localName) {
      case 'a':
      case 'span':
      case 'button':
        attributes.push('text()');
        attrRawValues.push(node.textContent);
        break;
      default:
    }

    // Iterate through each attribute.
    for (let index = 0; index < attributes.length; index++) {
      let classifier =
          buildClassifier(node, attributes[index], attrRawValues[index]);
      // Add the classifier and see if adding it generates a unique XPath.
      let numMatchesBefore = numMatches;
      classifiers.push(classifier);
      nodeXPath = buildXPathForSingleNode(node, classifiers);
      testXPath = `//${nodeXPath}${xPath}`;
      numMatches = countNumberOfMatches(testXPath);

      if (numMatches === 0) {
        // The classifier is faulty, log an error and remove the classifier.
        console.warn('Encountered faulty classifier: ' + classifiers.pop());
      } else if (numMatches === 1) {
        // The current XPath is unique, exit.
        return {isUnique: true, xPath: testXPath};
      } else if (numMatches === numMatchesBefore) {
        // Adding this classifier to the XPath does not narrow down the
        // number of elements this XPath matches. Therefore the XPath does not
        // need this classifier. Remove the classifier.
        classifiers.pop();
      }
    }

    // A XPath with the current node as the root is not unique.
    // Check if the node has siblings with the same XPath. If so,
    // add a child node index to the current node's XPath.
    const queryResult = document.evaluate(
        nodeXPath, node.parentNode, null,
        XPathResult.ORDERED_NODE_SNAPSHOT_TYPE, null);

    if (queryResult.snapshotLength === 1) {
      return {isUnique: false, xPath: `/${nodeXPath}${xPath}`};
    } else {
      // The current node has siblings with the same XPath. Add an index
      // to the current node's XPath.
      for (let index = 0; index < queryResult.snapshotLength; index++) {
        if (queryResult.snapshotItem(index) === node) {
          let testXPath = `/${nodeXPath}[${(index + 1)}]${xPath}`;
          return {isUnique: false, xPath: testXPath};
        }
      }

      throw ('Assert: unable to find the target node!');
    }
  }

  function buildClassifier(element, attributeXPathToken, attributeValue) {
    if (!attributeValue ||
        // Skip values that has the " character. There is just no good way to
        // wrap this character in xslt, and the best way to handle it
        // involves doing gynmnastics with string concatenation, as in
        // "concat('Single', "'", 'quote. Double', '"', 'quote.')]";
        // It is easier to just skip attributes containing the '"' character.
        // Furthermore, attributes containing the '"' character are very rare.
        attributeValue.indexOf('"') >= 0 ||
        // Skip values that have the \ character. JavaScript does not
        // serialize this character correctly.
        attributeValue.indexOf('\\') >= 0) {
      return attributeXPathToken;
    }

    // Sometimes, the attribute is a piece of code containing spaces,
    // carriage returns or tabs. A XPath constructed using the raw value
    // will not work. Normalize the value instead.
    const queryResult = document.evaluate(
        `normalize-space(${attributeXPathToken})`, element, null,
        XPathResult.STRING_TYPE, null);
    if (queryResult.stringValue === attributeValue) {
      // There is no difference between the raw value and the normalized
      // value, return a shorter XPath with the raw value.
      return `${attributeXPathToken}="${attributeValue}"`;
    } else {
      return `normalize-space(${attributeXPathToken})=` +
          `"${queryResult.stringValue}"`;
    }
  }

  function buildXPathForSingleNode(element, classifiers = []) {
    let xPathSelector = element.localName;
    // Add the classifiers
    if (classifiers.length > 0) {
      xPathSelector += `[${classifiers[0]}`;
      for (let index = 1; index < classifiers.length; index++) {
        xPathSelector += ` and ${classifiers[index]}`;
      }
      xPathSelector += ']';
    }
    return xPathSelector;
  }

  function countNumberOfMatches(xPath) {
    const queryResult = document.evaluate(
        `count(${xPath})`, document, null, XPathResult.NUMBER_TYPE, null);
    return queryResult.numberValue;
  }

  return build(target);
}

let autofillTriggerElementInfo = null;
let lastTypingEventInfo = null;
let frameContext;
let mutationObserver = null;
let started = false;

function resetAutofillTriggerElement() {
  autofillTriggerElementInfo = null;
}

function isEditableInputElement(element) {
  return (element.localName === 'select') ||
      (element.localName === 'textarea') || canTriggerAutofill(element);
}

function isPasswordInputElement(element) {
  return element.getAttribute('type') === 'password';
}

function isChromeRecognizedPasswordField(element) {
  const passwordManagerParserAnnotation =
      element.getAttribute('pm_parser_annotation');
  return passwordManagerParserAnnotation === 'password_element' ||
      passwordManagerParserAnnotation === 'new_password_element' ||
      passwordManagerParserAnnotation === 'confirmation_password_element';
}

function isChromeRecognizedUserNameField(element) {
  return element.getAttribute('pm_parser_annotation') === 'username_element';
}

function canTriggerAutofill(element) {
  return (element.localName === 'input' && [
    'checkbox', 'radio', 'button', 'submit', 'hidden', 'reset'
  ].indexOf(element.getAttribute('type')) === -1);
}

async function extractAndSendChromePasswordManagerProfile(passwordField) {
  // Extract the user name field.
  const form = passwordField.form;
  const usernameField = form.querySelector(
      `*[form_signature][pm_parser_annotation='username_element']`);
  if (!usernameField) {
    console.warn('Failed to detect the user name field!');
    return;
  }

  const autofilledPasswordManagerProfile = {
    username: usernameField.value,
    password: passwordField.value,
    website: window.location.origin
  };
  return await sendRuntimeMessageToBackgroundScript({
    type: RecorderMsgEnum.SET_PASSWORD_MANAGER_PROFILE_ENTRY,
    entry: autofilledPasswordManagerProfile
  });
}

async function sendChromeAutofillProfileEntry(field) {
  const entry = {
    type: field.getAttribute('autofill-prediction'),
    value: field.value
  };
  return await sendRuntimeMessageToBackgroundScript(
      {type: RecorderMsgEnum.SET_AUTOFILL_PROFILE_ENTRY, entry: entry});
}

async function addActionToRecipe(action) {
  return await sendRuntimeMessageToBackgroundScript(
      {type: RecorderMsgEnum.ADD_ACTION, action: action});
}

async function onUserMakingSelectionChange(element) {
  const selector = buildXPathForElement(element);
  const elementReadyState = automation_helper.getElementState(element);
  const index = element.options.selectedIndex;

  console.log(`Select detected on: ${selector} with '${index}'`);
  const action = {
    context: frameContext,
    index: index,
    selector: selector,
    type: ActionTypeEnum.SELECT,
    visibility: elementReadyState
  };
  await addActionToRecipe(action);
}

async function onUserFinishingTypingInput(element) {
  const selector = buildXPathForElement(element);
  const elementReadyState = automation_helper.getElementState(element);

  console.log(`Typing detected on: ${selector}`);
  // Distinguish between typing inside password input fields and
  // other type of text input fields.
  //
  // This extension generates test recipes to be consumed by the Captured
  // Sites Automation Framework. The automation framework replays a typing
  // action by using JavaScript to set the value of a text input field.
  //
  // However, to trigger the Chrome Password Manager, the automation
  // framework must simulate user typing inside the password field by
  // sending individual character keyboard input - because Chrome Password
  // Manager deliberately ignores forms filled by JavaScript.
  //
  // Simulating keyboard input is a less reliable and therefore the less
  // preferred way for filling text inputs. The Automation Framework uses
  // keyboard input only when necessary. So this extension separates
  // typing password actions from other typing actions.
  const isPasswordField = isPasswordInputElement(event.target);

  const action = {
    context: frameContext,
    selector: selector,
    type: isPasswordField ? ActionTypeEnum.TYPE_PASSWORD : ActionTypeEnum.TYPE,
    value: element.value,
    visibility: elementReadyState
  };

  await addActionToRecipe(action);
}

async function onUserInvokingAutofill() {
  console.log(`Triggered autofill on ${autofillTriggerElementInfo.selector}`);
  const autofillAction = {
    selector: autofillTriggerElementInfo.selector,
    context: frameContext,
    type: ActionTypeEnum.AUTOFILL,
    visibility: autofillTriggerElementInfo.visibility
  };
  resetAutofillTriggerElement();
  await addActionToRecipe(autofillAction);
}

async function onChromeAutofillingNonPasswordInput(element) {
  const selector = buildXPathForElement(element);
  const elementReadyState = automation_helper.getElementState(element);
  const value = element.value;
  const autofillType = element.getAttribute('autofill-prediction');

  console.log(`Autofill detected on: ${selector} with value '${value}'`);
  let action = {
    context: frameContext,
    expectedValue: value,
    selector: selector,
    type: ActionTypeEnum.VALIDATE_FIELD,
    visibility: elementReadyState
  };

  if (autofillType) {
    action.expectedAutofillType = autofillType;
  }
  await addActionToRecipe(action);

  if (autofillType) {
    await sendChromeAutofillProfileEntry(element);
  }
}

async function onChromeAutofillingPasswordInput(element) {
  const elementReadyState = automation_helper.getElementState(element);
  const selector = buildXPathForElement(element);
  const value = element.value;

  console.log(`Autofill detected on: ${selector} with value '${value}'`);
  let validateAction = {
    selector: selector,
    context: frameContext,
    expectedValue: value,
    type: ActionTypeEnum.VALIDATE_FIELD,
    visibility: elementReadyState
  };
  await addActionToRecipe(validateAction);

  await extractAndSendChromePasswordManagerProfile(element);
}

async function onInputChangeActionHandler(event) {
  if (event.target.autofilledByChrome &&
      isChromeRecognizedPasswordField(event.target)) {
    await onChromeAutofillingPasswordInput(event.target);
  } else if (event.target.autofilledByChrome) {
    // If the user has previously clicked on a field that can trigger
    // autofill, add a trigger autofill action.
    if (autofillTriggerElementInfo !== null) {
      await onUserInvokingAutofill();
    }
    await onChromeAutofillingNonPasswordInput(event.target);
  } else if (event.target.localName === 'select') {
    await onUserMakingSelectionChange(event.target);
  } else if (
      lastTypingEventInfo && lastTypingEventInfo.target === event.target &&
      lastTypingEventInfo.value === event.target.value) {
    await onUserFinishingTypingInput(event.target);
  }
}

function registerOnInputChangeActionListener(root) {
  const inputElements = root.querySelectorAll('input, select, textarea');
  inputElements.forEach((element) => {
    if (isEditableInputElement(element)) {
      element.addEventListener('change', onInputChangeActionHandler, true);
      element.addEventListener('animationstart', onAnimationStartHandler, true);
    }
  });
}

function unRegisterOnInputChangeActionListener(root) {
  const inputElements = root.querySelectorAll('input, select, textarea');
  inputElements.forEach((element) => {
    if (isEditableInputElement(element)) {
      element.removeEventListener('change', onInputChangeActionHandler, true);
      element.removeEventListener(
          'animationstart', onAnimationStartHandler, true);
    }
  });
}

// If the user clicks on an element that can trigger autofill, save the
// element's xpath. If the extension detects Chrome autofill events later,
// the extension will designate this element as the autofill trigger
// element.
function onClickingAutofillableElement(element) {
  const selector = buildXPathForElement(event.target);
  const elementReadyState = automation_helper.getElementState(event.target);
  autofillTriggerElementInfo = {
    selector: selector,
    visibility: elementReadyState
  };
}

async function onLeftMouseClickingPageElement(element) {
  // Reset the autofill trigger element.
  resetAutofillTriggerElement();

  // Do not record left mouse clicks on editable inputs.
  // These clicks should always precede either a typing action, or an
  // autofill action. The extension will record typing actions and
  // autofill actions separately.
  if (isEditableInputElement(element)) {
    if (canTriggerAutofill(element)) {
      onClickingAutofillableElement(element);
    }
    return;
  }

  // Ignore left mouse clicks on the html element. A page fires an event
  // with the entire html element as the target when the user clicks on
  // Chrome's side scroll bar.
  if (event.target.localName === 'html') return;

  const elementReadyState = automation_helper.getElementState(element);
  const selector = buildXPathForElement(element);

  console.log(`Left-click detected on: ${selector}`);
  await addActionToRecipe({
    selector: selector,
    visibility: elementReadyState,
    context: frameContext,
    type: ActionTypeEnum.CLICK
  });
}

async function onRightMouseClickingPageElement(element) {
  const selector = buildXPathForElement(element);
  const elementReadyState = automation_helper.getElementState(element);

  console.log(`Right-click detected on: ${selector}`);
  await addActionToRecipe({
    selector: selector,
    visibility: elementReadyState,
    context: frameContext,
    type: ActionTypeEnum.HOVER
  });
}

async function onClickActionHander(event) {
  if (event.button === Buttons.LEFT_BUTTON) {
    await onLeftMouseClickingPageElement(event.target);
  } else if (event.button === Buttons.RIGHT_BUTTON) {
    await onRightMouseClickingPageElement(event.target);
  }
}

async function onEnterKeyUp(element) {
  const elementReadyState = automation_helper.getElementState(element);
  const selector = buildXPathForElement(element);

  console.log(`Enter detected on: ${selector}'`);
  await addActionToRecipe({
    selector: selector,
    visibility: elementReadyState,
    context: frameContext,
    type: ActionTypeEnum.PRESS_ENTER
  });
}

async function onKeyUpActionHandler(event) {
  if (event.key === 'Enter') {
    return await onEnterKeyUp(event.target);
  }

  if (isEditableInputElement(event.target)) {
    lastTypingEventInfo = { target: event.target, value: event.target.value };
  } else {
    lastTypingEventInfo = null;
  }
}

async function onPasswordFormSubmitHandler(event) {
  const form = event.target;

  // Extract the form signature value from the form.
  const fields =
      form.querySelectorAll(`*[form_signature][pm_parser_annotation]`);
  let username = null;
  let password = null;
  for (const field of fields) {
    const passwordManagerAnnotation =
        field.getAttribute('pm_parser_annotation');
    switch (passwordManagerAnnotation) {
      case 'password_element':
      case 'new_password_element':
      case 'confirmation_password_element':
        password = field.value;
        break;
      case 'username_element':
        username = field.value;
        break;
      default:
    }
  }

  if (!username || !password) {
    // The form is missing a user name field or a password field.
    // The content script should not forward an incomplete password form to
    // the recorder extension. Exit.
    return;
  }

  await sendRuntimeMessageToBackgroundScript({
    type: RecorderMsgEnum.SET_PASSWORD_MANAGER_ACTION_PARAMS,
    params: {
      username: username,
      password: password,
      website: window.location.origin
    }
  });
}

function registerOnPasswordFormSubmitHandler(root) {
  const formElements = root.querySelectorAll('form');
  formElements.forEach((form) => {
    form.addEventListener('submit', onPasswordFormSubmitHandler, true);
  });
}

function unRegisterOnPasswordFormSubmitHandler(root) {
  const formElements = root.querySelectorAll('form');
  formElements.forEach((form) => {
    form.removeEventListener('submit', onPasswordFormSubmitHandler, true);
  });
}

function addCssStyleToTriggerAutofillEvents() {
  let style = document.createElement('style');
  style.type = 'text/css';
  const css = `@keyframes onAutoFillStart {
                   from {/**/}  to {/**/}
                 }
                 @keyframes onAutoFillCancel {
                   from {/**/}  to {/**/}
                 }

                 input:-webkit-autofill,
                 select:-webkit-autofill,
                 textarea:-webkit-autofill {
                   animation-name: onAutoFillStart;
                 }

                 input:not(:-webkit-autofill),
                 select:not(:-webkit-autofill),
                 textarea:not(:-webkit-autofill) {
                   animation-name: onAutoFillCancel;
                 }`;
  style.appendChild(document.createTextNode(css));
  document.getElementsByTagName('head')[0].appendChild(style);
}

function onAnimationStartHandler(event) {
  switch (event.animationName) {
    case 'onAutoFillStart':
      event.target.autofilledByChrome = true;
      break;
    case 'onAutoFillCancel':
      event.target.autofilledByChrome = false;
      break;
    default:
  }
}

function startRecording(context) {
  frameContext = context;
  // Add a css rule to allow the extension to detect when Chrome
  // autofills password input fields.
  addCssStyleToTriggerAutofillEvents();
  // Register on change listeners on all the input elements.
  registerOnInputChangeActionListener(document);
  registerOnPasswordFormSubmitHandler(document);
  // Register a mouse up listener on the entire document.
  //
  // The content script registers a 'Mouse Up' listener rather than a
  // 'Mouse Down' to correctly handle the following scenario:
  //
  // A user types inside a search box, then clicks the search button.
  //
  // The following events will fire in quick chronological succession:
  // * Mouse down on the search button.
  // * Change on the search input box.
  // * Mouse up on the search button.
  //
  // To capture the correct sequence of actions, the content script
  // should tie left mouse click actions to the mouseup event.
  document.addEventListener('mouseup', onClickActionHander);
  // Register a key press listener on the entire document.
  document.addEventListener('keyup', onKeyUpActionHandler);
  // Setup mutation observer to listen for event on nodes added after
  // recording starts.
  mutationObserver = new MutationObserver((mutations) => {
    mutations.forEach(mutation => {
      mutation.addedNodes.forEach(node => {
        if (node.nodeType === Node.ELEMENT_NODE) {
          // Add the onchange listener on any new input elements. This
          // way the recorder can record user interactions with new
          // elements.
          registerOnInputChangeActionListener(node);
          // Add the on password form submission listener on any new
          // form elements.
          registerOnPasswordFormSubmitHandler(node);
        }
      });
    });
  });
  mutationObserver.observe(document, {childList: true, subtree: true});
  started = true;
  return Promise.resolve();
}

function stopRecording() {
  if (started) {
    mutationObserver.disconnect();
    document.removeEventListener('mousedown', onClickActionHander);
    document.removeEventListener('keyup', onKeyUpActionHandler);
    unRegisterOnInputChangeActionListener(document);
    unRegisterOnPasswordFormSubmitHandler(document);
  }
}

function queryIframeName(iframeUrl) {
  let iframe = null;
  const frameLocation = new URL(iframeUrl);
  const iframes = document.querySelectorAll('iframe');
  // Find the target iframe.
  for (let index = 0; index < iframes.length; index++) {
    const url = new URL(iframes[index].src, location.origin);
    // Try to identify the iframe using the entire URL.
    if (frameLocation.href === url.href) {
      iframe = iframes[index];
    }
    // On rare occasions, the hash in an iframe's URL may not match with the
    // hash in the iframe element's src attribute. For example, the iframe
    // document reports that the iframe url is 'http://foobar.com/index#bar',
    // whereas in the parent document, the iframe element's src attribute is
    // '/index'
    // To handle the scenario described above, this code optionally ignores
    // the iframe url's hash.
    if (iframes === null && frameLocation.origin === url.origin &&
        frameLocation.pathname === url.pathname &&
        frameLocation.search === url.search) {
      iframe = iframes[index];
    }
  }

  if (iframe === null) {
    return Promise.reject(
        new Error(`Unable to find iframe with url '${iframeUrl}'!`));
  }

  if (iframe.name) {
    return Promise.resolve(iframe.name);
  } else {
    return Promise.resolve(false);
  }
}

chrome.runtime.onMessage.addListener((request, sender, sendResponse) => {
  if (!request) return;
  switch (request.type) {
    case RecorderMsgEnum.START:
      startRecording(request.frameContext)
          .then(() => sendResponse(true))
          .catch((error) => {
            sendResponse(false);
            console.error(
                `Unable to start recording on ${window.location.href}!\r\n`,
                error);
          });
      return true;
    case RecorderMsgEnum.STOP:
      stopRecording();
      sendResponse(true);
      break;
    case RecorderMsgEnum.GET_IFRAME_NAME:
      console.log(`Cross: ${request.url}`);
      queryIframeName(request.url)
          .then((context) => {
            sendResponse(context);
          })
          .catch((error) => {
            sendResponse(false);
            console.error(`Unable to query for iframe!\r\n`, error);
          });
      return true;
    default:
  }
  // Return false to close the message channel. Lingering message
  // channels will prevent background scripts from unloading, causing it
  // to persist.
  return false;
});

if (typeof automation_helper === 'undefined') {
  var automation_helper = {
    // An enum specifying the state of an element on the page.
    DomElementReadyState: Object.freeze({
      'present': 0,
      'visible': 1 << 0,
      'enabled': 1 << 1,
      'on_top': 1 << 2,
    }),
  };

  automation_helper.getElementState =
      function(element) {
    let state_flags = this.DomElementReadyState.present;
    // Check if the element is disabled.
    if (!element.disabled) {
      state_flags |= this.DomElementReadyState.enabled;
    }
    // Check if the element is visible.
    if (element.offsetParent !== null && element.offsetWidth > 0 &&
        element.offsetHeight > 0) {
      state_flags |= this.DomElementReadyState.visible;

      // Check if the element is also on top.
      const rect = element.getBoundingClientRect();
      const topElement = document.elementFromPoint(
          // As coordinates, use the center of the element, minus
          // the window offset in case the element is outside the
          // view.
          rect.left + rect.width / 2 - window.pageXOffset,
          rect.top + rect.height / 2 - window.pageYOffset);
      if (isSelfOrDescendant(element, topElement)) {
        state_flags |= this.DomElementReadyState.on_top;
      }
    }
    return state_flags;
  };

  function isSelfOrDescendant(parent, child) {
    var node = child;
    while (node != null) {
      if (node == parent) {
        return true;
      }
      node = node.parentNode;
    }
    return false;
  }

  return automation_helper;
}
})();
