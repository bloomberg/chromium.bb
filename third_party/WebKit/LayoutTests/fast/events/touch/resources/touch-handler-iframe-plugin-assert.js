description("This test passes if it doesn't trigger an ASSERT - crbug.com/254203");

window.jsTestIsAsync = true;

if (window.internals) {
    window.internals.settings.setForceCompositingMode(true);
}

function onDone()
{
  finishJSTest();
}

var frameElem = document.getElementById('frame');
frameElem.addEventListener('load', function() {
    frameElem.src = 'about:blank';
    window.setTimeout(onDone, 0);
});
