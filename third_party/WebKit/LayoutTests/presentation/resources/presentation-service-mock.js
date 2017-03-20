/*
 * Mock implementation of mojo PresentationService.
 */

"use strict";

let presentationServiceMock = loadMojoModules(
    'presentationServiceMock',
    [
      'third_party/WebKit/public/platform/modules/presentation/presentation.mojom',
      'url/mojo/url.mojom',
      'mojo/public/js/bindings',
    ]).then(mojo => {
      let [ presentationService, url, bindings ] = mojo.modules;

      class MockPresentationConnection {
      };

      class PresentationServiceMock {
        constructor(interfaceProvider) {
          interfaceProvider.addInterfaceOverrideForTesting(
              presentationService.PresentationService.name,
              handle => this.bindingSet_.addBinding(this, handle));
          this.interfaceProvider_ = interfaceProvider;
          this.pendingResponse_ = null;
          this.bindingSet_ = new bindings.BindingSet(
              presentationService.PresentationService);
          this.controllerConnectionPtr_ = null;
          this.receiverConnectionRequest_ = null;

          this.onSetClient = null;
        }

        setClient(client) {
          this.client_ = client;

          if (this.onSetClient)
            this.onSetClient();
        }

        startPresentation(urls) {
          return Promise.resolve({
              presentation_info: { url: urls[0], id: 'fakePresentationId' },
              error: null,
          });
        }

        reconnectPresentation(urls) {
          return Promise.resolve({
              presentation_info: { url: urls[0], id: 'fakePresentationId' },
              error: null,
          });
        }

        terminate(presentationUrl, presentationId) {
          this.client_.onConnectionStateChanged(
              { url: presentationUrl, id: presentationId },
              presentationService.PresentationConnectionState.TERMINATED);
        }

        setPresentationConnection(
            presentation_info, controllerConnectionPtr,
            receiverConnectionRequest) {
          this.controllerConnectionPtr_ = controllerConnectionPtr;
          this.receiverConnectionRequest_ = receiverConnectionRequest;
          this.client_.onConnectionStateChanged(
              presentation_info,
              presentationService.PresentationConnectionState.CONNECTED);
        }

        onReceiverConnectionAvailable(
            strUrl, id, opt_controllerConnectionPtr, opt_receiverConnectionRequest) {
          const mojoUrl = new url.Url();
          mojoUrl.url = strUrl;
          var controllerConnectionPtr = opt_controllerConnectionPtr;
          if (!controllerConnectionPtr) {
            controllerConnectionPtr = new presentationService.PresentationConnectionPtr();
            const connectionBinding = new bindings.Binding(
                presentationService.PresentationConnection,
                new MockPresentationConnection(),
                bindings.makeRequest(controllerConnectionPtr));
          }

          var receiverConnectionRequest = opt_receiverConnectionRequest;
          if (!receiverConnectionRequest) {
            receiverConnectionRequest = bindings.makeRequest(
                new presentationService.PresentationConnectionPtr());
          }

          this.client_.onReceiverConnectionAvailable(
              { url: mojoUrl, id: id },
              controllerConnectionPtr, receiverConnectionRequest);
        }

        getControllerConnectionPtr() {
          return this.controllerConnectionPtr_;
        }

        getReceiverConnectionRequest() {
          return this.receiverConnectionRequest_;
        }
      }

      return new PresentationServiceMock(mojo.frameInterfaces);
    });

function waitForClick(callback, button) {
  button.addEventListener('click', callback, { once: true });

  if (!('eventSender' in window))
    return;

  const boundingRect = button.getBoundingClientRect();
  const x = boundingRect.left + boundingRect.width / 2;
  const y = boundingRect.top + boundingRect.height / 2;

  eventSender.mouseMoveTo(x, y);
  eventSender.mouseDown();
  eventSender.mouseUp();
}
