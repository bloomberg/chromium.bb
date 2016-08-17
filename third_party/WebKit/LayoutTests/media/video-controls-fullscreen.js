"use strict";

function fullscreen_test()
{
    async_test(function(t)
    {
        var v1 = document.createElement("video");
        var v2 = document.createElement("video");
        v1.controls = v2.controls = true;
        v1.src = findMediaFile("video", "content/test");
        v2.src = findMediaFile("audio", "content/test");
        document.body.appendChild(v1);
        document.body.appendChild(v2);

        // load event fires when both video elements are ready
        window.addEventListener("load", t.step_func(function()
        {
            assert_true(hasFullscreenButton(v1),
                        "fullscreen button shown when there is a video track");
            assert_false(hasFullscreenButton(v2),
                         "fullscreen button not shown when there is no video track");

            // click the fullscreen button
            var coords = mediaControlsButtonCoordinates(v1, "fullscreen-button");
            eventSender.mouseMoveTo(coords[0], coords[1]);
            eventSender.mouseDown();
            eventSender.mouseUp();
            // wait for the fullscreenchange event
        }));

        v1.addEventListener("webkitfullscreenchange", t.step_func_done());
        v2.addEventListener("webkitfullscreenchange", t.unreached_func());
    });
}

function fullscreen_iframe_test()
{
    async_test(function(t)
    {
        var iframe = document.querySelector("iframe");
        var doc = iframe.contentDocument;
        var v = doc.createElement("video");
        v.controls = true;
        v.src = findMediaFile("video", "content/test");
        doc.body.appendChild(v);

        v.addEventListener("loadeddata", t.step_func_done(function()
        {
            assert_equals(hasFullscreenButton(v), iframe.allowFullscreen,
                          "fullscreen button shown if and only if fullscreen is allowed");
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

        v.addEventListener("loadeddata", t.step_func_done(function()
        {
            assert_false(hasFullscreenButton(v),
                         "fullscreen button not show when fullscreen is not supported");
        }));
    });
}
