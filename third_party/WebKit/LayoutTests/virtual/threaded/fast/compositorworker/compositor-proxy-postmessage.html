<!DOCTYPE html>
<div id='container'></div>
<script src="../../../../resources/testharness.js"></script>
<script src="../../../../resources/testharnessreport.js"></script>
<script>

var test = async_test("This test checks that an element's compositor proxy can be sent from the main thread to the compositor thread and back again.");

var lastAttribute = '';
function checkResponse(proxy, opacity, transform) {
    assert_equals(typeof proxy, 'object');
    assert_true(opacity || transform);
    assert_not_equals(opacity, transform);
    var currentAttribute = opacity ? 'opacity' : 'transform';
    if (lastAttribute == '') {
        lastAttribute = currentAttribute;
    } else {
        assert_not_equals(lastAttribute, currentAttribute);
        test.done();
    }
}

function processMessage(msg) {
    var message = msg.data;
    checkResponse(message.proxy, message.opacity, message.transform);
}

var first = new CompositorWorker('resources/proxy-echo.js');
first.onmessage = processMessage;
var proxy = new CompositorProxy(document.getElementById('container'), ['opacity']);
assert_true(proxy.supports('opacity'));
assert_false(proxy.supports('transform'));
assert_false(proxy.supports('scrollTop'));
first.postMessage(proxy);

var second = new CompositorWorker('resources/proxy-echo.js');
second.onmessage = processMessage;
proxy = new CompositorProxy(document.getElementById('container'), ['transform']);
assert_true(proxy.supports('transform'));
assert_false(proxy.supports('opacity'));
assert_false(proxy.supports('scrollTop'));
second.postMessage(proxy);

</script>
