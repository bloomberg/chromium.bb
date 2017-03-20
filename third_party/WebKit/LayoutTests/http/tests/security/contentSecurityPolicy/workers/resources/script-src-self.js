importScripts("http://127.0.0.1:8000/resources/testharness.js");
importScripts("http://127.0.0.1:8000/security/contentSecurityPolicy/resources/testharness-helper.js");

test(t => {
  // TODO(mkwst): The error event isn't firing. :/

  assert_throws(EvalError(),
                function () { eval("1 + 1"); },
                "`eval()` should throw 'EvalError'.");

  assert_throws(EvalError(),
                function () { var x = new Function("1 + 1"); },
                "`new Function()` should throw 'EvalError'.");
}, "`eval()` blocked in " + self.location.protocol);

async_test(t => {
  waitUntilCSPEventForEval(t, 21)
    .then(_ => t.done());

  assert_equals(
      setTimeout("assert_unreached('setTimeout([string]) should not execute.')", 0),
      0);
}, "`setTimeout([string])` blocked in " + self.location.protocol);

done();
