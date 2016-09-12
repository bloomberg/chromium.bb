// We expect the items in the overflow to appear in the following ordering.
var overflowButtonsCSS = [
    "-webkit-media-controls-mute-button",
    "-internal-media-controls-cast-button",
    "-webkit-media-controls-toggle-closed-captions-button",
    "-webkit-media-controls-fullscreen-button",
    "-webkit-media-controls-play-button"];
//  PseudoID for the overflow button
var menuID = "-internal-overflow-menu-button";
//  PseudoID for the overflow list
var listID = "-internal-media-controls-overflow-menu-list";

// Returns the overflow menu button within the given media element
function getOverflowMenuButton(media) {
  return mediaControlsElement(internals.shadowRoot(media).firstChild, menuID);
}

// Returns the overflow menu list of overflow controls
function getOverflowList(media) {
  return mediaControlsElement(internals.shadowRoot(media).firstChild, listID);
}

// Location of media control element in the overflow button
var OverflowMenuButtons = {
  MUTE: 0,
  CAST: 1,
  CLOSED_CAPTIONS: 2,
  FULLSCREEN: 3,
  PLAY: 4,
};

// Default text within the overflow menu
var overflowMenuText = ["Mute", "Cast", "Captions", "Fullscreen", "Play"];

