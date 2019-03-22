// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

document.addEventListener('DOMContentLoaded', () => {
  const startingURLField = document.getElementById('startingURL');
  const saveBtn = document.getElementById('saveBtn');
  const stopBtn = document.getElementById('stopBtn');
  const cancelBtn = document.getElementById('cancelBtn');
  const actionsListDiv = document.getElementById('actions');

  // Sets up the 'Add Chrome Action' panel.
  const newActionTypeSelect = document.getElementById('newActionType');
  const passwordActionUserParam = document.getElementById('pmUser');
  const passwordActionPasswordParam = document.getElementById('pmPassword');
  createNewActionTypeOption('Save Password', 'savePassword');
  createNewActionTypeOption('Don\'t Save Password', 'rejectSavePassword');
  createNewActionTypeOption('Update Password', 'updatePassword');
  createNewActionTypeOption('Don\'t Update Password', 'rejectUpdatePassword');
  const addChromeAction = document.getElementById('addChromeAction');
  addChromeAction.addEventListener('click', (event) => {
    let action = {type: newActionTypeSelect.value};

    switch (newActionTypeSelect.value) {
      case ActionTypeEnum.SAVE_PASSWORD:
      case ActionTypeEnum.REJECT_SAVE_PASSWORD:
      case ActionTypeEnum.UPDATE_PASSWORD:
      case ActionTypeEnum.REJECT_UPDATE_PASSWORD:
        action.userName = passwordActionUserParam.value;
        action.password = passwordActionPasswordParam.value;
        break;
      default:
    }

    return sendRuntimeMessageToBackgroundScript(
        {type: RecorderMsgEnum.ADD_ACTION, action: action});
  });

  function createNewActionTypeOption(text, value) {
    const option = document.createElement('option');
    option.value = value;
    option.textContent = text;
    newActionTypeSelect.appendChild(option);
  }

  function createActionElement(action) {
    const actionDiv = document.createElement('div');
    actionDiv.classList.add('action');

    // The action row.
    const flexContainerDiv = document.createElement('div');
    flexContainerDiv.classList.add('h-flex-container');
    flexContainerDiv.classList.add('tooltip-trigger');
    const selectorLabel = document.createElement('h4');
    const actionLabel = document.createElement('h4');
    const iconBoxDiv = document.createElement('div');
    iconBoxDiv.classList.add('iconBox');
    iconBoxDiv.textContent = 'X';
    iconBoxDiv.addEventListener('click', (event) => {
      sendRuntimeMessageToBackgroundScript({
        type: RecorderUiMsgEnum.REMOVE_ACTION,
        action_index: action.action_index
      })
          .then((response) => {
            if (response) {
              actionDiv.remove();
            }
          })
          .catch((error) => {
            console.error('Unable to remove the recorded action!', error);
          });
    });
    flexContainerDiv.appendChild(selectorLabel);
    flexContainerDiv.appendChild(actionLabel);
    flexContainerDiv.appendChild(iconBoxDiv);

    // The action detail row.
    const detailsDiv = document.createElement('div');
    detailsDiv.classList.add('tooltip-body');
    const selectorDetailLabel = document.createElement('label');
    const actionDetailLabel = document.createElement('label');
    detailsDiv.appendChild(selectorDetailLabel);
    detailsDiv.appendChild(actionDetailLabel);

    selectorLabel.textContent = action.selector;
    selectorDetailLabel.textContent = action.selector;

    switch (action.type) {
      case ActionTypeEnum.AUTOFILL:
        actionLabel.textContent = 'trigger autofill';
        actionDetailLabel.textContent = `trigger autofill`;
        break;
      case ActionTypeEnum.CLICK:
        actionLabel.textContent = 'left-click';
        actionDetailLabel.textContent = `left click element`;
        break;
      case ActionTypeEnum.HOVER:
        actionLabel.textContent = 'hover';
        actionDetailLabel.textContent = `hover over element`;
        break;
      case ActionTypeEnum.LOAD_PAGE:
        actionLabel.textContent = 'loaded page';
        actionDetailLabel.textContent = 'loaded a new page';
        selectorLabel.textContent = action.url;
        selectorDetailLabel.textContent = action.url;
        break;
      case ActionTypeEnum.PRESS_ENTER:
        actionLabel.textContent = 'enter';
        actionDetailLabel.textContent = `press enter`;
        break;
      case ActionTypeEnum.SELECT:
        actionLabel.textContent = 'select dropdown option';
        actionDetailLabel.textContent = `select option ${action.index}`;
        break;
      case ActionTypeEnum.TYPE:
        actionLabel.textContent = 'type';
        actionDetailLabel.textContent = `type '${action.value}'`;
        break;
      case ActionTypeEnum.TYPE_PASSWORD:
        actionLabel.textContent = 'type password';
        actionDetailLabel.textContent = `type '${action.value}'`;
        break;
      case ActionTypeEnum.VALIDATE_FIELD:
        actionLabel.textContent = 'validate field';
        if (action.expectedAutofillType) {
          actionDetailLabel.textContent = `check that field
              (${action.expectedAutofillType}) has the value
              '${action.expectedValue}'`;
        } else {
          actionDetailLabel.textContent = `check that field
              has the value '${action.expectedValue}'`;
        }
        break;
      case ActionTypeEnum.SAVE_PASSWORD:
        actionLabel.textContent = 'save password';
        actionDetailLabel.textContent = `save password
            '${action.password}' for
            '${action.userName}'`;
        break;
      case ActionTypeEnum.REJECT_SAVE_PASSWORD:
        actionLabel.textContent = 'reject saving password';
        actionDetailLabel.textContent = `reject saving password
            '${action.password}' for
            '${action.userName}'`;
        break;
      case ActionTypeEnum.UPDATE_PASSWORD:
        actionLabel.textContent = 'update password';
        actionDetailLabel.textContent = `update password
            '${action.password}' for
            '${action.userName}'`;
        break;
      case ActionTypeEnum.REJECT_UPDATE_PASSWORD:
        actionLabel.textContent = 'reject updating password';
        actionDetailLabel.textContent = `reject updating password
            '${action.password}' for
            '${action.userName}'`;
        break;
      default:
        actionLabel.textContent = 'unknown';
        actionDetailLabel.textContent = `unknown action: ${action.type}`;
    }

    flexContainerDiv.appendChild(detailsDiv);
    actionDiv.appendChild(flexContainerDiv);
    return actionDiv;
  }

  function addAction(action) {
    actionsListDiv.appendChild(createActionElement(action));
    // Scroll to the action that the panel just appended.
    actionsListDiv.scrollTop = actionsListDiv.scrollHeight;
  }

  function setPasswordEventParams(userName, password) {
    passwordActionUserParam.value = userName;
    passwordActionPasswordParam.value = password;
  }

  saveBtn.addEventListener('click', (event) => {
    sendRuntimeMessageToBackgroundScript({type: RecorderMsgEnum.SAVE});
  });
  stopBtn.addEventListener('click', (event) => {
    sendRuntimeMessageToBackgroundScript({type: RecorderMsgEnum.STOP});
  });
  cancelBtn.addEventListener('click', (event) => {
    sendRuntimeMessageToBackgroundScript({type: RecorderMsgEnum.CANCEL});
  });

  chrome.runtime.onMessage.addListener((request, sender, sendResponse) => {
    if (!request) return;
    switch (request.type) {
      case RecorderUiMsgEnum.ADD_ACTION:
        addAction(request.action);
        sendResponse(true);
        break;
      case RecorderUiMsgEnum.SET_PASSWORD_MANAGER_ACTION_PARAMS:
        setPasswordEventParams(request.userName, request.password);
        sendResponse(true);
      default:
    }
    return false;
  });

  // Get the recipe from the background script and render it.
  sendRuntimeMessageToBackgroundScript({
    type: RecorderUiMsgEnum.GET_RECIPE
  }).then((recipe) => {
    while (actionsListDiv.hasChildNodes()) {
      actionsListDiv.removeChild(actionsListDiv.lastChild);
    }

    startingURLField.value = recipe.startingURL;

    for (let index = 0; index < recipe.actions.length; index++) {
      const action = recipe.actions[index];
      actionsListDiv.appendChild(createActionElement(action));
    }
  });

  // Get the saved event parameters from the background script and
  // set parameters on the UI.
  sendRuntimeMessageToBackgroundScript(
      {type: RecorderUiMsgEnum.GET_SAVED_ACTION_PARAMS})
      .then((params) => {
        if (params.passwordManagerParams) {
          passwordActionUserParam.value = params.passwordManagerParams.userName;
          passwordActionPasswordParam.value =
              params.passwordManagerParams.password;
        }
      })
      .catch((error) => {
        console.error('Unable to obtain saved action parameters!\r\n', error);
      });
});
