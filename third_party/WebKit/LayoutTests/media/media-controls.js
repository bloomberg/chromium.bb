var captionsButtonElement;
var captionsButtonCoordinates;

// As specified in mediaControls.css, this is how long it takes to fade out controls
const controlsFadeOutDurationMs = 300;

// The timeout for the hide-after-no-mouse-movement behavior. Defined (and
// should mirror) the value 'timeWithoutMouseMovementBeforeHidingMediaControls'
// in MediaControls.cpp.
const controlsMouseMovementTimeoutMs = 3000;

function overlayCastButton(videoElement)
{
    var controlID = '-internal-media-controls-overlay-cast-button';
    var button = mediaControlsElement(window.internals.shadowRoot(videoElement).firstChild, controlID);
    if (!button)
        throw 'Failed to find cast button';
    return button;
}

function mediaControlsElement(first, id)
{
    for (var element = first; element; element = element.nextSibling) {
        // Not every element in the media controls has a shadow pseudo ID, eg. the
        // text nodes for the time values, so guard against exceptions.
        try {
            if (internals.shadowPseudoId(element) == id)
                return element;
        } catch (exception) { }

        if (element.firstChild) {
            var childElement = mediaControlsElement(element.firstChild, id);
            if (childElement)
                return childElement;
        }
    }

    return null;
}

function mediaControlsButton(element, id)
{
    var controlID = "-webkit-media-controls-" + id;
    var button = mediaControlsElement(internals.shadowRoot(element).firstChild, controlID);
    if (!button)
        throw "Failed to find media control element ID '" + id + "'";
    return button;
}

function elementCoordinates(element)
{
    var elementBoundingRect = element.getBoundingClientRect();
    var x = elementBoundingRect.left + elementBoundingRect.width / 2;
    var y = elementBoundingRect.top + elementBoundingRect.height / 2;
    return new Array(x, y);
}

function mediaControlsButtonCoordinates(element, id)
{
    var button = mediaControlsButton(element, id);
    return elementCoordinates(button);
}

function mediaControlsButtonDimensions(element, id)
{
    var button = mediaControlsButton(element, id);
    var buttonBoundingRect = button.getBoundingClientRect();
    return new Array(buttonBoundingRect.width, buttonBoundingRect.height);
}

function textTrackContainerElement(parentElement) {
    return mediaControlsElement(internals.shadowRoot(parentElement).firstChild,
        "-webkit-media-text-track-container");
}

function textTrackCueDisplayElement(parentElement) {
    var containerElement = textTrackContainerElement(parentElement);
    return mediaControlsElement(containerElement.firstChild, "-webkit-media-text-track-display");
}

function textTrackCueElementByIndex(parentElement, cueIndex) {
    var displayElement = textTrackCueDisplayElement(parentElement);
    if (displayElement) {
        for (i = 0; i < cueIndex; i++)
            displayElement = displayElement.nextSibling;
    }

    return displayElement;
}

// TODO(srirama.m): Phase out the uses of this function and
// replace with more specific functions at each call site.
// For ex: textTrackDisplayElement(video, 'region') => textTrackRegionElement(video) and
// textTrackDisplayElement(video, 'region-container') => textTrackRegionContainerElement(video).
function textTrackDisplayElement(parentElement, id, cueNumber)
{
    var containerElement = textTrackContainerElement(parentElement);

    if (!containerElement)
        throw "Failed to find text track container element";

    if (!id)
        return containerElement;

    var controlID = id;
    if (controlID != 'cue')
        controlID = "-webkit-media-text-track-" + id;

    var displayElement = mediaControlsElement(containerElement.firstChild, controlID);
    if (!displayElement)
        throw "No text track cue with display id '" + controlID + "' is currently visible";

    if (cueNumber) {
        for (i = 0; i < cueNumber; i++)
            displayElement = displayElement.nextSibling;

        if (!displayElement)
            throw "There are not " + cueNumber + " text track cues visible";
    }

    return displayElement;
}

function isClosedCaptionsButtonVisible(currentMediaElement)
{
    var captionsButtonElement = mediaControlsButton(currentMediaElement, "toggle-closed-captions-button");
    var captionsButtonCoordinates = mediaControlsButtonCoordinates(currentMediaElement, "toggle-closed-captions-button");

    if (!captionsButtonElement.disabled
        && captionsButtonCoordinates[0] > 0
        && captionsButtonCoordinates[1] > 0) {
        return true;
    }

    return false;
}

function testClosedCaptionsButtonVisibility(expected)
{
    try {
        captionsButtonElement = mediaControlsButton(mediaElement, "toggle-closed-captions-button");
        captionsButtonCoordinates = mediaControlsButtonCoordinates(mediaElement, "toggle-closed-captions-button");
    } catch (exception) {
        consoleWrite("Failed to find a closed captions button or its coordinates: " + exception);
        if (expected)
            failTest();
        return;
    }

    consoleWrite("");
    if (expected == true) {
        consoleWrite("** Caption button should be visible and enabled.");
        testExpected("captionsButtonCoordinates[0]", 0, ">");
        testExpected("captionsButtonCoordinates[1]", 0, ">");
        testExpected("captionsButtonElement.disabled", false);
    } else {
        consoleWrite("** Caption button should not be visible.");
        testExpected("captionsButtonCoordinates[0]", 0, "<=");
        testExpected("captionsButtonCoordinates[1]", 0, "<=");
    }
}

function clickAtCoordinates(x, y)
{
    eventSender.mouseMoveTo(x, y);
    eventSender.mouseDown();
    eventSender.mouseUp();
}

function clickCCButton()
{
    consoleWrite("*** Click the CC button.");
    clickAtCoordinates(captionsButtonCoordinates[0], captionsButtonCoordinates[1]);
}

function textTrackListItemAtIndex(video, index)
{
    var textTrackListElementID = "-internal-media-controls-text-track-list";
    var textTrackListElement = mediaControlsElement(internals.shadowRoot(video).firstChild, textTrackListElementID);
    if (!textTrackListElement)
        throw "Failed to find text track list element";

    var trackListItems = textTrackListElement.childNodes;
    for (var i = 0; i < trackListItems.length; i++) {
        var trackListItem = trackListItems[i];
        if (trackListItem.firstChild.getAttribute("data-track-index") == index)
            return trackListItem;
    }
}

function selectTextTrack(video, index)
{
    clickCCButton();
    var trackListItemElement = textTrackListItemAtIndex(video, index);
    var trackListItemCoordinates = elementCoordinates(trackListItemElement);
    clickAtCoordinates(trackListItemCoordinates[0], trackListItemCoordinates[1]);
}

function clickTextTrackAtIndex(video, index)
{
    var captionsButtonCoordinates = mediaControlsButtonCoordinates(video, "toggle-closed-captions-button");
    clickAtCoordinates(captionsButtonCoordinates[0], captionsButtonCoordinates[1]);
    var trackListItemElement = textTrackListItemAtIndex(video, index);
    var trackListItemCoordinates = elementCoordinates(trackListItemElement);
    clickAtCoordinates(trackListItemCoordinates[0], trackListItemCoordinates[1]);
}

function turnClosedCaptionsOff(video)
{
    clickTextTrackAtIndex(video, -1);
}

function runAfterHideMediaControlsTimerFired(func, mediaElement)
{
    if (mediaElement.paused)
        throw "The media element is not playing";

    // Compute the time it'll take until the controls will be invisible -
    // assuming playback has been started prior to invoking this
    // function. Allow 500ms slack.
    var hideTimeoutMs = controlsMouseMovementTimeoutMs + controlsFadeOutDurationMs + 500;

    if (!mediaElement.loop && hideTimeoutMs >= 1000 * (mediaElement.duration - mediaElement.currentTime))
        throw "The media will end before the controls have been hidden";

    setTimeout(func, hideTimeoutMs);
}

function hasFullscreenButton(element)
{
    var size = mediaControlsButtonDimensions(element, "fullscreen-button");
    return size[0] > 0 && size[1] > 0;
}
