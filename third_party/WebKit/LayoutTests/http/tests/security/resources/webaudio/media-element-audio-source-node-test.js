// Test MediaStreamAudioSourceNode's with different URLs.
//
var context = 0;
var lengthInSeconds = 1;
var sampleRate = 44100;
var source = 0;
var audio = 0;

// Create an MediaElementSource node with the given |url| and connect it to webaudio.
// |oncomplete| is given the completion event to check the result.
function runTest (url, oncomplete, tester)
{
    if (window.testRunner) {
        testRunner.dumpAsText();
        testRunner.waitUntilDone();
    }

    window.jsTestIsAsync = true;

    context = new OfflineAudioContext(1, sampleRate * lengthInSeconds, sampleRate);

    audio = document.createElement('audio');

    if (tester) {
        tester();
    } else {
        audio.src = url;
    }

    source = context.createMediaElementSource(audio);
    source.connect(context.destination);

    audio.addEventListener("playing", function(e) {
            context.startRendering();
        });

    context.oncomplete = function(e) {
        checkResult(e);
        finishJSTest();
    }

    audio.play();
}
