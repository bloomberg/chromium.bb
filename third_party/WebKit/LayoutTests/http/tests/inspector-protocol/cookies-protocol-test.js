(async function(testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Tests that cookies are set, updated and removed.`);

  async function logCookies(success) {
    testRunner.log('Logging Cookies');
    if (success !== undefined)
      testRunner.log('Success: ' + success);
    var data = (await dp.Network.getAllCookies()).result;
    testRunner.log('Num of cookies ' + data.cookies.length);
    data.cookies.sort((a, b) => a.name.localeCompare(b.name));
    for (var cookie of data.cookies) {
      var suffix = ''
      if (cookie.secure)
        suffix += `, secure`;
      if (cookie.httpOnly)
        suffix += `, httpOnly`;
      if (cookie.session)
        suffix += `, session`;
      if (cookie.sameSite)
        suffix += `, ${cookie.sameSite}`;
      testRunner.log(`name: ${cookie.name}, value: ${cookie.value}, domain: ${cookie.domain}, path: ${cookie.path}${suffix}`);
    }
  }

  async function setCookie(cookie) {
    testRunner.log('Setting Cookie');
    var response = await dp.Network.setCookie(cookie);
    await logCookies(response.result.success);
  }

  async function deleteCookie(cookie) {
    testRunner.log('Deleting Cookie');
    await dp.Network.deleteCookie(cookie);
    await logCookies();
  }

  async function setCookies(cookies) {
    testRunner.log('Adding multiple cookies');
    var response = await dp.Network.setCookies({cookies});
    await logCookies();
  }

  async function deleteAllCookies() {
    var data = (await dp.Network.getAllCookies()).result;
    var promises = [];
    for (var cookie of data.cookies) {
      var url = 'http://' + cookie.domain + '/' + cookie.path;
      promises.push(dp.Network.deleteCookie({url, cookieName: cookie.name}));
    }
    await Promise.all(promises);
  }

  testRunner.log('Test started');
  testRunner.log('Enabling network');
  await dp.Network.enable();

  testRunner.runTestSuite([
    async function simpleCookieAdd() {
      await setCookie({url: 'http://127.0.0.1', name: 'foo', value: 'bar1'});
    },

    async function simpleCookieChange() {
      await setCookie({url: 'http://127.0.0.1', name: 'foo', value: 'second bar2'});
    },

    async function anotherSimpleCookieAdd() {
      await setCookie({url: 'http://127.0.0.1', name: 'foo2', value: 'bar1'});
    },

    async function simpleCookieDelete() {
      await deleteCookie({url: 'http://127.0.0.1', cookieName: 'foo'});
    },

    deleteAllCookies,

    async function sessionCookieAdd() {
      await setCookie({url: 'http://127.0.0.1', name: 'foo', value: 'bar4', expires: undefined});
    },

    deleteAllCookies,

    async function nonSessionCookieZeroAdd() {
      await setCookie({url: 'http://127.0.0.1', name: 'foo', value: 'bar5', expires: 0});
    },

    deleteAllCookies,

    async function nonSessionCookieAdd() {
      await setCookie({url: 'http://127.0.0.1', name: 'foo', value: 'bar6', expires: Date.now() + 1000000});
    },

    deleteAllCookies,

    async function differentOriginCookieAdd() {
      // Will result in success but not show up
      await setCookie({url: 'http://example.com', name: 'foo', value: 'bar7'});
    },

    deleteAllCookies,

    async function invalidCookieAddDomain() {
      await setCookie({url: 'ht2tp://127.0.0.1', name: 'foo', value: 'bar8'});
    },

    deleteAllCookies,

    async function invalidCookieAddName() {
      await setCookie({url: 'http://127.0.0.1', name: 'foo\0\r\na', value: 'bar9'});
    },

    deleteAllCookies,

    async function secureCookieAdd() {
      // Will succeed but not be shown because not over https
      await setCookie({url: 'http://127.0.0.1', secure: true, name: 'foo', value: 'bar'});
    },

    deleteAllCookies,

    async function cookieAddHttpOnly() {
      await setCookie({url: 'http://127.0.0.1', httpOnly: true, name: 'foo', value: 'bar'});
    },

    deleteAllCookies,

    async function cookieAddSameSiteLax() {
      await setCookie({url: 'http://127.0.0.1', sameSite: 'Lax', name: 'foo', value: 'bar'});
    },

    deleteAllCookies,

    async function cookieAddSameSiteLax() {
      await setCookie({url: 'http://127.0.0.1', sameSite: 'Strict', name: 'foo', value: 'bar'});
    },

    deleteAllCookies,

    async function setCookiesBasic() {
      await setCookies([{name: 'foo1', value: 'bar1', domain: '127.0.0.1', path: '/', },
                        {name: 'foo2', value: 'bar2', domain: '127.0.0.1', path: '/', httpOnly: true },
                        {name: 'foo3', value: 'bar3', domain: '127.0.0.1', path: '/', secure: true },
                        {name: 'foo4', value: 'bar4', domain: '127.0.0.1', path: '/', sameSite: 'Lax' },
                        {name: 'foo5', value: 'bar5', domain: '127.0.0.1', path: '/' },
                        {name: 'foo6', value: 'bar6', domain: '.chromium.org', path: '/path', expires: Date.now() + 1000 },
                        {name: 'foo7', value: 'bar7', url: 'https://www.chromium.org/foo', expires: Date.now() + 1000 }]);
    },

    deleteAllCookies,

  ]);
})
