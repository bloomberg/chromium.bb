function assertPictureInPictureButtonVisible(videoElement)
{
  assert_true(isVisible(pictureInPictureButton(videoElement)),
      "Picture in picture button should be visible.");
}

function assertPictureInPictureButtonNotVisible(videoElement)
{
  assert_false(isVisible(pictureInPictureButton(videoElement)),
      "Picture in picture button should not be visible.");
}

function pictureInPictureButton(videoElement)
{
  var elementId = '-internal-media-controls-picture-in-picture-button';
  var button = mediaControlsElement(
      window.internals.shadowRoot(videoElement).firstChild,
      elementId);
  if (!button)
    throw 'Failed to find picture in picture button.';
  return button;
}

function assertPictureInPictureInterstitialVisible(videoElement)
{
  assert_true(isVisible(pictureInPictureInterstitial(videoElement)),
      "Picture in picture interstitial should be visible.");
}

function assertPictureInPictureInterstitialNotVisible(videoElement)
{
  assert_false(isVisible(pictureInPictureInterstitial(videoElement)),
      "Picture in picture interstitial should not be visible.");
}

function pictureInPictureInterstitial(videoElement)
{
  var elementId = '-internal-picture-in-picture-interstitial';
  var interstitial = mediaControlsElement(
      window.internals.shadowRoot(videoElement).firstChild,
      elementId);
  if (!interstitial)
    throw 'Failed to find picture in picture interstitial.';
  return interstitial;
}

function enablePictureInPictureForTest(t)
{
  var pictureInPictureEnabledValue =
      internals.runtimeFlags.pictureInPictureEnabled;
  internals.runtimeFlags.pictureInPictureEnabled = true;

  t.add_cleanup(() => {
    internals.runtimeFlags.pictureInPictureEnabled =
        pictureInPictureEnabledValue;
  });
}

function click(button)
{
  const pos = offset(button);
  const rect = button.getBoundingClientRect();
  singleTapAtCoordinates(
      Math.ceil(pos.left + rect.width / 2),
      Math.ceil(pos.top + rect.height / 2));
}