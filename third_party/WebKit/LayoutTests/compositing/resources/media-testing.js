
if (window.testRunner)
    testRunner.waitUntilDone();

function setupVideo(videoElement, videoPath, canPlayThroughCallback)
{
    var mediaFile = findMediaFile("video", videoPath);
    videoElement.addEventListener("canplaythrough", canPlayThroughCallback);
    videoElement.src = mediaFile;
}
