/*
 * payment-request-mock contains a mock implementation of PaymentRequest.
 */

"use strict";

let paymentRequestMock = loadMojoModules(
    'paymentRequestMock',
    ['components/payments/payment_request.mojom',
     'mojo/public/js/router',
    ]).then(mojo => {
  let [paymentRequest, router] =  mojo.modules;

  class PaymentRequestMock {
    constructor(interfaceProvider) {
      interfaceProvider.addInterfaceOverrideForTesting(
        paymentRequest.PaymentRequest.name,
        handle => this.connectPaymentRequest_(handle));

      this.interfaceProvider_ = interfaceProvider;
      this.pendingResponse_ = null;
    }

    connectPaymentRequest_(handle) {
      this.paymentRequestStub_ = new paymentRequest.PaymentRequest.stubClass(this);
      this.paymentRequestRouter_ = new router.Router(handle);
      this.paymentRequestRouter_.setIncomingReceiver(this.paymentRequestStub_);
    }

    init(client, supportedMethods, details, options) {
      this.client_ = client;
      if (this.pendingResponse_) {
        let response = this.pendingResponse_;
        this.pendingResponse_ = null;
        this.onPaymentResponse(response);
      }
    }

    show() {
    }

    updateWith(details) {
    }

    complete(success) {
    }

    onPaymentResponse(data) {
      if (!this.client_) {
        this.pendingResponse_ = data;
        return;
      }
      this.client_.onPaymentResponse(new paymentRequest.PaymentResponse(data));
    }

    onComplete() {
      this.client_.onComplete();
    }
  }
  return new PaymentRequestMock(mojo.frameInterfaces);
});
