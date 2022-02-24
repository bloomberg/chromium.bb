// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/mojo_webui_test_support.js';

import {CrAutoImgElement, RecipeModuleElement, recipeTasksV2Descriptor, TaskModuleHandlerProxy} from 'chrome://new-tab-page/new_tab_page.js';
import {TaskModuleHandlerRemote} from 'chrome://new-tab-page/task_module.mojom-webui.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

import {installMock} from '../../test_support.js';

suite('NewTabPageModulesRecipesV2ModuleTest', () => {
  let handler: TestBrowserProxy;

  setup(() => {
    document.body.innerHTML = '';

    handler =
        installMock(TaskModuleHandlerRemote, TaskModuleHandlerProxy.setHandler);
  });

  test('module appears on render with recipes', async () => {
    // Arrange.
    const task = {
      title: 'First Recipes',
      taskItems: [
        {
          name: 'apricot',
          imageUrl: {url: 'https://apricot.com/img.png'},
          info: 'Viewed 6 months ago',
          siteName: 'Apricot Site',
        },
        {
          name: 'banana',
          imageUrl: {url: 'https://banana.com/img.png'},
          info: 'Viewed 2 days ago',
          siteName: 'Banana Site',
        },
        {
          name: 'cranberry',
          imageUrl: {url: 'https://cranberry.com/img.png'},
          info: 'Viewed 3 weeks ago',
          siteName: 'Cranberry Site',
        },
      ],
    };
    handler.setResultFor('getPrimaryTask', Promise.resolve({task}));

    // Act.
    const moduleElement =
        await recipeTasksV2Descriptor.initialize(0) as RecipeModuleElement;
    assertTrue(!!moduleElement);
    document.body.append(moduleElement);
    moduleElement.$.recipesRepeat.render();

    // Assert.
    const recipes =
        Array.from(moduleElement.shadowRoot!.querySelectorAll<HTMLElement>(
            '.recipe-item'));
    assertEquals(1, handler.getCallCount('getPrimaryTask'));
    assertEquals(3, recipes.length);
    assertEquals(
        'https://apricot.com/img.png',
        recipes[0]!.querySelector<CrAutoImgElement>('img')!.autoSrc);
    let nameElement = recipes[0]!.querySelector<HTMLElement>('.name')!;
    assertEquals('apricot', nameElement.innerText);
    assertEquals('apricot', nameElement.title);
    assertEquals(
        'Viewed 6 months ago',
        recipes[0]!.querySelector<HTMLElement>('.info')!.innerText);
    assertEquals(
        'Apricot Site',
        recipes[0]!.querySelector<HTMLElement>('.site-name')!.innerText);
    assertEquals(
        'https://banana.com/img.png',
        recipes[1]!.querySelector<CrAutoImgElement>('img')!.autoSrc);
    nameElement = recipes[1]!.querySelector<HTMLElement>('.name')!;
    assertEquals('banana', nameElement.innerText);
    assertEquals('banana', nameElement.title);
    assertEquals(
        'Viewed 2 days ago',
        recipes[1]!.querySelector<HTMLElement>('.info')!.innerText);
    assertEquals(
        'Banana Site',
        recipes[1]!.querySelector<HTMLElement>('.site-name')!.innerText);
    assertEquals(
        'https://cranberry.com/img.png',
        recipes[2]!.querySelector<CrAutoImgElement>('img')!.autoSrc);
    nameElement = recipes[2]!.querySelector<HTMLElement>('.name')!;
    assertEquals('cranberry', nameElement.innerText);
    assertEquals('cranberry', nameElement.title);
    assertEquals(
        'Viewed 3 weeks ago',
        recipes[2]!.querySelector<HTMLElement>('.info')!.innerText);
    assertEquals(
        'Cranberry Site',
        recipes[2]!.querySelector<HTMLElement>('.site-name')!.innerText);
  });

  test('empty module renders if no tasks available', async () => {
    // Arrange.
    handler.setResultFor('getPrimaryTask', Promise.resolve({task: null}));

    // Act.
    const moduleElement = await recipeTasksV2Descriptor.initialize(0);

    // Assert.
    assertTrue(!!moduleElement);
  });
});
