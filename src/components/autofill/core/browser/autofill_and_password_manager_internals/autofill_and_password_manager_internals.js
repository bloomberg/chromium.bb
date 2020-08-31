// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Autoscrolling keeps the page scrolled down. Intended usage is as follows:
// before modifying the DOM, check needsScrollDown(), and afterwards invoke
// scrollDown() if needsScrollDown() was true.

function isScrolledDown() {
  return window.innerHeight + window.scrollY >= document.body.offsetHeight;
}

let autoScrollActive = false;  // True iff autoscroll is currently scrolling.
let autoScrollTimer = null;    // Timer for resetting |autoScrollActive|.

function needsScrollDown() {
  const checkbox = document.getElementById('enable-autoscroll');
  return autoScrollActive || (isScrolledDown() && checkbox && checkbox.checked);
}

function scrollDown() {
  autoScrollActive = true;
  window.scrollTo(0, document.body.scrollHeight);
  (function unsetAutoScrollActiveAfterIdletime() {
    if (isScrolledDown()) {
      autoScrollActive = false;
    } else {
      clearTimeout(autoScrollTimer);
      autoScrollTimer = setTimeout(unsetAutoScrollActiveAfterIdletime, 50);
    }
  })();
}

// The configuration of log display can be represented in the URI fragment.
// Below are utility functions for setting/getting these parameters.

function makeKeyValueRegExp(key) {
  return new RegExp(`\\b${key}=([^&]*)`);
}

function setUrlHashParam(key, value) {
  key = encodeURIComponent(key);
  value = encodeURIComponent(value);
  const keyValueRegExp = makeKeyValueRegExp(key);
  const keyValue = `${key}=${value}`;
  if (keyValueRegExp.test(window.location.hash)) {
    const replaced = window.location.hash.replace(keyValueRegExp, keyValue);
    window.location.hash = replaced;
  } else {
    window.location.hash +=
        (window.location.hash.length > 0 ? '&' : '') + keyValue;
  }
}

function getUrlHashParam(key) {
  key = encodeURIComponent(key);
  const match = window.location.hash.match(makeKeyValueRegExp(key));
  if (!match || match[1] === undefined) {
    return undefined;
  }
  return decodeURIComponent(match[1]);
}


function addLog(logText) {
  const logDiv = $('log-entries');
  if (!logDiv) {
    return;
  }
  const scrollAfterInsert = needsScrollDown();
  logDiv.appendChild(document.createElement('hr'));
  const textDiv = document.createElement('div');
  textDiv.innerText = logText;
  logDiv.appendChild(textDiv);
  if (scrollAfterInsert) {
    scrollDown();
  }
}

// Converts an internal representation of nodes to actual DOM nodes that can
// be attached to the DOM. The internal representation has the following
// properties for each node:
// - type: 'element' | 'text'
// - value: name of tag | text content
// - children (opt): list of child nodes
// - attributes (opt): dictionary of name/value pairs
function nodeToDomNode(node) {
  if (node.type === 'text') {
    return document.createTextNode(node.value);
  }
  // Else the node is of type 'element'.
  const domNode = document.createElement(node.value);
  if ('children' in node) {
    node.children.forEach((child) => {
      domNode.appendChild(nodeToDomNode(child));
    });
  }
  if ('attributes' in node) {
    for (const attribute in node.attributes) {
      domNode.setAttribute(attribute, node.attributes[attribute]);
    }
  }
  return domNode;
}

// TODO(crbug.com/928595) Rename this to "addStructuredLog". Punting on this
// to simplify an existing CL that shall be merged.
function addRawLog(node) {
  const logDiv = $('log-entries');
  if (!logDiv) {
    return;
  }
  const scrollAfterInsert = needsScrollDown();
  logDiv.appendChild(document.createElement('hr'));
  if (node.type === 'fragment') {
    if ('children' in node) {
      node.children.forEach((child) => {
        logDiv.appendChild(nodeToDomNode(child));
      });
    }
  } else {
    logDiv.appendChild(nodeToDomNode(node));
  }
  if (scrollAfterInsert) {
    scrollDown();
  }
}

function setUpAutofillInternals() {
    document.title = "Autofill Internals";
    document.getElementById("h1-title").textContent = "Autofill Internals";
    document.getElementById("logging-note").innerText =
      "Captured autofill logs are listed below. Logs are cleared and no longer \
      captured when all autofill-internals pages are closed.";
    document.getElementById("logging-note-incognito").innerText =
      "Captured autofill logs are not available in Incognito.";
    setUpLogDisplayConfig();
}

function setUpPasswordManagerInternals() {
    document.title = "Password Manager Internals";
    document.getElementById("h1-title").textContent =
      "Password Manager Internals";
    document.getElementById("logging-note").innerText =
      "Captured password manager logs are listed below. Logs are cleared and \
      no longer captured when all password-manager-internals pages are closed.";
    document.getElementById("logging-note-incognito").innerText =
      "Captured password manager logs are not available in Incognito.";
}

function notifyAboutIncognito(isIncognito) {
  document.body.dataset.incognito = isIncognito;
}

function notifyAboutVariations(variations) {
  const list = document.createElement("div");
  for (const item of variations) {
    list.appendChild(document.createTextNode(item));
    list.appendChild(document.createElement("br"));
  }
  const variationsList = document.getElementById("variations-list");
  variationsList.appendChild(list);
}

// Sets up the top bar with checkboxes to show/hide the different sorts of log
// event types, a checkbox to enable/disable autoscroll, and a (fake) button to
// add visual markers (it's fake to keep Autofill from parsing the form).
function setUpLogDisplayConfig() {
  const SCOPES = [
    'Context',
    'Parsing',
    'AbortParsing',
    'Filling',
    'Submission',
    'AutofillServer',
    'Metrics',
    'AddressProfileFormImport',
  ];
  const displayConfigDiv = document.getElementById('log-display-config');
  const logDiv = document.getElementById('log-entries');
  const autoScrollInput = document.getElementById('enable-autoscroll');

  displayConfigDiv.style.display = 'block';
  displayConfigDiv.parentElement.classList.add('sticky-bar');

  // Initialize the auto-scroll checkbox.
  autoScrollInput.checked = getUrlHashParam('autoscroll') !== 'n';
  autoScrollInput.addEventListener('change', (event) => {
    setUrlHashParam('autoscroll', autoScrollInput.checked ? 'y' : 'n');
  });

  // Create and initialize filter checkboxes: remove/add hide-<Scope> class to
  // |logDiv| when (un)checked.
  for (const scope of SCOPES) {
    const input = document.createElement('input');
    input.setAttribute('type', 'checkbox');
    input.setAttribute('id', `checkbox-${scope}`);
    input.checked = getUrlHashParam(scope) !== 'n';
    function changeHandler() {
      setUrlHashParam(scope, input.checked ? 'y' : 'n');
      const cls = `hide-${scope}`;
      const scrollAfterInsert = needsScrollDown();
      if (!input.checked) {
        logDiv.classList.add(cls);
      } else {
        logDiv.classList.remove(cls);
      }
      if (scrollAfterInsert) {
        scrollDown();
      }
    }
    input.addEventListener('change', changeHandler);
    changeHandler();  // Call once to initialize |logDiv|'s classes.
    const label = document.createElement('label');
    label.setAttribute('for', `checkbox-${scope}`);
    label.innerText = scope;
    displayConfigDiv.appendChild(input);
    displayConfigDiv.appendChild(label);
  }

  // Initialize marker field: when pressed, add fake log event.
  let markerCounter = 0;
  const markerFakeButton = document.getElementById('marker-fake-button');
  markerFakeButton.addEventListener('click', () => {
    ++markerCounter;
    const scrollAfterInsert = needsScrollDown();
    addRawLog({
      type: 'element',
      value: 'div',
      attributes: {'class': 'marker', 'contenteditable': 'true'},
      children: [{type: 'text', value: `#${markerCounter} `}]
    });
    if (scrollAfterInsert) {
      scrollDown();
      // Focus marker div, set caret at end of line.
      const markerNode = logDiv.lastChild;
      const textNode = markerNode.lastChild;
      markerNode.focus();
      window.getSelection().collapse(textNode, textNode.length);
    }
  });
}

document.addEventListener("DOMContentLoaded", function(event) {
  chrome.send('loaded');
});
