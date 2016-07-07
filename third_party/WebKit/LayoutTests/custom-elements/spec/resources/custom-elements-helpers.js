function create_window_in_test(t) {
  let p = new Promise((resolve) => {
    let f = document.createElement('iframe');
    f.srcdoc = '';
    f.onload = (event) => {
      let w = f.contentWindow;
      t.add_cleanup(() => f.remove());
      resolve(w);
    };
    document.body.appendChild(f);
  });
  return p;
}

function test_with_window(f, name) {
  promise_test((t) => {
    return create_window_in_test(t)
    .then((w) => {
      f(w);
    });
  }, name);
}

function assert_throws_dom_exception(global_context, code, func, description) {
  let exception;
  assert_throws(code, () => {
    try {
      func.call(this);
    } catch(e) {
      exception = e;
      throw e;
    }
  }, description);
  assert_true(exception instanceof global_context.DOMException, 'DOMException on the appropriate window');
}