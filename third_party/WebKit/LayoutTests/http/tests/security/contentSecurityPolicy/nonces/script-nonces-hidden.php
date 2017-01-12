<?php
    header("Content-Security-Policy: script-src 'self' 'nonce-abc'; img-src 'none'");
?>
<!doctype html>
<script src="/resources/testharness.js"></script>
<script src="/resources/testharnessreport.js"></script>
<body>
<!-- Basics -->
<script nonce="abc">
    test(t => {
      assert_equals(document.querySelector('[nonce=abc]'), null);
      assert_equals(document.currentScript.getAttribute('nonce'), '[Replaced]');
      assert_equals(document.currentScript.nonce, 'abc');
    }, "Reading 'nonce' content attribute and IDL attribute.");

    test(t => {
      document.currentScript.setAttribute('nonce', 'xyz');
      assert_equals(document.currentScript.getAttribute('nonce'), '[Replaced]');
      assert_equals(document.currentScript.nonce, 'xyz');
    }, "Writing 'nonce' content attribute.");

    test(t => {
      assert_equals(document.currentScript.nonce, 'xyz');
      document.currentScript.nonce = 'foo';
      assert_equals(document.currentScript.nonce, 'foo');
    }, "Writing 'nonce' DOM attribute.");

    async_test(t => {
      var script = document.currentScript;
      assert_equals(script.nonce, 'foo');

      setTimeout(_ => {
        assert_equals(script.nonce, "");
        t.done();
      }, 1);
    }, "'nonce' DOM attribute cleared after current task.");
</script>

<!-- CSS Leakage -->
<style>
  #test { display: block; }
  #test[nonce=abc] { background: url(/security/resources/abe.png); }
</style>
<script nonce="abc">
    var css_test = async_test(t => {
      document.addEventListener('securitypolicyviolation', e => {
        assert_unreached("No image should be requested via CSS.");
      });
    }, "Nonces don't leak via CSS side-channels.");
</script>
<script id="test" nonce="abc">
  window.onload = e => {
    css_test.done();
  };
</script>
