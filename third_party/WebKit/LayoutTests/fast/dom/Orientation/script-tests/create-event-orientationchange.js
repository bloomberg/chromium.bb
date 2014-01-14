description('Tests that document.createEvent() works with orientationChange if enabled')

function handleTestResult()
{
    document.getElementById('result').innerHTML = "PASS";
}

if (window.internals && internals.runtimeFlags.orientationEventEnabled) {
    window.addEventListener('orientationchange', handleTestResult, false);

    try {
        var event = document.createEvent("OrientationEvent");
        event.initEvent("orientationchange", false, false);
        window.dispatchEvent(event);
    } catch(e) {
        document.getElementById('result').innerHTML = "FAIL... orientationChange event doesn't appear to be enabled or implemented.";
    }
} else {
    handleTestResult();
}
