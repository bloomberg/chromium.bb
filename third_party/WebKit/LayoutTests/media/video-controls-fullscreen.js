"use strict";

function fullscreen_test(controller)
{
    async_test(function(t)
    {
        var v1 = document.createElement("video");
        var v2 = document.createElement("video");
        v1.controls = v2.controls = true;
        v1.controller = v2.controller = controller;
        v1.src = findMediaFile("video", "content/test");
        v2.src = findMediaFile("audio", "content/test");
        document.body.appendChild(v1);
        document.body.appendChild(v2);

        // load event fires when both video elements are ready
        window.addEventListener("load", t.step_func(function()
        {
            // no fullscreen button for a video element with no video track
            assert_button_hidden(v2);

            // click the fullscreen button
            var coords = mediaControlsButtonCoordinates(v1, "fullscreen-button");
            eventSender.mouseMoveTo(coords[0], coords[1]);
            eventSender.mouseDown();
            eventSender.mouseUp();
            // wait for the fullscreenchange event
        }));

        v1.addEventListener("webkitfullscreenchange", t.step_func(function()
        {
            t.done();
        }));

        v2.addEventListener("webkitfullscreenchange", t.step_func(function()
        {
            assert_unreached();
        }));
    });
}

function fullscreen_not_supported_test()
{
    async_test(function(t)
    {
        var v = document.createElement("video");
        v.controls = true;
        v.src = findMediaFile("video", "content/test");
        document.body.appendChild(v);

        // load event fires when video elements is ready
        window.addEventListener("load", t.step_func(function()
        {
            // no fullscreen button for a video element when fullscreen is not
            // supported
            assert_button_hidden(v);
            t.done();
        }));
    });
}

function assert_button_hidden(elm)
{
    assert_array_equals(mediaControlsButtonDimensions(elm, "fullscreen-button"), [0, 0]);
}
