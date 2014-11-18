if (window.testRunner)
    testRunner.dumpAsText();

(function() {
    var portrait = { width: window.innerWidth, height: window.innerHeight };
    var landscape = { width: window.innerHeight, height: window.innerWidth };

    var run = function() {
        window.resizeTo(portrait.width, portrait.height);
        PerfTestRunner.forceLayoutOrFullFrame();
        window.resizeTo(landscape.width, landscape.height);
        PerfTestRunner.forceLayoutOrFullFrame();
    };

    function onTestDone() {
        var logNode = document.getElementById("log");
        logNode.parentNode.removeChild(logNode);
        document.body.innerHTML = "";
        document.body.appendChild(logNode);
    };

    function startTest() {
        PerfTestRunner.measureRunsPerSecond({
            description: "Exercising window resize and following relayout",
            run: run,
            done: onTestDone
        });
    }

    document.addEventListener("DOMContentLoaded", startTest);
})();
