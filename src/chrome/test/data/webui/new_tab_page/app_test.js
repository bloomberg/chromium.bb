// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {$$, BackgroundManager, BackgroundSelectionType, CustomizeDialogPage, ModuleRegistry, NewTabPageProxy, NtpElement, PromoBrowserCommandProxy, VoiceAction, WindowProxy} from 'chrome://new-tab-page/new_tab_page.js';
import {isMac} from 'chrome://resources/js/cr.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.m.js';
import {fakeMetricsPrivate, MetricsTracker} from 'chrome://test/new_tab_page/metrics_test_support.js';
import {assertNotStyle, assertStyle, createTheme} from 'chrome://test/new_tab_page/test_support.js';
import {TestBrowserProxy} from 'chrome://test/test_browser_proxy.m.js';
import {eventToPromise, flushTasks} from 'chrome://test/test_util.m.js';

suite('NewTabPageAppTest', () => {
  /** @type {!AppElement} */
  let app;

  /**
   * @implements {WindowProxy}
   * @extends {TestBrowserProxy}
   */
  let windowProxy;

  /**
   * @implements {newTabPage.mojom.PageHandlerRemote}
   * @extends {TestBrowserProxy}
   */
  let handler;

  /** @type {newTabPage.mojom.PageHandlerRemote} */
  let callbackRouterRemote;

  /** @type {MetricsTracker} */
  let metrics;

  /**
   * @implements {ModuleRegistry}
   * @extends {TestBrowserProxy}
   */
  let moduleRegistry;

  /**
   * @implements {BackgroundManager}
   * @extends {TestBrowserProxy}
   */
  let backgroundManager;

  /** @type {PromiseResolver} */
  let moduleResolver;

  setup(async () => {
    PolymerTest.clearBody();

    windowProxy = TestBrowserProxy.fromClass(WindowProxy);
    handler = TestBrowserProxy.fromClass(newTabPage.mojom.PageHandlerRemote);
    handler.setResultFor('getBackgroundCollections', Promise.resolve({
      collections: [],
    }));
    handler.setResultFor('getDoodle', Promise.resolve({
      doodle: null,
    }));
    handler.setResultFor('getPromo', Promise.resolve({promo: null}));
    windowProxy.setResultMapperFor('matchMedia', () => ({
                                                   addListener() {},
                                                   removeListener() {},
                                                 }));
    windowProxy.setResultFor('waitForLazyRender', Promise.resolve());
    windowProxy.setResultFor('createIframeSrc', '');
    WindowProxy.setInstance(windowProxy);
    const callbackRouter = new newTabPage.mojom.PageCallbackRouter();
    NewTabPageProxy.setInstance(handler, callbackRouter);
    callbackRouterRemote = callbackRouter.$.bindNewPipeAndPassRemote();
    backgroundManager = TestBrowserProxy.fromClass(BackgroundManager);
    backgroundManager.setResultFor(
        'getBackgroundImageLoadTime', Promise.resolve(0));
    BackgroundManager.setInstance(backgroundManager);
    moduleRegistry = TestBrowserProxy.fromClass(ModuleRegistry);
    moduleResolver = new PromiseResolver();
    moduleRegistry.setResultFor('getDescriptors', []);
    moduleRegistry.setResultFor('initializeModules', moduleResolver.promise);
    ModuleRegistry.setInstance(moduleRegistry);
    metrics = fakeMetricsPrivate();

    app = document.createElement('ntp-app');
    document.body.appendChild(app);
    await flushTasks();
  });

  test('customize dialog closed on start', () => {
    // Assert.
    assertFalse(!!app.shadowRoot.querySelector('ntp-customize-dialog'));
  });

  test('clicking customize button opens customize dialog', async () => {
    // Act.
    $$(app, '#customizeButton').click();
    await flushTasks();

    // Assert.
    assertTrue(!!app.shadowRoot.querySelector('ntp-customize-dialog'));
  });

  test('setting theme updates customize dialog', async () => {
    // Arrange.
    $$(app, '#customizeButton').click();
    const theme = createTheme();

    // Act.
    callbackRouterRemote.setTheme(theme);
    await callbackRouterRemote.$.flushForTesting();

    // Assert.
    assertDeepEquals(
        theme, app.shadowRoot.querySelector('ntp-customize-dialog').theme);
  });

  test('setting theme updates ntp', async () => {
    // Act.
    callbackRouterRemote.setTheme(createTheme());
    await callbackRouterRemote.$.flushForTesting();

    // Assert.
    assertEquals(1, backgroundManager.getCallCount('setBackgroundColor'));
    assertEquals(
        0xffff0000 /* red */,
        (await backgroundManager.whenCalled('setBackgroundColor')).value);
    assertStyle(
        $$(app, '#content'), '--ntp-theme-shortcut-background-color',
        'rgba(0, 255, 0, 1)');
    assertStyle(
        $$(app, '#content'), '--ntp-theme-text-color', 'rgba(0, 0, 255, 1)');
    assertEquals(1, backgroundManager.getCallCount('setShowBackgroundImage'));
    assertFalse(await backgroundManager.whenCalled('setShowBackgroundImage'));
    assertStyle($$(app, '#backgroundImageAttribution'), 'display', 'none');
    assertStyle($$(app, '#backgroundImageAttribution2'), 'display', 'none');
    assertFalse($$(app, '#logo').singleColored);
    assertFalse($$(app, '#logo').dark);
    assertEquals(0xffff0000, $$(app, '#logo').backgroundColor.value);
  });

  test('setting 3p theme shows attribution', async () => {
    // Arrange.
    const theme = createTheme();
    theme.backgroundImage = {
      url: {url: 'https://foo.com'},
      attributionUrl: {url: 'chrome://theme/foo'},
    };

    // Act.
    callbackRouterRemote.setTheme(theme);
    await callbackRouterRemote.$.flushForTesting();

    assertNotStyle($$(app, '#themeAttribution'), 'display', 'none');
    assertEquals('chrome://theme/foo', $$(app, '#themeAttribution img').src);
  });

  test('realbox is not visible by default', async () => {
    // Assert.
    assertStyle($$(app, '#realbox'), 'visibility', 'hidden');

    // Act.
    callbackRouterRemote.setTheme(createTheme());
    await callbackRouterRemote.$.flushForTesting();

    // Assert.
    assertStyle($$(app, '#realbox'), 'visibility', 'visible');
  });

  test('open voice search event opens voice search overlay', async () => {
    // Act.
    $$(app, '#realbox').dispatchEvent(new Event('open-voice-search'));
    await flushTasks();

    // Assert.
    assertTrue(!!app.shadowRoot.querySelector('ntp-voice-search-overlay'));
    assertEquals(1, metrics.count('NewTabPage.VoiceActions'));
    assertEquals(
        1,
        metrics.count(
            'NewTabPage.VoiceActions', VoiceAction.kActivateSearchBox));
  });

  test('voice search keyboard shortcut', async () => {
    // Test correct shortcut opens voice search.
    // Act.
    window.dispatchEvent(new KeyboardEvent('keydown', {
      ctrlKey: true,
      shiftKey: true,
      code: 'Period',
    }));
    await flushTasks();

    // Assert.
    assertTrue(!!app.shadowRoot.querySelector('ntp-voice-search-overlay'));
    assertEquals(1, metrics.count('NewTabPage.VoiceActions'));
    assertEquals(
        1,
        metrics.count(
            'NewTabPage.VoiceActions', VoiceAction.kActivateKeyboard));

    // Test other shortcut doesn't close voice search.
    // Act
    window.dispatchEvent(new KeyboardEvent('keydown', {
      ctrlKey: true,
      shiftKey: true,
      code: 'Enter',
    }));
    await flushTasks();

    // Assert.
    assertTrue(!!app.shadowRoot.querySelector('ntp-voice-search-overlay'));
  });

  if (isMac) {
    test('keyboard shortcut opens voice search overlay on mac', async () => {
      // Act.
      window.dispatchEvent(new KeyboardEvent('keydown', {
        metaKey: true,
        shiftKey: true,
        code: 'Period',
      }));
      await flushTasks();

      // Assert.
      assertTrue(!!app.shadowRoot.querySelector('ntp-voice-search-overlay'));
    });
  }

  test('setting background image shows image', async () => {
    // Arrange.
    const theme = createTheme();
    theme.backgroundImage = {url: {url: 'https://img.png'}};

    // Act.
    backgroundManager.resetResolver('setShowBackgroundImage');
    callbackRouterRemote.setTheme(theme);
    await callbackRouterRemote.$.flushForTesting();

    // Assert.
    assertEquals(1, backgroundManager.getCallCount('setShowBackgroundImage'));
    assertTrue(await backgroundManager.whenCalled('setShowBackgroundImage'));
    assertNotStyle(
        $$(app, '#backgroundImageAttribution'), 'text-shadow', 'none');
    assertEquals(1, backgroundManager.getCallCount('setBackgroundImage'));
    assertEquals(
        'https://img.png',
        (await backgroundManager.whenCalled('setBackgroundImage')).url.url);
    assertEquals(null, $$(app, '#logo').backgroundColor);
  });

  test('setting attributions shows attributions', async function() {
    // Arrange.
    const theme = createTheme();
    theme.backgroundImageAttribution1 = 'foo';
    theme.backgroundImageAttribution2 = 'bar';
    theme.backgroundImageAttributionUrl = {url: 'https://info.com'};

    // Act.
    callbackRouterRemote.setTheme(theme);
    await callbackRouterRemote.$.flushForTesting();

    // Assert.
    assertNotStyle($$(app, '#backgroundImageAttribution'), 'display', 'none');
    assertNotStyle($$(app, '#backgroundImageAttribution2'), 'display', 'none');
    assertEquals(
        'https://info.com',
        $$(app, '#backgroundImageAttribution').getAttribute('href'));
    assertEquals(
        'foo', $$(app, '#backgroundImageAttribution1').textContent.trim());
    assertEquals(
        'bar', $$(app, '#backgroundImageAttribution2').textContent.trim());
  });

  test('setting logo color colors logo', async function() {
    // Arrange.
    const theme = createTheme();
    theme.logoColor = {value: 0xffff0000};

    // Act.
    callbackRouterRemote.setTheme(theme);
    await callbackRouterRemote.$.flushForTesting();

    // Assert.
    assertTrue($$(app, '#logo').singleColored);
    assertStyle($$(app, '#logo'), '--ntp-logo-color', 'rgba(255, 0, 0, 1)');
  });

  test('preview background image', async () => {
    const theme = createTheme();
    theme.backgroundImage = {url: {url: 'https://example.com/image.png'}};
    theme.backgroundImageAttribution1 = 'foo';
    theme.backgroundImageAttribution2 = 'bar';
    theme.backgroundImageAttributionUrl = {url: 'https://info.com'};
    callbackRouterRemote.setTheme(theme);
    await callbackRouterRemote.$.flushForTesting();
    assertEquals(backgroundManager.getCallCount('setBackgroundImage'), 1);
    assertEquals(
        'https://example.com/image.png',
        (await backgroundManager.whenCalled('setBackgroundImage')).url.url);
    assertEquals(
        'https://info.com/', $$(app, '#backgroundImageAttribution').href);
    assertEquals('foo', $$(app, '#backgroundImageAttribution1').innerText);
    assertEquals('bar', $$(app, '#backgroundImageAttribution2').innerText);
    $$(app, '#customizeButton').click();
    await flushTasks();
    const dialog = app.shadowRoot.querySelector('ntp-customize-dialog');
    backgroundManager.resetResolver('setBackgroundImage');
    dialog.backgroundSelection = {
      type: BackgroundSelectionType.IMAGE,
      image: {
        attribution1: '1',
        attribution2: '2',
        attributionUrl: {url: 'https://example.com'},
        imageUrl: {url: 'https://example.com/other.png'},
      },
    };
    assertEquals(1, backgroundManager.getCallCount('setBackgroundImage'));
    assertEquals(
        'https://example.com/other.png',
        (await backgroundManager.whenCalled('setBackgroundImage')).url.url);
    assertEquals(
        'https://example.com/', $$(app, '#backgroundImageAttribution').href);
    assertEquals('1', $$(app, '#backgroundImageAttribution1').innerText);
    assertEquals('2', $$(app, '#backgroundImageAttribution2').innerText);
    assertFalse($$(app, '#backgroundImageAttribution2').hidden);

    backgroundManager.resetResolver('setBackgroundImage');
    dialog.backgroundSelection = {type: BackgroundSelectionType.NO_SELECTION};
    assertEquals(1, backgroundManager.getCallCount('setBackgroundImage'));
    assertEquals(
        'https://example.com/image.png',
        (await backgroundManager.whenCalled('setBackgroundImage')).url.url);
  });

  test('theme update when dialog closed clears selection', async () => {
    const theme = createTheme();
    theme.backgroundImage = {url: {url: 'https://example.com/image.png'}};
    callbackRouterRemote.setTheme(theme);
    await callbackRouterRemote.$.flushForTesting();
    assertEquals(1, backgroundManager.getCallCount('setBackgroundImage'));
    assertEquals(
        'https://example.com/image.png',
        (await backgroundManager.whenCalled('setBackgroundImage')).url.url);
    $$(app, '#customizeButton').click();
    await flushTasks();
    const dialog = app.shadowRoot.querySelector('ntp-customize-dialog');
    backgroundManager.resetResolver('setBackgroundImage');
    dialog.backgroundSelection = {
      type: BackgroundSelectionType.IMAGE,
      image: {
        attribution1: '1',
        attribution2: '2',
        attributionUrl: {url: 'https://example.com'},
        imageUrl: {url: 'https://example.com/other.png'},
      },
    };
    assertEquals(1, backgroundManager.getCallCount('setBackgroundImage'));
    assertEquals(
        'https://example.com/other.png',
        (await backgroundManager.whenCalled('setBackgroundImage')).url.url);
    backgroundManager.resetResolver('setBackgroundImage');
    dialog.dispatchEvent(new Event('close'));
    assertEquals(0, backgroundManager.getCallCount('setBackgroundImage'));
    backgroundManager.resetResolver('setBackgroundImage');
    callbackRouterRemote.setTheme(theme);
    await callbackRouterRemote.$.flushForTesting();
    assertEquals(2, backgroundManager.getCallCount('setBackgroundImage'));
    assertEquals(
        'https://example.com/image.png',
        (await backgroundManager.whenCalled('setBackgroundImage')).url.url);
  });

  test('theme updates add shortcut color', async () => {
    const theme = createTheme();
    theme.shortcutUseWhiteAddIcon = true;
    callbackRouterRemote.setTheme(theme);
    const mostVisited = $$(app, '#mostVisited');
    assertFalse(mostVisited.hasAttribute('use-white-add-icon'));
    await callbackRouterRemote.$.flushForTesting();
    assertTrue(mostVisited.hasAttribute('use-white-add-icon'));
  });

  test('theme updates use title pill', async () => {
    const theme = createTheme();
    theme.shortcutUseTitlePill = true;
    callbackRouterRemote.setTheme(theme);
    const mostVisited = $$(app, '#mostVisited');
    assertFalse(mostVisited.hasAttribute('use-title-pill'));
    await callbackRouterRemote.$.flushForTesting();
    assertTrue(mostVisited.hasAttribute('use-title-pill'));
  });

  test('can show promo with browser command', async () => {
    const promoBrowserCommandHandler = TestBrowserProxy.fromClass(
        promoBrowserCommand.mojom.CommandHandlerRemote);
    PromoBrowserCommandProxy.getInstance().handler = promoBrowserCommandHandler;
    promoBrowserCommandHandler.setResultFor(
        'canShowPromoWithCommand', Promise.resolve({canShow: true}));

    const commandId = 123;  // Unsupported command.
    window.dispatchEvent(new MessageEvent('message', {
      data: {
        frameType: 'one-google-bar',
        messageType: 'can-show-promo-with-browser-command',
        commandId,
      },
      source: window,
      origin: window.origin,
    }));

    // Make sure the command is sent to the browser.
    const expectedCommandId =
        await promoBrowserCommandHandler.whenCalled('canShowPromoWithCommand');
    // Unsupported commands get resolved to the default command before being
    // sent to the browser.
    assertEquals(
        promoBrowserCommand.mojom.Command.kUnknownCommand, expectedCommandId);

    // Make sure the promo frame gets notified whether the promo can be shown.
    const {data} = await eventToPromise('message', window);
    assertEquals('can-show-promo-with-browser-command', data.messageType);
    assertTrue(data[commandId]);
  });

  test('executes promo browser command', async () => {
    const promoBrowserCommandHandler = TestBrowserProxy.fromClass(
        promoBrowserCommand.mojom.CommandHandlerRemote);
    PromoBrowserCommandProxy.getInstance().handler = promoBrowserCommandHandler;
    promoBrowserCommandHandler.setResultFor(
        'executeCommand', Promise.resolve({commandExecuted: true}));

    const commandId = 123;  // Unsupported command.
    const clickInfo = {middleButton: true};
    window.dispatchEvent(new MessageEvent('message', {
      data: {
        frameType: 'one-google-bar',
        messageType: 'execute-browser-command',
        data: {
          commandId,
          clickInfo,
        },
      },
      source: window,
      origin: window.origin,
    }));

    // Make sure the command and click information are sent to the browser.
    const [expectedCommandId, expectedClickInfo] =
        await promoBrowserCommandHandler.whenCalled('executeCommand');
    // Unsupported commands get resolved to the default command before being
    // sent to the browser.
    assertEquals(
        promoBrowserCommand.mojom.Command.kUnknownCommand, expectedCommandId);
    assertEquals(clickInfo, expectedClickInfo);

    // Make sure the promo frame gets notified whether the command was executed.
    const {data: commandExecuted} = await eventToPromise('message', window);
    assertTrue(commandExecuted);
  });

  suite('click recording', () => {
    suiteSetup(() => {
      loadTimeData.overrideValues({
        modulesEnabled: true,
      });
    });

    [['#content', NtpElement.kBackground],
     ['ntp-logo', NtpElement.kLogo],
     ['ntp-realbox', NtpElement.kRealbox],
     ['ntp-most-visited', NtpElement.kMostVisited],
     ['ntp-middle-slot-promo', NtpElement.kMiddleSlotPromo],
     ['ntp-modules', NtpElement.kModule],
    ].forEach(([selector, element]) => {
      test(`clicking '${selector}' records click`, () => {
        // Act.
        $$(app, selector).click();

        // Assert.
        assertEquals(1, metrics.count('NewTabPage.Click'));
        assertEquals(1, metrics.count('NewTabPage.Click', element));
      });
    });

    test('clicking customize records click', () => {
      // Act.
      $$(app, '#customizeButton').click();
      $$(app, '#customizeDialogIf').render();
      $$(app, 'ntp-customize-dialog').click();

      // Assert.
      assertEquals(2, metrics.count('NewTabPage.Click'));
      assertEquals(2, metrics.count('NewTabPage.Click', NtpElement.kCustomize));
    });

    test('clicking OGB records click', () => {
      // Act.
      window.dispatchEvent(new MessageEvent('message', {
        data: {
          frameType: 'one-google-bar',
          messageType: 'click',
        }
      }));

      // Assert.
      assertEquals(1, metrics.count('NewTabPage.Click'));
      assertEquals(
          1, metrics.count('NewTabPage.Click', NtpElement.kOneGoogleBar));
    });
  });

  suite('modules', () => {
    suiteSetup(() => {
      loadTimeData.overrideValues({
        modulesEnabled: true,
      });
    });

    test('modules can open customize dialog', async () => {
      // Act.
      $$(app, 'ntp-modules').dispatchEvent(new Event('customize-module'));
      $$(app, '#customizeDialogIf').render();

      // Assert.
      assertTrue(!!$$(app, 'ntp-customize-dialog'));
      assertEquals(
          CustomizeDialogPage.MODULES,
          $$(app, 'ntp-customize-dialog').selectedPage);
    });

    test('promo and modules coordinate', async () => {
      // Arrange.
      loadTimeData.overrideValues({navigationStartTime: 0.0});
      windowProxy.setResultFor('now', 123.0);
      const middleSlotPromo = $$(app, 'ntp-middle-slot-promo');
      const modules = $$(app, 'ntp-modules');

      // Assert.
      assertStyle(middleSlotPromo, 'display', 'none');
      assertStyle(modules, 'display', 'none');

      // Act.
      middleSlotPromo.dispatchEvent(new Event('ntp-middle-slot-promo-loaded'));

      // Assert.
      assertStyle(middleSlotPromo, 'display', 'none');
      assertStyle(modules, 'display', 'none');

      // Act.
      modules.dispatchEvent(new Event('modules-loaded'));

      // Assert.
      assertNotStyle(middleSlotPromo, 'display', 'none');
      assertNotStyle(modules, 'display', 'none');
      assertEquals(1, metrics.count('NewTabPage.Modules.ShownTime'));
      assertEquals(1, metrics.count('NewTabPage.Modules.ShownTime', 123));
    });
  });

  suite('counterfactual modules', () => {
    suiteSetup(() => {
      loadTimeData.overrideValues({
        modulesEnabled: false,
        modulesLoadEnabled: true,
      });
    });

    test('modules loaded but not rendered if counterfactual', async () => {
      // Act.
      moduleResolver.resolve([
        {
          descriptor: {id: 'foo'},
          element: document.createElement('div'),
        },
        {
          descriptor: {id: 'bar'},
          element: document.createElement('div'),
        }
      ]);
      await flushTasks();

      // Assert.
      assertEquals(1, moduleRegistry.getCallCount('initializeModules'));
      assertEquals(1, handler.getCallCount('onModulesLoadedWithData'));
      assertEquals(
          0, app.shadowRoot.querySelectorAll('ntp-module-wrapper').length);
    });
  });
});
