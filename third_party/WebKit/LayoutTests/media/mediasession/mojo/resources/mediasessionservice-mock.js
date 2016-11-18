/*
 * mediasessionservice-mock contains a mock implementation of MediaSessionService.
 */

"use strict";

var MediaSessionAction;

function mojoString16ToJS(mojoString16) {
  return String.fromCharCode.apply(null, mojoString16.data);
}

function mojoImageToJS(mojoImage) {
  var src = mojoImage.src.url;
  var type = mojoString16ToJS(mojoImage.type);
  var sizes = "";
  for (var i = 0; i < mojoImage.sizes.length; i++) {
    if (i > 0)
      sizes += " ";

    var mojoSize = mojoImage.sizes[i];
    sizes += mojoSize.width.toString() + "x" + mojoSize.height.toString();
  }
  return { src: src, type: type, sizes: sizes };
}

function mojoMetadataToJS(mojoMetadata) {
  if (mojoMetadata == null)
    return null;

  var title = mojoString16ToJS(mojoMetadata.title);
  var artist = mojoString16ToJS(mojoMetadata.artist);
  var album = mojoString16ToJS(mojoMetadata.album);
  var artwork = [];
  for (var i = 0; i < mojoMetadata.artwork.length; i++)
    artwork.push(mojoImageToJS(mojoMetadata.artwork[i]));

  return new MediaMetadata({title: title, artist: artist, album: album, artwork: artwork});
}

let mediaSessionServiceMock = loadMojoModules(
    'mediaSessionServiceMock',
    ['third_party/WebKit/public/platform/modules/mediasession/media_session.mojom',
     'mojo/public/js/router',
    ]).then(mojo => {
      let [mediaSessionService, router] = mojo.modules;

      MediaSessionAction = mediaSessionService.MediaSessionAction;

      class MediaSessionServiceMock {
        constructor(interfaceProvider) {
          interfaceProvider.addInterfaceOverrideForTesting(
              mediaSessionService.MediaSessionService.name,
              handle => this.connectMediaSessionService_(handle));
          this.interfaceProvider_ = interfaceProvider;
          this.pendingResponse_ = null;
        }

        connectMediaSessionService_(handle) {
          this.mediaSessionServiceStub_ = new mediaSessionService.MediaSessionService.stubClass(this);
          this.mediaSessionServiceRouter_ = new router.Router(handle);
          this.mediaSessionServiceRouter_.setIncomingReceiver(this.mediaSessionServiceStub_);
        }

        setMetadata(metadata) {
          if (!!this.metadataCallback_)
            this.metadataCallback_(mojoMetadataToJS(metadata));
        }

        setMetadataCallback(callback) {
          this.metadataCallback_ = callback;
        }

        enableAction(action) {
          if (!!this.enableDisableActionCallback_)
            this.enableDisableActionCallback_(action, true);
        }

        disableAction(action) {
          if (!!this.enableDisableActionCallback_)
            this.enableDisableActionCallback_(action, false);
        }

        setEnableDisableActionCallback(callback) {
          this.enableDisableActionCallback_ = callback;
        }

        setClient(client) {
          this.client_ = client;
          if (!!this.clientCallback_)
            this.clientCallback_();
        }

        setClientCallback(callback) {
          this.clientCallback_ = callback;
        }

        getClient() {
          return this.client_;
        }
      }

      return new MediaSessionServiceMock(mojo.frameInterfaces);
    });
