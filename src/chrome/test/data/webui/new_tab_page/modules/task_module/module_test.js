// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {shoppingTasksDescriptor, TaskModuleHandlerProxy} from 'chrome://new-tab-page/new_tab_page.js';
import {TestBrowserProxy} from 'chrome://test/test_browser_proxy.m.js';
import {eventToPromise} from 'chrome://test/test_util.m.js';

suite('NewTabPageModulesTaskModuleTest', () => {
  /**
   * @implements {TaskModuleHandlerProxy}
   * @extends {TestBrowserProxy}
   */
  let testProxy;

  setup(() => {
    PolymerTest.clearBody();

    testProxy = TestBrowserProxy.fromClass(TaskModuleHandlerProxy);
    testProxy.handler =
        TestBrowserProxy.fromClass(taskModule.mojom.TaskModuleHandlerRemote);
    TaskModuleHandlerProxy.instance_ = testProxy;
  });

  test('creates no module if no task', async () => {
    // Arrange.
    testProxy.handler.setResultFor(
        'getPrimaryTask', Promise.resolve({task: null}));

    // Act.
    await shoppingTasksDescriptor.initialize();

    // Assert.
    assertEquals(1, testProxy.handler.getCallCount('getPrimaryTask'));
    assertEquals(null, shoppingTasksDescriptor.element);
  });

  test('creates module if task', async () => {
    // Arrange.
    const task = {
      title: 'Hello world',
      taskItems: [
        {
          name: 'foo',
          imageUrl: {url: 'https://foo.com/img.png'},
          price: '1 gazillion dollars',
          info: 'foo info',
          targetUrl: {url: 'https://foo.com'},
        },
        {
          name: 'bar',
          imageUrl: {url: 'https://bar.com/img.png'},
          price: '2 gazillion dollars',
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
    testProxy.handler.setResultFor('getPrimaryTask', Promise.resolve({task}));

    // Act.
    await shoppingTasksDescriptor.initialize();
    const moduleElement = shoppingTasksDescriptor.element;
    document.body.append(moduleElement);
    moduleElement.$.taskItemsRepeat.render();
    moduleElement.$.relatedSearchesRepeat.render();

    // Assert.
    const products =
        Array.from(moduleElement.shadowRoot.querySelectorAll('.task-item'));
    const pills =
        Array.from(moduleElement.shadowRoot.querySelectorAll('.pill'));
    assertEquals(1, testProxy.handler.getCallCount('getPrimaryTask'));
    assertEquals(2, products.length);
    assertEquals(2, pills.length);
    assertEquals('https://foo.com/', products[0].href);
    assertEquals(
        'https://foo.com/img.png', products[0].querySelector('img').autoSrc);
    assertEquals(
        '1 gazillion dollars', products[0].querySelector('.price').innerText);
    assertEquals('foo', products[0].querySelector('.name').innerText);
    assertEquals('foo', products[0].querySelector('.name').title);
    assertEquals('foo info', products[0].querySelector('.info').innerText);
    assertEquals('https://bar.com/', products[1].href);
    assertEquals(
        'https://bar.com/img.png', products[1].querySelector('img').autoSrc);
    assertEquals(
        '2 gazillion dollars', products[1].querySelector('.price').innerText);
    assertEquals('bar', products[1].querySelector('.name').innerText);
    assertEquals('bar', products[1].querySelector('.name').title);
    assertEquals('bar info', products[1].querySelector('.info').innerText);
    assertEquals('https://baz.com/', pills[0].href);
    assertEquals('baz', pills[0].querySelector('.search-text').innerText);
    assertEquals('https://blub.com/', pills[1].href);
    assertEquals('blub', pills[1].querySelector('.search-text').innerText);
  });

  test('products and pills are hidden when cutoff', async () => {
    const repeat = (n, fn) => Array(n).fill(0).map(fn);
    testProxy.handler.setResultFor('getPrimaryTask', Promise.resolve({
      task: {
        title: 'Hello world',
        taskItems: repeat(20, () => ({
                                name: 'foo',
                                imageUrl: {url: 'https://foo.com/img.png'},
                                price: '1 gazillion dollars',
                                info: 'foo info',
                                targetUrl: {url: 'https://foo.com'},
                              })),
        relatedSearches: repeat(20, () => ({
                                      text: 'baz',
                                      targetUrl: {url: 'https://baz.com'},
                                    })),
      }
    }));
    await shoppingTasksDescriptor.initialize();
    const moduleElement = shoppingTasksDescriptor.element;
    document.body.append(moduleElement);
    moduleElement.$.taskItemsRepeat.render();
    moduleElement.$.relatedSearchesRepeat.render();
    const getElements = () => Array.from(
        moduleElement.shadowRoot.querySelectorAll('.task-item, .pill'));
    assertEquals(40, getElements().length);
    const hiddenCount = () =>
        getElements().filter(el => el.style.visibility === 'hidden').length;
    const checkHidden = async (width, count) => {
      const waitForVisibilityUpdate =
          eventToPromise('visibility-update', moduleElement);
      moduleElement.style.width = width;
      await waitForVisibilityUpdate;
      assertEquals(count, hiddenCount());
    };
    await checkHidden('500px', 31);
    await checkHidden('300px', 35);
    await checkHidden('700px', 26);
    await checkHidden('500px', 31);
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
          price: '1 gazillion dollars',
          info: 'foo info',
          targetUrl: {url: 'https://foo.com'},
        },
        {
          name: 'bar',
          imageUrl: {url: 'https://bar.com/img.png'},
          price: '2 gazillion dollars',
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
    testProxy.handler.setResultFor('getPrimaryTask', Promise.resolve({task}));


    // Act.
    await shoppingTasksDescriptor.initialize();

    // Assert.
    assertEquals('function', typeof shoppingTasksDescriptor.actions.dismiss);
    assertEquals('function', typeof shoppingTasksDescriptor.actions.restore);

    // Act.
    const toastMessage = shoppingTasksDescriptor.actions.dismiss();

    // Assert.
    assertEquals('Removed Hello world', toastMessage);
    assertDeepEquals(
        [taskModule.mojom.TaskModuleType.kShopping, 'Hello world'],
        await testProxy.handler.whenCalled('dismissTask'));

    // Act.
    shoppingTasksDescriptor.actions.restore();

    // Assert.
    assertDeepEquals(
        [taskModule.mojom.TaskModuleType.kShopping, 'Hello world'],
        await testProxy.handler.whenCalled('restoreTask'));
  });
});
