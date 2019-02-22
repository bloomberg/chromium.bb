importScripts('/resources/testharness.js',
              '/resources/origin-trials-helper.js');

test(t => {
  OriginTrialsHelper.check_interfaces_missing(
    self,
    ['CookieStore', 'ExtendableCookieChangeEvent']);
}, 'Cookie Store API interfaces in Origin-Trial disabled worker.');

test(t => {
  assert_false('cookieStore' in self,
               'cookieStore property does not exist on global scope');
  assert_false('oncookiechange' in self,
               'oncookiechange property does not exist on global scope');
}, 'Cookie Store API entry points in Origin-Trial disabled worker.');

done();
