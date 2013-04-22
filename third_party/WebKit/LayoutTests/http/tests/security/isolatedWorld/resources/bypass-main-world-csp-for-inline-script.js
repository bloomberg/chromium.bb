if (window.testRunner) {
    testRunner.dumpAsText();
    testRunner.waitUntilDone();
}

tests = 4;
window.addEventListener("message", function(message) {
    tests -= 1;
    test();
}, false);

function test() {
    function injectInlineScript(isolated) {
        var script = document.createElement('script');
        script.innerText = "alert('EXECUTED in " + (isolated ? "isolated world" : "main world") + ".');";
        document.body.appendChild(script);
        window.postMessage("next", "*");
    }

    switch (tests) {
        case 4:
            alert("Injecting in main world: this should fail.");
            injectInlineScript(false);
            break;
        case 3:
            alert("Injecting into isolated world without bypass: this should fail.");
            testRunner.evaluateScriptInIsolatedWorld(1, String(eval("injectInlineScript")) + "\ninjectInlineScript(true);");
            break;
        case 2:
            alert("Starting to bypass main world's CSP: this should pass!");
            testRunner.setIsolatedWorldContentSecurityPolicy(1, 'script-src \'unsafe-inline\' *');
            testRunner.evaluateScriptInIsolatedWorld(1, String(eval("injectInlineScript")) + "\ninjectInlineScript(true);");
            break;
        case 1:
            alert("Injecting into main world again: this should fail.");
            injectInlineScript(false);
            break;
        case 0:
            testRunner.setIsolatedWorldContentSecurityPolicy(1, '');
            testRunner.notifyDone();
            break;
    }
}

document.addEventListener('DOMContentLoaded', test);

setTimeout(testRunner.notifyDone.bind(testRunner), 2000);
