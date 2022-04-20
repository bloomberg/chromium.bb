// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/mojo_webui_test_support.js';

import {DismissModuleEvent, recipeTasksDescriptor, TaskModuleElement, TaskModuleHandlerProxy} from 'chrome://new-tab-page/lazy_load.js';
import {$$, CrAutoImgElement} from 'chrome://new-tab-page/new_tab_page.js';
import {TaskModuleHandlerRemote} from 'chrome://new-tab-page/task_module.mojom-webui.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {eventToPromise, flushTasks} from 'chrome://webui-test/test_util.js';

import {installMock} from '../../test_support.js';

suite('NewTabPageModulesTaskModuleTest', () => {
  let handler: TestBrowserProxy;

  setup(() => {
    document.body.innerHTML = '';

    handler =
        installMock(TaskModuleHandlerRemote, TaskModuleHandlerProxy.setHandler);
  });

  test('creates no module if no task', async () => {
    // Arrange.
    handler.setResultFor('getPrimaryTask', Promise.resolve({task: null}));

    // Act.
    const moduleElement =
        await recipeTasksDescriptor.initialize(0) as TaskModuleElement;

    // Assert.
    assertEquals(1, handler.getCallCount('getPrimaryTask'));
    assertEquals(null, moduleElement);
  });

  test('creates module if task', async () => {
    // Arrange.
    const task = {
      title: 'Hello world',
      taskItems: [
        {
          name: 'foo',
          imageUrl: {url: 'https://foo.com/img.png'},
          siteName: 'Foo Site',
          info: 'foo info',
          targetUrl: {url: 'https://foo.com'},
        },
        {
          name: 'bar',
          imageUrl: {url: 'https://bar.com/img.png'},
          siteName: 'Bar Site',
          info: 'bar info',
          targetUrl: {url: 'https://bar.com'},
        },
      ],
      relatedSearches: [
        {
          text: 'baz',
          targetUrl: {url: 'https://baz.com'},
        },
        {
          text: 'blub',
          targetUrl: {url: 'https://blub.com'},
        },
      ],
    };
    handler.setResultFor('getPrimaryTask', Promise.resolve({task}));

    // Act.
    const moduleElement =
        await recipeTasksDescriptor.initialize(0) as TaskModuleElement;
    assertTrue(!!moduleElement);
    document.body.append(moduleElement);
    moduleElement.$.taskItemsRepeat.render();
    moduleElement.$.relatedSearchesRepeat.render();

    // Assert.
    const recipes =
        moduleElement.shadowRoot!.querySelectorAll<HTMLAnchorElement>(
            '.task-item');
    const pills =
        moduleElement.shadowRoot!.querySelectorAll<HTMLAnchorElement>('.pill');
    assertEquals(1, handler.getCallCount('getPrimaryTask'));
    assertEquals(2, recipes.length);
    assertEquals(2, pills.length);
    assertEquals('https://foo.com/', recipes[0]!.href);
    assertEquals(
        'https://foo.com/img.png',
        recipes[0]!.querySelector<CrAutoImgElement>('img')!.autoSrc);
    assertEquals(
        'Foo Site',
        recipes[0]!.querySelector<HTMLElement>('.secondary')!.innerText);
    assertEquals(
        'foo info', recipes[0]!.querySelector<HTMLElement>('.tag')!.innerText);
    assertEquals(
        'foo', recipes[0]!.querySelector<HTMLElement>('.name')!.innerText);
    assertEquals('foo', recipes[0]!.querySelector<HTMLElement>('.name')!.title);
    assertEquals('https://bar.com/', recipes[1]!.href);
    assertEquals(
        'https://bar.com/img.png',
        recipes[1]!.querySelector<CrAutoImgElement>('img')!.autoSrc);
    assertEquals(
        'Bar Site',
        recipes[1]!.querySelector<HTMLElement>('.secondary')!.innerText);
    assertEquals(
        'bar info', recipes[1]!.querySelector<HTMLElement>('.tag')!.innerText);
    assertEquals(
        'bar', recipes[1]!.querySelector<HTMLElement>('.name')!.innerText);
    assertEquals('bar', recipes[1]!.querySelector<HTMLElement>('.name')!.title);
    assertEquals('https://baz.com/', pills[0]!.href);
    assertEquals(
        'baz', pills[0]!.querySelector<HTMLElement>('.search-text')!.innerText);
    assertEquals('https://blub.com/', pills[1]!.href);
    assertEquals(
        'blub',
        pills[1]!.querySelector<HTMLElement>('.search-text')!.innerText);
  });

  test('recipes and pills are hidden when cutoff', async () => {
    const repeat = (n: number, fn: () => any) => Array(n).fill(0).map(fn);
    handler.setResultFor('getPrimaryTask', Promise.resolve({
      task: {
        title: 'Hello world',
        taskItems: repeat(20, () => ({
                                name: 'foo',
                                imageUrl: {url: 'https://foo.com/img.png'},
                                siteName: 'Foo Site',
                                targetUrl: {url: 'https://foo.com'},
                              })),
        relatedSearches: repeat(20, () => ({
                                      text: 'baz',
                                      targetUrl: {url: 'https://baz.com'},
                                    })),
      }
    }));
    const moduleElement =
        await recipeTasksDescriptor.initialize(0) as TaskModuleElement;
    assertTrue(!!moduleElement);
    document.body.append(moduleElement);
    moduleElement.$.taskItemsRepeat.render();
    moduleElement.$.relatedSearchesRepeat.render();
    const getElements = () => Array.from(
        moduleElement.shadowRoot!.querySelectorAll<HTMLAnchorElement>(
            '.task-item, .pill'));
    assertEquals(40, getElements().length);
    const hiddenCount = () =>
        getElements().filter(el => el.style.visibility === 'hidden').length;
    const checkHidden = async (width: string, count: number) => {
      const waitForVisibilityUpdate =
          eventToPromise('visibility-update', moduleElement);
      moduleElement.style.width = width;
      await waitForVisibilityUpdate;
      assertEquals(count, hiddenCount());
    };
    await checkHidden('500px', 32);
    await checkHidden('300px', 36);
    await checkHidden('700px', 28);
    await checkHidden('500px', 32);
  });

  test('Backend is notified when module is dismissed or restored', async () => {
    // Arrange.
    const task = {
      title: 'Continue searching for Hello world',
      name: 'Hello world',
      taskItems: [
        {
          name: 'foo',
          imageUrl: {url: 'https://foo.com/img.png'},
          siteName: 'Foo Site',
          targetUrl: {url: 'https://foo.com'},
        },
        {
          name: 'bar',
          imageUrl: {url: 'https://bar.com/img.png'},
          siteName: 'Bar Site',
          targetUrl: {url: 'https://bar.com'},
        },
      ],
      relatedSearches: [
        {
          text: 'baz',
          targetUrl: {url: 'https://baz.com'},
        },
        {
          text: 'blub',
          targetUrl: {url: 'https://blub.com'},
        },
      ],
    };
    handler.setResultFor('getPrimaryTask', Promise.resolve({task}));

    // Arrange.
    const moduleElement =
        await recipeTasksDescriptor.initialize(0) as TaskModuleElement;
    assertTrue(!!moduleElement);
    document.body.append(moduleElement);
    await flushTasks();

    // Act.
    const waitForDismissEvent = eventToPromise('dismiss-module', moduleElement);
    const dismissButton =
        moduleElement.shadowRoot!.querySelector('ntp-module-header')!
            .shadowRoot!.querySelector<HTMLElement>('#dismissButton')!;
    dismissButton.click();
    const dismissEvent: DismissModuleEvent = await waitForDismissEvent;
    const toastMessage = dismissEvent.detail.message;
    const restoreCallback = dismissEvent.detail.restoreCallback;

    // Assert.
    assertEquals('Recipe ideas hidden', toastMessage);
    assertEquals('Hello world', await handler.whenCalled('dismissTask'));

    // Act.
    restoreCallback();

    // Assert.
    assertEquals('Hello world', await handler.whenCalled('restoreTask'));
  });

  test('info button click opens info dialog', async () => {
    // Arrange.
    const task = {
      title: '',
      taskItems: [],
      relatedSearches: [],
    };
    handler.setResultFor('getPrimaryTask', Promise.resolve({task}));
    const moduleElement =
        await recipeTasksDescriptor.initialize(0) as TaskModuleElement;
    assertTrue(!!moduleElement);
    document.body.append(moduleElement);

    // Act.
    ($$(moduleElement, 'ntp-module-header')!
     ).dispatchEvent(new Event('info-button-click'));

    // Assert.
    assertTrue(!!$$(moduleElement, 'ntp-info-dialog'));
  });
});
