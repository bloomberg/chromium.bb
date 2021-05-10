// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as Common from '../common/common.js';
import * as Diff from '../diff/diff.js';
import * as Host from '../host/host.js';
import * as i18n from '../i18n/i18n.js';
import * as Platform from '../platform/platform.js';
import * as UI from '../ui/ui.js';

import {FilteredListWidget, Provider, registerProvider} from './FilteredListWidget.js';
import {QuickOpenImpl} from './QuickOpen.js';

export const UIStrings = {
  /**
  * @description Message to display if a setting change requires a reload of DevTools
  */
  oneOrMoreSettingsHaveChanged: 'One or more settings have changed which requires a reload to take effect.',
  /**
  * @description Text in Command Menu of the Command Menu
  */
  noCommandsFound: 'No commands found',
  /**
  * @description Text in Command Menu of the Command Menu
  */
  runCommand: 'Run Command',
};
const str_ = i18n.i18n.registerUIStrings('quick_open/CommandMenu.js', UIStrings);
const i18nString = i18n.i18n.getLocalizedString.bind(undefined, str_);

/** @type {!CommandMenu} */
let commandMenuInstance;

export class CommandMenu {
  /** @private */
  constructor() {
    /** @type {!Array<!Command>} */
    this._commands = [];
    this._loadCommands();
  }
  /**
   * @param {{forceNew: ?boolean}} opts
   */
  static instance(opts = {forceNew: null}) {
    const {forceNew} = opts;
    if (!commandMenuInstance || forceNew) {
      commandMenuInstance = new CommandMenu();
    }
    return commandMenuInstance;
  }

  /**
   * @param {!CreateCommandOptions} options
   * @return {!Command}
   */
  static createCommand(options) {
    const {category, keys, title, shortcut, executeHandler, availableHandler, userActionCode} = options;

    let handler = executeHandler;
    if (userActionCode) {
      const actionCode = userActionCode;
      handler = () => {
        Host.userMetrics.actionTaken(actionCode);
        executeHandler();
      };
    }
    if (title === 'Show Issues') {
      const cached_handler = handler;
      handler = () => {
        Host.userMetrics.issuesPanelOpenedFrom(Host.UserMetrics.IssueOpener.CommandMenu);
        cached_handler();
      };
    }

    return new Command(category, title, keys, shortcut, handler, availableHandler);
  }

  /**
   * @param {!Common.Settings.Setting<*>} setting
   * @param {string} title
   * @param {V} value
   * @return {!Command}
   * @template V
   */
  static createSettingCommand(setting, title, value) {
    const category = setting.category();
    if (!category) {
      throw new Error(`Creating '${title}' setting command failed. Setting has no category.`);
    }
    const tags = setting.tags() || '';
    const reloadRequired = Boolean(setting.reloadRequired());
    return CommandMenu.createCommand({
      category: Common.Settings.getLocalizedSettingsCategory(category),
      keys: tags,
      title,
      shortcut: '',
      executeHandler: () => {
        setting.set(value);
        if (reloadRequired) {
          UI.InspectorView.InspectorView.instance().displayReloadRequiredWarning(
              i18nString(UIStrings.oneOrMoreSettingsHaveChanged));
        }
      },
      availableHandler,
      userActionCode: undefined,
    });

    /**
     * @return {boolean}
     */
    function availableHandler() {
      return setting.get() !== value;
    }
  }

  /**
   * @param {!ActionCommandOptions} options
   * @return {!Command}
   */
  static createActionCommand(options) {
    const {action, userActionCode} = options;
    const shortcut = UI.ShortcutRegistry.ShortcutRegistry.instance().shortcutTitleForAction(action.id()) || '';

    return CommandMenu.createCommand({
      category: action.category(),
      keys: action.tags() || '',
      title: action.title() || '',
      shortcut,
      executeHandler: action.execute.bind(action),
      userActionCode,
      availableHandler: undefined,
    });
  }

  /**
   * @param {!RevealViewCommandOptions} options
   * @return {!Command}
   */
  static createRevealViewCommand(options) {
    const {title, tags, category, userActionCode, id} = options;

    return CommandMenu.createCommand({
      category,
      keys: tags,
      title,
      shortcut: '',
      executeHandler: UI.ViewManager.ViewManager.instance().showView.bind(
          UI.ViewManager.ViewManager.instance(), id, /* userGesture */ true),
      userActionCode,
      availableHandler: undefined
    });
  }

  _loadCommands() {
    const locations = new Map();
    for (const {category, name} of UI.ViewManager.getRegisteredLocationResolvers()) {
      if (category && name) {
        locations.set(name, category);
      }
    }
    const views = UI.ViewManager.getRegisteredViewExtensions();
    for (const view of views) {
      const viewLocation = view.location();
      const category = viewLocation && locations.get(viewLocation);
      if (!category) {
        continue;
      }

      /** @type {!RevealViewCommandOptions} */
      const options = {
        title: view.commandPrompt(),
        tags: view.tags() || '',
        category,
        userActionCode: undefined,
        id: view.viewId()
      };
      this._commands.push(CommandMenu.createRevealViewCommand(options));
    }
    // Populate allowlisted settings.
    const settingsRegistrations = Common.Settings.getRegisteredSettings();
    for (const settingRegistration of settingsRegistrations) {
      const options = settingRegistration.options;
      if (!options || !settingRegistration.category) {
        continue;
      }
      for (const pair of options) {
        const setting = Common.Settings.Settings.instance().moduleSetting(settingRegistration.settingName);
        this._commands.push(CommandMenu.createSettingCommand(setting, pair.title(), pair.value));
      }
    }
  }

  /**
   * @return {!Array.<!Command>}
   */
  commands() {
    return this._commands;
  }
}

/**
 * @typedef {{
 *   action: !UI.ActionRegistration.Action,
 *   userActionCode: (!Host.UserMetrics.Action|undefined),
 * }}
 */
// @ts-ignore typedef
export let ActionCommandOptions;

/**
 * @typedef {{
 *   id: string,
 *   title: string,
 *   tags: string,
 *   category: string,
 *   userActionCode: (!Host.UserMetrics.Action|undefined)
 * }}
 */
// @ts-ignore typedef
export let RevealViewCommandOptions;

/**
 * @typedef {{
 *   category: string,
 *   keys: string,
 *   title: string,
 *   shortcut: string,
 *   executeHandler: !function():*,
 *   availableHandler: ((!function():boolean)|undefined),
 *   userActionCode: (!Host.UserMetrics.Action|undefined)
 * }}
 */
// @ts-ignore typedef
export let CreateCommandOptions;

/** @type {!CommandMenuProvider} */
let commandMenuProviderInstance;


export class CommandMenuProvider extends Provider {
  /** @private */
  constructor() {
    super();
    /** @type {!Array<!Command>} */
    this._commands = [];
  }
  /**
   * @param {{forceNew: ?boolean}} opts
   */
  static instance(opts = {forceNew: null}) {
    const {forceNew} = opts;
    if (!commandMenuProviderInstance || forceNew) {
      commandMenuProviderInstance = new CommandMenuProvider();
    }

    return commandMenuProviderInstance;
  }

  /**
   * @override
   */
  attach() {
    const allCommands = CommandMenu.instance().commands();

    // Populate allowlisted actions.
    const actions = UI.ActionRegistry.ActionRegistry.instance().availableActions();
    for (const action of actions) {
      const category = action.category();
      if (!category) {
        continue;
      }

      /** @type {!ActionCommandOptions} */
      const options = {action, userActionCode: undefined};
      this._commands.push(CommandMenu.createActionCommand(options));
    }

    for (const command of allCommands) {
      if (command.available()) {
        this._commands.push(command);
      }
    }

    this._commands = this._commands.sort(commandComparator);

    /**
     * @param {!Command} left
     * @param {!Command} right
     * @return {number}
     */
    function commandComparator(left, right) {
      const cats = Platform.StringUtilities.compare(left.category(), right.category());
      return cats ? cats : Platform.StringUtilities.compare(left.title(), right.title());
    }
  }

  /**
   * @override
   */
  detach() {
    this._commands = [];
  }

  /**
   * @override
   * @return {number}
   */
  itemCount() {
    return this._commands.length;
  }

  /**
   * @override
   * @param {number} itemIndex
   * @return {string}
   */
  itemKeyAt(itemIndex) {
    return this._commands[itemIndex].key();
  }

  /**
   * @override
   * @param {number} itemIndex
   * @param {string} query
   * @return {number}
   */
  itemScoreAt(itemIndex, query) {
    const command = this._commands[itemIndex];
    let score = Diff.Diff.DiffWrapper.characterScore(query.toLowerCase(), command.title().toLowerCase());

    // Score panel/drawer reveals above regular actions.
    if (command.category().startsWith('Panel')) {
      score += 2;
    } else if (command.category().startsWith('Drawer')) {
      score += 1;
    }

    return score;
  }

  /**
   * @override
   * @param {number} itemIndex
   * @param {string} query
   * @param {!Element} titleElement
   * @param {!Element} subtitleElement
   */
  renderItem(itemIndex, query, titleElement, subtitleElement) {
    const command = this._commands[itemIndex];
    titleElement.removeChildren();
    const tagElement = /** @type {!HTMLElement} */ (titleElement.createChild('span', 'tag'));
    const index = Platform.StringUtilities.hashCode(command.category()) % MaterialPaletteColors.length;
    tagElement.style.backgroundColor = MaterialPaletteColors[index];
    tagElement.textContent = command.category();
    UI.UIUtils.createTextChild(titleElement, command.title());
    FilteredListWidget.highlightRanges(titleElement, query, true);
    subtitleElement.textContent = command.shortcut();
  }

  /**
   * @override
   * @param {?number} itemIndex
   * @param {string} promptValue
   */
  selectItem(itemIndex, promptValue) {
    if (itemIndex === null) {
      return;
    }
    this._commands[itemIndex].execute();
    Host.userMetrics.actionTaken(Host.UserMetrics.Action.SelectCommandFromCommandMenu);
  }

  /**
   * @override
   * @return {string}
   */
  notFoundText() {
    return i18nString(UIStrings.noCommandsFound);
  }
}

export const MaterialPaletteColors = [
  '#F44336', '#E91E63', '#9C27B0', '#673AB7', '#3F51B5', '#03A9F4', '#00BCD4', '#009688', '#4CAF50', '#8BC34A',
  '#CDDC39', '#FFC107', '#FF9800', '#FF5722', '#795548', '#9E9E9E', '#607D8B'
];

export class Command {
  /**
   * @param {string} category
   * @param {string} title
   * @param {string} key
   * @param {string} shortcut
   * @param {function():*} executeHandler
   * @param {function()=} availableHandler
   */
  constructor(category, title, key, shortcut, executeHandler, availableHandler) {
    this._category = category;
    this._title = title;
    this._key = category + '\0' + title + '\0' + key;
    this._shortcut = shortcut;
    this._executeHandler = executeHandler;
    this._availableHandler = availableHandler;
  }

  /**
   * @return {string}
   */
  category() {
    return this._category;
  }

  /**
   * @return {string}
   */
  title() {
    return this._title;
  }

  /**
   * @return {string}
   */
  key() {
    return this._key;
  }

  /**
   * @return {string}
   */
  shortcut() {
    return this._shortcut;
  }

  /**
   * @return {boolean}
   */
  available() {
    return this._availableHandler ? this._availableHandler() : true;
  }

  execute() {
    this._executeHandler();
  }
}

/** @type {!ShowActionDelegate} */
let showActionDelegateInstance;
/**
 * @implements {UI.ActionRegistration.ActionDelegate}
 */
export class ShowActionDelegate {
  /**
   * @param {{forceNew: ?boolean}} opts
   */
  static instance(opts = {forceNew: null}) {
    const {forceNew} = opts;
    if (!showActionDelegateInstance || forceNew) {
      showActionDelegateInstance = new ShowActionDelegate();
    }

    return showActionDelegateInstance;
  }

  /**
   * @override
   * @param {!UI.Context.Context} context
   * @param {string} actionId
   * @return {boolean}
   */
  handleAction(context, actionId) {
    Host.InspectorFrontendHost.InspectorFrontendHostInstance.bringToFront();
    QuickOpenImpl.show('>');
    return true;
  }
}

registerProvider({
  prefix: '>',
  title: () => i18nString(UIStrings.runCommand),
  provider: CommandMenuProvider.instance,
});
