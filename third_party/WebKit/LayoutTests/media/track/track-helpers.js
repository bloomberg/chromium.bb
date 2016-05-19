function enableAllTextTracks(textTracks) {
    for (var i = 0; i < textTracks.length; i++) {
        var track = textTracks[i];
        if (track.mode == "disabled")
            track.mode = "hidden";
    }
}

function assert_cues_equal(cues, expected) {
    for (var i = 0; i < cues.length; i++) {
        assert_equals(cues[i].id, expected[i].id);
        assert_equals(cues[i].startTime, expected[i].startTime);
        assert_equals(cues[i].endTime, expected[i].endTime);
        assert_equals(cues[i].text, expected[i].text);
    }
}