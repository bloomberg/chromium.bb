# Image Capture API

This folder contains the implementation of the [W3C Image Capture API](https://w3c.github.io/mediacapture-image/).  Image Capture was shipped in Chrome M59; please consult the [Implementation Status](https://github.com/w3c/mediacapture-image/blob/master/implementation-status.md) if you think a feature should be available and isn't.

This API is structured around the [ImageCapture class](https://w3c.github.io/mediacapture-image/#imagecaptureapi) _and_ a number of [extensions](https://w3c.github.io/mediacapture-image/#extensions) to the `MediaStreamTrack` feeding it (let's call them `theImageCapturer` and `theTrack`, respectively).

## API Mechanics

### `takePhoto()` and `grabFrame()`

- `takePhoto()` returns the result of a single photographic exposure as a `Blob` which can be downloaded, stored by the browser or displayed in an `img` element. This method uses the highest available photographic camera resolution.

- `grabFrame()` returns a snapshot of the live video in `theTrack` as an `ImageBitmap` object  which could (for example) be drawn on a `canvas` and then post-processed to selectively change color values. Note that the `ImageBitmap` will only have the resolution of the video track — which will generally be lower than the camera's still-image resolution.

(_Adapted from the [Origin Trials Web Update post](https://developers.google.com/web/updates/2016/12/imagecapture)_)

### Photo settings and capabilities

The photo-specific options and settings are associated to `theImageCapturer` or `theTrack` depending on whether a given capability/setting has an immediately recognisable effect on `theTrack`, in other words if it's "live" or not. For example, changing the zoom level is instantly reflected on the `theTrack`, while connecting the Red Eye Reduction, if available, is not.

 Object | type | retrieved by... |
 :--| :-- | --: |
[`PhotoCapabilities`](https://w3c.github.io/mediacapture-image/##photocapabilities-section) | non-live capabilities | `theImageCapturer.getPhotoCapabilities()` |
[`MediaTrackCapabilities`](https://w3c.github.io/mediacapture-image/#mediatrackcapabilities-section) | live capabilities | `theTrack.getCapabilities()` |
|  |  |
[`PhotoSettings`](https://w3c.github.io/mediacapture-image/##photocapabilities-section) | non-live settings |  |
[`MediaTrackSettings`](https://w3c.github.io/mediacapture-image/#mediatracksettings-section) | live settings | `theTrack.getSettings()` |

## Other topics

### Are `takePhoto()` and `grabFrame()` the same?

These methods would not produce the same results as explained in [this issue comment](https://bugs.chromium.org/p/chromium/issues/detail?id=655107#c8):


>  Let me reconstruct the conversion steps each image goes through in CrOs/Linux;
>  [...]
>
>  a) Live video capture produces frames via `V4L2CaptureDelegate::DoCapture()` [1].
>  The original data (from the WebCam) comes in either YUY2 (a 4:2:2 format) or
>  MJPEG, depending if the capture is smaller than 1280x720p or not, respectively.

>  b) This `V4L2CaptureDelegate` sends the capture frame to a conversion stage to
>  I420 [2].  I420 is a 4:2:0 format, so it has lost some information
>  irretrievably.  This I420 format is the one used for transporting video frames
>  to the rendered.

>  c) This I420 is the input to `grabFrame()`, which produces a JS ImageBitmap,
>  unencoded, after converting the I420 into RGBA [3] of the appropriate endian-ness.

> What happens to `takePhoto()`? It takes the data from the Webcam in a) and
> either returns a JPEG Blob [4] or converts the YUY2 [5] and encodes it to PNG
>  using the default compression value (6 in a 0-10 scale IIRC) [6].

>  IOW:

```
  - for smaller video resolutions:

  OS -> YUY2 ---> I420 --> RGBA --> ImageBitmap     grabFrame()
             |
             +--> RGBA --> PNG ---> Blob            takePhoto()

  - and for larger video resolutions:

  OS -> MJPEG ---> I420 --> RGBA --> ImageBitmap    grabFrame()
              |
              +--> JPG ------------> Blob           takePhoto()
```


> Where every conversion to-I420 loses information and so does the encoding to
> PNG.  Even a conversion `RGBA --> I420 --> RGBA` would not produce the original
> image.  (Plus, when you show `ImageBitmap` and/or Blob on an `<img>` or `<canvas>`
> there are more stages of decoding and even colour correction involved!)

> With all that, I'm not surprised at all that the images are not pixel
> accurate!  :-)


### Why are `PhotoCapabilities.fillLightMode` and `MediaTrackCapabilities.torch` separated?

Because they are different things: `torch` means flash constantly on/off whereas `fillLightMode` means flash always-on/always-off/auto _when taking a photographic exposure_.

`torch` lives in `theTrack` because the effect can be seen "live" on it, whereas `fillLightMode` lives in `theImageCapture` object because the effect of modifying it can only be seen after taking a picture.



## Testing

Sensors layout tests are located in [`LayoutTests/imagecapture`](https://chromium.googlesource.com/chromium/src/+/master/third_party/WebKit/LayoutTests/imagecapture/), [`LayoutTests/fast/imagecapture`](https://chromium.googlesource.com/chromium/src/+/master/third_party/WebKit/LayoutTests/fast/imagecapture/) and [`LayoutTests/external/mediacapture-image`](https://chromium.googlesource.com/chromium/src/+/master/third_party/WebKit/LayoutTests/external/wpt/mediacapture-image/).
