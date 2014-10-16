(function(){

var framesPerTimerReading = 10;
var frameCount = 0;
var startTime;
var trackingFrameRate = false;
var currentTest;

function trackFrameRate(currTime)
{
    if (++frameCount == framesPerTimerReading) {
        frameCount = 0;
        PerfTestRunner.measureValueAsync(1000 * framesPerTimerReading / (currTime - startTime));
        startTime = currTime;
    }

    if (currentTest && currentTest.run)
        currentTest.run();

    if (trackingFrameRate)
        requestAnimationFrame(trackFrameRate);
}

window.startTrackingFrameRate = function(test) {
    if (trackingFrameRate)
        return;
    trackingFrameRate = true;
    currentTest = test;
    startTime = performance.now();
    trackFrameRate();
};

window.stopTrackingFrameRate = function() {
    trackingFrameRate = false;
    currentTest = undefined;
};

})();
