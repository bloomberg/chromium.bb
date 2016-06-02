function enableAllTextTracks(textTracks) {
    for (var i = 0; i < textTracks.length; i++) {
        var track = textTracks[i];
        if (track.mode == "disabled")
            track.mode = "hidden";
    }
}

function assert_cues_equal(cues, expected) {
    assert_equals(cues.length, expected.length);
    for (var i = 0; i < cues.length; i++) {
        assert_equals(cues[i].id, expected[i].id);
        assert_equals(cues[i].startTime, expected[i].startTime);
        assert_equals(cues[i].endTime, expected[i].endTime);
        assert_equals(cues[i].text, expected[i].text);
    }
}

function assert_cues_match(cues, expected) {
    assert_equals(cues.length, expected.length);
    for (var i = 0; i < cues.length; i++) {
        var cue = cues[i];
        var expectedItem = expected[i];
        for (var property of Object.getOwnPropertyNames(expectedItem))
            assert_equals(cue[property], expectedItem[property]);
    }
}

function check_cues_from_track(src, func) {
    async_test(function(t) {
        var video = document.createElement("video");
        var trackElement = document.createElement("track");
        trackElement.src = src;
        trackElement.default = true;
        video.appendChild(trackElement);

        trackElement.onload = t.step_func_done(function() {
            func(trackElement.track);
        });
    }, "Check cues from " + src);
}