(async function(testRunner) {
  let {page, session, dp} = await testRunner.startBlank(
      `Tests that cookies are set, updated and removed.`);

  async function logCookies(success) {
    testRunner.log('Logging Cookies');
    if (success !== undefined)
      testRunner.log('Success: ' + success);
    var data = (await dp.Network.getCookies()).result;
    testRunner.log('Num of cookies ' + data.cookies.length);
    for (var cookie of data.cookies) {
      testRunner.log('  Cookie: ');
      testRunner.log('    Domain: '  + cookie.domain);
      testRunner.log('    Name: '  + cookie.name);
      testRunner.log('    Value: '  + cookie.value);
      testRunner.log('    Path: '  + cookie.path);
      testRunner.log('    HttpOnly: '  + cookie.httpOnly);
      testRunner.log('    Secure: '  + cookie.secure);
      testRunner.log('    Session: '  + cookie.session);
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

  async function deleteAllCookies() {
    testRunner.log('Removing All Cookies');
    var data = (await dp.Network.getCookies()).result;
    var promises = [];
    for (var cookie of data.cookies) {
      var url = 'http://' + cookie.domain + '/' + cookie.path;
      promises.push(dp.Network.deleteCookie({url, cookieName: cookie.name}));
    }
    await Promise.all(promises);
    await logCookies();
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
      await setCookie({url: 'http://127.0.0.1', name: 'foo', value: 'bar4', expirationDate: undefined});
    },

    deleteAllCookies,

    async function nonSessionCookieZeroAdd() {
      await setCookie({url: 'http://127.0.0.1', name: 'foo', value: 'bar5', expirationDate: 0});
    },

    deleteAllCookies,

    async function nonSessionCookieAdd() {
      await setCookie({url: 'http://127.0.0.1', name: 'foo', value: 'bar6', expirationDate: new Date().getTime() + 1000000});
    },

    deleteAllCookies,

    async function differentOriginCookieAdd() {
      // Will result in success but not show up
      await setCookie({url: 'http://example.com', name: 'foo', value: 'bar7'});
    },

    async function invalidCookieAddDomain() {
      await setCookie({url: 'ht2tp://127.0.0.1', name: 'foo', value: 'bar8'});
    },

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
    }
  ]);
})
