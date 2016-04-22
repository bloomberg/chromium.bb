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

function textTrackDisplayElement(parentElement, id, cueNumber)
{
    var textTrackContainerID = "-webkit-media-text-track-container";
    var containerElement = mediaControlsElement(internals.shadowRoot(parentElement).firstChild, textTrackContainerID);

    if (!containerElement)
        throw "Failed to find text track container element";

    if (!id)
        return containerElement;

    if (arguments[1] != 'cue')
        var controlID = "-webkit-media-text-track-" + arguments[1];
    else
        var controlID = arguments[1];

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

function turnClosedCaptionsOff(video)
{
    selectTextTrack(video, -1);
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
