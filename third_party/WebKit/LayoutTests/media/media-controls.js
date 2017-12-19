var captionsButtonElement;
var captionsButtonCoordinates;

// As specified in mediaControls.css, this is how long it takes to fade out controls
const controlsFadeOutDurationMs = 300;

// The timeout for the hide-after-no-mouse-movement behavior. Defined (and
// should mirror) the value 'timeWithoutMouseMovementBeforeHidingMediaControls'
// in MediaControls.cpp.
const controlsMouseMovementTimeoutMs = 3000;

function isControlVisible(control) {
    var style = getComputedStyle(control);
    var visibility = style.getPropertyValue("visibility");
    var display = style.getPropertyValue("display");
    return (display != "none" && visibility == "visible");
}

function castButton(videoElement) {
    var controlID = '-internal-media-controls-cast-button';
    var button = mediaControlsElement(window.internals.shadowRoot(videoElement).firstChild, controlID);
    if (!button)
        throw 'Failed to find cast button';
    return button;
}

function downloadButton(videoElement) {
    var controlID = '-internal-media-controls-download-button';
    var button = mediaControlsElement(window.internals.shadowRoot(videoElement).firstChild, controlID);
    if (!button)
        throw 'Failed to find download button';
    return button;
}

function fullscreenButton(videoElement) {
    var controlID = '-webkit-media-controls-fullscreen-button';
    var button = mediaControlsElement(window.internals.shadowRoot(videoElement).firstChild, controlID);
    if (!button)
        throw 'Failed to find fullscreen button';
    return button;
}

function overlayCastButton(videoElement)
{
    var controlID = '-internal-media-controls-overlay-cast-button';
    var button = mediaControlsElement(window.internals.shadowRoot(videoElement).firstChild, controlID);
    if (!button)
        throw 'Failed to find cast button';
    return button;
}

function overflowButton(videoElement)
{
    var controlID = '-internal-media-controls-overflow-button';
    var button = mediaControlsElement(window.internals.shadowRoot(videoElement).firstChild, controlID);
    if (!button)
        throw 'Failed to find cast button';
    return button;
}

function textTrackMenu(video)
{
  var controlID = '-internal-media-controls-text-track-list';
  var element = mediaControlsElement(window.internals.shadowRoot(video).firstChild, controlID);
  if (!element)
    throw 'Failed to find the overflow menu';
  return element;
}

function overflowMenu(video)
{
  var controlID = '-internal-media-controls-overflow-menu-list';
  var element = mediaControlsElement(window.internals.shadowRoot(video).firstChild, controlID);
  if (!element)
    throw 'Failed to find the overflow menu';
  return element;
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

function coordinatesOutsideElement(element)
{
    var elementBoundingRect = element.getBoundingClientRect();
    var x = elementBoundingRect.left - 1;
    var y = elementBoundingRect.top - 1;
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

function textTrackCueElementByIndex(parentElement, cueIndex) {
    var displayElement = textTrackDisplayElement(parentElement);
    if (displayElement) {
        for (i = 0; i < cueIndex; i++)
            displayElement = displayElement.nextSibling;
    }

    return displayElement;
}

function textTrackRegionElement(parentElement)
{
    var containerElement = textTrackContainerElement(parentElement);
    return mediaControlsElement(containerElement.firstChild, "-webkit-media-text-track-region");
}

function textTrackRegionContainerElement(parentElement)
{
    var containerElement = textTrackContainerElement(parentElement);
    return mediaControlsElement(containerElement.firstChild, "-webkit-media-text-track-region-container");
}

function textTrackDisplayElement(parentElement)
{
    var containerElement = textTrackContainerElement(parentElement);
    return mediaControlsElement(containerElement.firstChild, "-webkit-media-text-track-display");
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

function toggleClosedCaptionsButton(videoElement) {
    return mediaControlsButton(videoElement, 'toggle-closed-captions-button');
}

function playButton(videoElement) {
    return mediaControlsButton(videoElement, 'play-button');
}

function muteButton(videoElement) {
    return mediaControlsButton(videoElement, 'mute-button');
}

function clickAtCoordinates(x, y)
{
    eventSender.mouseMoveTo(x, y);
    eventSender.mouseDown();
    eventSender.mouseUp();
}

function textTrackListItemAtIndex(video, index)
{
    var trackListItems = textTrackMenu(video).childNodes;
    for (var i = 0; i < trackListItems.length; i++) {
        var trackListItem = trackListItems[i];
        if (trackListItem.firstChild.getAttribute("data-track-index") == index)
            return trackListItem;
    }
}

function clickCaptionButton(video)
{
    var captionsButtonCoordinates =
            mediaControlsButtonCoordinates(video, "toggle-closed-captions-button");
    clickAtCoordinates(captionsButtonCoordinates[0], captionsButtonCoordinates[1]);
}

function clickTextTrackAtIndex(video, index)
{
    clickCaptionButton(video);
    var trackListItemElement = textTrackListItemAtIndex(video, index);
    var trackListItemCoordinates = elementCoordinates(trackListItemElement);
    clickAtCoordinates(trackListItemCoordinates[0], trackListItemCoordinates[1]);
}

function turnClosedCaptionsOff(video)
{
    clickTextTrackAtIndex(video, -1);
}

function checkCaptionsVisible(video, captions)
{
    for (var i = 0; i < captions.length; i++) {
      assert_equals(textTrackCueElementByIndex(video, i).innerText, captions[i]);
    }
}

function checkCaptionsHidden(video)
{
    assert_equals(textTrackDisplayElement(video), null);
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

function isControlsPanelVisible(element)
{
    return getComputedStyle(mediaControlsButton(element, "panel")).opacity == "1";
}

function isVisible(button) {
    var computedStyle = getComputedStyle(button);
    return computedStyle.display !== "none" &&
           computedStyle.visibility === "visible";
}

function checkButtonHasClass(button, className) {
  assert_true(button.classList.contains(className));
}

function checkButtonNotHasClass(button, className) {
  assert_false(button.classList.contains(className));
}

function checkControlsClassName(videoElement, className) {
  assert_equals(window.internals.shadowRoot(videoElement).firstChild.className, className);
}

function mediaControlsOverlayPlayButton(videoElement) {
  return mediaControlsButton(videoElement, 'overlay-play-button');
}

function mediaControlsOverlayPlayButtonInternal(videoElement) {
  var controlID = '-internal-media-controls-overlay-play-button-internal';
  var element = mediaControlsElement(
      window.internals.shadowRoot(
          mediaControlsOverlayPlayButton(videoElement)).firstChild, controlID);
  if (!element)
    throw 'Failed to find the internal overlay play button';
  return element;
}

function doubleTapAtCoordinates(x, y, timeout) {
  if (timeout == undefined)
    timeout = 100;

  chrome.gpuBenchmarking.pointerActionSequence([
    {
      source: 'mouse',
      actions: [
        { name: 'pointerDown', x: x, y: y },
        { name: 'pointerUp' },
        { name: 'pause', duration: timeout / 1000 },
        { name: 'pointerDown', x: x, y: y },
        { name: 'pointerUp' }
      ]
    }
  ]);
}

function singleTapAtCoordinates(xPos, yPos, callback) {
  chrome.gpuBenchmarking.pointerActionSequence([
    {
      source: 'mouse',
      actions: [
        { name: 'pointerDown', x: xPos, y: yPos },
        { name: 'pointerUp' }
      ]
    }
  ], callback);
}

function enableDoubleTapToJumpForTest(t) {
  var doubleTapToJumpOnVideoEnabledValue =
      internals.runtimeFlags.doubleTapToJumpOnVideoEnabled;
  internals.runtimeFlags.doubleTapToJumpOnVideoEnabled = true;

  t.add_cleanup(() => {
    internals.runtimeFlags.doubleTapToJumpOnVideoEnabled =
        doubleTapToJumpOnVideoEnabledValue;
  });
}
