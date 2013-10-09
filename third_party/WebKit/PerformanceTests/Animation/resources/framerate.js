(function(){

var framesPerTimerReading = 10;
var frameCount = 0;
var startTime;
var trackingFrameRate = false;

function trackFrameRate(currTime)
{
    if (++frameCount == framesPerTimerReading) {
        frameCount = 0;
        PerfTestRunner.measureValueAsync(1000 * framesPerTimerReading / (currTime - startTime));
        startTime = currTime;
    }

    if (trackingFrameRate)
        requestAnimationFrame(trackFrameRate);
}

window.startTrackingFrameRate = function() {
    if (trackingFrameRate)
        return;
    trackingFrameRate = true;
    startTime = performance.now();
    trackFrameRate();
};

window.stopTrackingFrameRate = function() {
    trackingFrameRate = false;
};

})();
