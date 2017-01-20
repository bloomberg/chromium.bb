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
    }, "HTML: Reading 'nonce' content attribute and IDL attribute.");

    test(t => {
      document.currentScript.setAttribute('nonce', 'xyz');
      assert_equals(document.currentScript.getAttribute('nonce'), '[Replaced]');
      assert_equals(document.currentScript.nonce, 'xyz');
    }, "HTML: Writing 'nonce' content attribute.");

    test(t => {
      assert_equals(document.currentScript.nonce, 'xyz');
      document.currentScript.nonce = 'foo';
      assert_equals(document.currentScript.nonce, 'foo');
      assert_equals(document.currentScript.getAttribute('nonce'), '[Replaced]');
    }, "HTML: Writing 'nonce' DOM attribute.");

    async_test(t => {
      var script = document.currentScript;
      assert_equals(script.nonce, 'foo');

      setTimeout(t.step_func_done(_ => {
        assert_equals(script.nonce, "foo");
      }), 1);
    }, "HTML: 'nonce' DOM attribute present after current task.");
</script>

<!-- SVGScriptElement -->
<svg xmlns="http://www.w3.org/2000/svg">
  <script nonce="abc">
    test(t => {
      assert_equals(document.querySelector('[nonce=abc]'), null);
      assert_equals(document.currentScript.getAttribute('nonce'), '[Replaced]');
      assert_equals(document.currentScript.nonce, 'abc');
    }, "SVG: Reading 'nonce' content attribute and IDL attribute.");

    test(t => {
      document.currentScript.setAttribute('nonce', 'xyz');
      assert_equals(document.currentScript.getAttribute('nonce'), '[Replaced]');
      assert_equals(document.currentScript.nonce, 'xyz');
    }, "SVG: Writing 'nonce' content attribute.");

    test(t => {
      assert_equals(document.currentScript.nonce, 'xyz');
      document.currentScript.nonce = 'foo';
      assert_equals(document.currentScript.nonce, 'foo');
      assert_equals(document.currentScript.getAttribute('nonce'), '[Replaced]');
    }, "SVG: Writing 'nonce' DOM attribute.");

    async_test(t => {
      var script = document.currentScript;
      assert_equals(script.nonce, 'foo');

      setTimeout(t.step_func_done(_ => {
        assert_equals(script.nonce, "foo");
      }), 1);
    }, "SVG: 'nonce' DOM attribute present after current task.");
  </script>
</svg>

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
