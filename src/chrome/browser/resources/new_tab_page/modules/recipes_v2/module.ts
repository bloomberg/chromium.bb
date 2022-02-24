// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../module_header.js';

import {DomRepeat, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../../i18n_setup.js';
import {TaskItem, TaskModuleType} from '../../task_module.mojom-webui.js';
import {ModuleDescriptorV2, ModuleHeight} from '../module_descriptor.js';
import {TaskModuleHandlerProxy} from '../task_module/task_module_handler_proxy.js';

import {getTemplate} from './module.html.js';

export interface RecipeModuleElement {
  $: {
    recipesRepeat: DomRepeat,
  };
}

export class RecipeModuleElement extends PolymerElement {
  static get is() {
    return 'ntp-recipes-module-redesigned';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      recipes: Array,
    };
  }

  recipes: TaskItem[];
}

customElements.define(RecipeModuleElement.is, RecipeModuleElement);

async function createModule(): Promise<HTMLElement> {
  const {task} = await TaskModuleHandlerProxy.getHandler().getPrimaryTask(
      TaskModuleType.kRecipe);
  const element = new RecipeModuleElement();
  element.recipes = (task && task.taskItems) || [];
  return element;
}

export const recipeTasksDescriptor: ModuleDescriptorV2 = new ModuleDescriptorV2(
    /*id=*/ 'recipe_tasks',
    /*name=*/ loadTimeData.getString('modulesRecipeTasksSentence'),
    /*height*/ ModuleHeight.TALL, createModule);
