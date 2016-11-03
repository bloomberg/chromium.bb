<?php
  header("Suborigin: foo");
?>
<!DOCTYPE html>
<script src="/resources/testharness.js"></script>
<script src="/resources/testharnessreport.js"></script>
<body>
<a id="bar"></a>
<script>
  window.onload = _ => {
    async_test(t => {
      history.pushState({"name": "value"}, "navigation", "foo.html");
      history.pushState({"name": "value"}, "navigation", "bar.html");
      assert_equals(window.location.href, "http://127.0.0.1:8000/security/suborigins/bar.html");

      window.addEventListener("popstate", t.step_func(e => {
        if (e.state) {
          assert_equals(window.location.href, "http://127.0.0.1:8000/security/suborigins/foo.html");
          assert_equals(e.state.name, "value");
          t.done();
        }
      }));

      history.back();
    }, "History state manipulation is allowed in suborigins.");

    async_test(t => {
      var original = window.location.href;
      window.location.hash = "bar";
      assert_equals(window.location.href, original + "#bar");

      window.addEventListener("hashchange", t.step_func_done(e => {
        assert_equals(e.newURL, original + "#bar");
      }));
    }, "Hash navigations are allowed in suborigins.");
  };
</script>
</body>
