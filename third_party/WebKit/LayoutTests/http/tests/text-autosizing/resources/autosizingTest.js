function setWindowSizeOverride(width, height) {
    if (window.internals)
        window.internals.settings.setTextAutosizingWindowSizeOverride(width, height);
}

function setFontScaleFactor(scale) {
    if (window.internals)
        window.internals.settings.setAccessibilityFontScaleFactor(scale);
}

function initAutosizingTest() {
    if (window.internals) {
        window.internals.settings.setTextAutosizingEnabled(true);
        setWindowSizeOverride(320, 480);
    } else if (window.console && console.warn) {
        console.warn("This test depends on Text Autosizing being enabled. Run with content shell "
                + "and --run-layout-test or manually enable Text Autosizing and either use a "
                + "mobile device with 320px device-width (like Nexus S or iPhone), or define "
                + "DEBUG_TEXT_AUTOSIZING_ON_DESKTOP.");
    }
}

// Automatically call init. Users who want a different window size can use setWindowSizeOverride.
initAutosizingTest();
