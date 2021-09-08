// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as i18n from '../i18n/i18n.js';
import type * as Platform from '../platform/platform.js';
import * as Root from '../root/root.js';

import type {SettingStorageType} from './Settings.js';

const UIStrings = {
  /**
  *@description Title of the Elements Panel
  */
  elements: 'Elements',
  /**
  *@description Text for DevTools appearance
  */
  appearance: 'Appearance',
  /**
  *@description Name of the Sources panel
  */
  sources: 'Sources',
  /**
  *@description Title of the Network tool
  */
  network: 'Network',
  /**
  *@description Text for the performance of something
  */
  performance: 'Performance',
  /**
  *@description Title of the Console tool
  */
  console: 'Console',
  /**
  *@description A title of the 'Persistence' setting category
  */
  persistence: 'Persistence',
  /**
  *@description Text that refers to the debugger
  */
  debugger: 'Debugger',
  /**
  *@description Text describing global shortcuts and settings that are available throughout the DevTools
  */
  global: 'Global',
  /**
  *@description Title of the Rendering tool
  */
  rendering: 'Rendering',
  /**
  *@description Title of a section on CSS Grid tooling
  */
  grid: 'Grid',
  /**
  *@description Text for the mobile platform, as opposed to desktop
  */
  mobile: 'Mobile',
  /**
  *@description Text for the memory of the page
  */
  memory: 'Memory',
  /**
  *@description Text for the extension of the page
  */
  extension: 'Extension',
  /**
  *@description Text for the adorner of the page
  */
  adorner: 'Adorner',
};
const str_ = i18n.i18n.registerUIStrings('core/common/SettingRegistration.ts', UIStrings);
const i18nString = i18n.i18n.getLocalizedString.bind(undefined, str_);
let registeredSettings: Array<SettingRegistration> = [];
const settingNameSet = new Set<string>();


export function registerSettingExtension(registration: SettingRegistration): void {
  const settingName = registration.settingName;
  if (settingNameSet.has(settingName)) {
    throw new Error(`Duplicate setting name '${settingName}'`);
  }
  settingNameSet.add(settingName);
  registeredSettings.push(registration);
}

export function getRegisteredSettings(): Array<SettingRegistration> {
  return registeredSettings.filter(
      setting =>
          Root.Runtime.Runtime.isDescriptorEnabled({experiment: setting.experiment, condition: setting.condition}));
}

export function registerSettingsForTest(settings: Array<SettingRegistration>, forceReset: boolean = false): void {
  if (registeredSettings.length === 0 || forceReset) {
    registeredSettings = settings;
    settingNameSet.clear();
    for (const setting of settings) {
      const settingName = setting.settingName;
      if (settingNameSet.has(settingName)) {
        throw new Error(`Duplicate setting name '${settingName}'`);
      }
      settingNameSet.add(settingName);
    }
  }
}

export function resetSettings(): void {
  registeredSettings = [];
}

export function maybeRemoveSettingExtension(settingName: string): boolean {
  const settingIndex = registeredSettings.findIndex(setting => setting.settingName === settingName);
  if (settingIndex < 0 || !settingNameSet.delete(settingName)) {
    return false;
  }
  registeredSettings.splice(settingIndex, 1);
  return true;
}

// TODO(crbug.com/1167717): Make this a const enum again
// eslint-disable-next-line rulesdir/const_enum
export enum SettingCategory {
  NONE = '',  // `NONE` must be a falsy value. Legacy code uses if-checks for the category.
  ELEMENTS = 'ELEMENTS',
  APPEARANCE = 'APPEARANCE',
  SOURCES = 'SOURCES',
  NETWORK = 'NETWORK',
  PERFORMANCE = 'PERFORMANCE',
  CONSOLE = 'CONSOLE',
  PERSISTENCE = 'PERSISTENCE',
  DEBUGGER = 'DEBUGGER',
  GLOBAL = 'GLOBAL',
  RENDERING = 'RENDERING',
  GRID = 'GRID',
  MOBILE = 'MOBILE',
  EMULATION = 'EMULATION',
  MEMORY = 'MEMORY',
  EXTENSIONS = 'EXTENSIONS',
  ADORNER = 'ADORNER',
}

export function getLocalizedSettingsCategory(category: SettingCategory): string|Platform.UIString.LocalizedString {
  switch (category) {
    case SettingCategory.ELEMENTS:
      return i18nString(UIStrings.elements);
    case SettingCategory.APPEARANCE:
      return i18nString(UIStrings.appearance);
    case SettingCategory.SOURCES:
      return i18nString(UIStrings.sources);
    case SettingCategory.NETWORK:
      return i18nString(UIStrings.network);
    case SettingCategory.PERFORMANCE:
      return i18nString(UIStrings.performance);
    case SettingCategory.CONSOLE:
      return i18nString(UIStrings.console);
    case SettingCategory.PERSISTENCE:
      return i18nString(UIStrings.persistence);
    case SettingCategory.DEBUGGER:
      return i18nString(UIStrings.debugger);
    case SettingCategory.GLOBAL:
      return i18nString(UIStrings.global);
    case SettingCategory.RENDERING:
      return i18nString(UIStrings.rendering);
    case SettingCategory.GRID:
      return i18nString(UIStrings.grid);
    case SettingCategory.MOBILE:
      return i18nString(UIStrings.mobile);
    case SettingCategory.EMULATION:
      return i18nString(UIStrings.console);
    case SettingCategory.MEMORY:
      return i18nString(UIStrings.memory);
    case SettingCategory.EXTENSIONS:
      return i18nString(UIStrings.extension);
    case SettingCategory.ADORNER:
      return i18nString(UIStrings.adorner);
    case SettingCategory.NONE:
      return '';
  }
}

// TODO(crbug.com/1167717): Make this a const enum again
// eslint-disable-next-line rulesdir/const_enum
export enum SettingType {
  ARRAY = 'array',
  REGEX = 'regex',
  ENUM = 'enum',
  BOOLEAN = 'boolean',
}

export interface RegExpSettingItem {
  pattern: string;
  disabled?: boolean;
}

export interface SettingRegistration {
  /**
   * The category with which the setting is displayed in the UI.
   */
  category?: SettingCategory;
  /**
   * Used to sort on screen the settings that belong to the same category.
   */
  order?: number;
  /**
   * The title with which the setting is shown on screen.
   */
  title?: () => Platform.UIString.LocalizedString;
  /**
   * The title with which the setting is shown on screen, if the platform running DevTools is 'mac'.
   * If not set, the 'title' field will be used instead.
   */
  titleMac?: () => Platform.UIString.LocalizedString;
  /**
   * The identifier of the setting.
   */
  settingName: string;
  /**
   * Determines how the possible values of the setting are expressed.
   *
   * - If the setting can only be enabled and disabled use BOOLEAN
   * - If the setting has a list of possible values use ENUM
   * - If each setting value is a set of objects use ARRAY
   * - If the setting value is a regular expression use REGEX
   */
  settingType: SettingType;
  /**
   * The value set by default to the setting.
   */
  defaultValue: unknown;
  /**
   * Words used to find a setting in the Command Menu.
   */
  tags?: Array<() => Platform.UIString.LocalizedString>;
  /**
   * The possible values the setting can have, each with a description composed of a title and an optional text.
   */
  options?: Array<SettingExtensionOption>;
  /**
   * Whether DevTools must be reloaded for a change in the setting to take effect.
   */
  reloadRequired?: boolean;
  /**
   * Determines if the setting value is stored in the global, local or session storage.
   */
  storageType?: SettingStorageType;
  /**
   * A condition that, when present in the queryParamsObject of Runtime, constraints the value
   * of the setting to be changed only if the user set it.
   */
  userActionCondition?: string;
  /**
   * The name of the experiment a setting is associated with. Enabling and disabling the declared
   * experiment will enable and disable the setting respectively.
   */
  experiment?: Root.Runtime.ExperimentName;
  /**
   * A condition represented as a string the setting's availability depends on. Conditions come
   * from the queryParamsObject defined in Runtime and just as the experiment field, they determine the availability
   * of the setting. A condition can be negated by prepending a ‘!’ to the value of the condition
   * property and in that case the behaviour of the setting's availability will be inverted.
   */
  condition?: Root.Runtime.ConditionName;
}
interface LocalizedSettingExtensionOption {
  value: boolean|string;
  title: () => Platform.UIString.LocalizedString;
  text?: () => Platform.UIString.LocalizedString;
  raw?: false;
}
interface RawSettingExtensionOption {
  value: boolean|string;
  title: () => Platform.UIString.LocalizedString;
  /**
   * Text used to describe the option. Must be localized if 'raw' is false.
   */
  text?: string;
  raw: true;
}
export type SettingExtensionOption = LocalizedSettingExtensionOption|RawSettingExtensionOption;
