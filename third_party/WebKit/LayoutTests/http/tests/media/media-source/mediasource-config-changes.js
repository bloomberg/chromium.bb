function fetchManifestAndData(test, manifestFilename, callback)
{
    var baseURL = '/media/resources/media-source/';
    var manifestURL = baseURL + manifestFilename;
    MediaSourceUtil.loadTextData(test, manifestURL, function(manifestText)
    {
        var manifest = JSON.parse(manifestText);

        assert_true(MediaSource.isTypeSupported(manifest.type), manifest.type + " is supported.");

        var mediaURL = baseURL + manifest.url;
        MediaSourceUtil.loadBinaryData(test, mediaURL, function(mediaData)
        {
            callback(manifest.type, mediaData);
        });
    });
}

function appendBuffer(test, sourceBuffer, data)
{
    test.expectEvent(sourceBuffer, "update");
    test.expectEvent(sourceBuffer, "updateend");
    sourceBuffer.appendBuffer(data);
}

function mediaSourceConfigChangeTest(directory, idA, idB, description)
{
    var manifestFilenameA = directory + "/test-" + idA + "-manifest.json";
    var manifestFilenameB = directory + "/test-" + idB + "-manifest.json";
    mediasource_test(function(test, mediaElement, mediaSource)
    {
        mediaElement.pause();
        test.failOnEvent(mediaElement, 'error');
        test.endOnEvent(mediaElement, 'ended');

        fetchManifestAndData(test, manifestFilenameA, function(typeA, dataA)
        {
            fetchManifestAndData(test, manifestFilenameB, function(typeB, dataB)
            {
                assert_equals(typeA, typeB, "Media format types match");

                var sourceBuffer = mediaSource.addSourceBuffer(typeA);

                appendBuffer(test, sourceBuffer, dataA);

                test.waitForExpectedEvents(function()
                {
                    // Add the second buffer starting at 0.5 second.
                    sourceBuffer.timestampOffset = 0.5;
                    appendBuffer(test, sourceBuffer, dataB);
                });

                test.waitForExpectedEvents(function()
                {
                    // Add the first buffer starting at 1 second.
                    sourceBuffer.timestampOffset = 1;
                    appendBuffer(test, sourceBuffer, dataA);
                });

                test.waitForExpectedEvents(function()
                {
                    // Add the second buffer starting at 1.5 second.
                    sourceBuffer.timestampOffset = 1.5;
                    appendBuffer(test, sourceBuffer, dataB);
                });

                test.waitForExpectedEvents(function()
                {
                    // Truncate the presentation to a duration of 2 seconds.
                    mediaSource.duration = 2;
                    mediaSource.endOfStream();

                    mediaElement.play();
                });
            });
        });
    }, description, { timeout: 10000 } );
};
