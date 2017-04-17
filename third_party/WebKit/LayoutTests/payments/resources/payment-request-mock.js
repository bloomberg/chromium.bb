/*
 * payment-request-mock contains a mock implementation of PaymentRequest.
 */

"use strict";

let paymentRequestMock = loadMojoModules(
    'paymentRequestMock',
    ['components/payments/content/payment_request.mojom',
     'mojo/public/js/bindings',
    ]).then(mojo => {
  let [paymentRequest, bindings] =  mojo.modules;

  class PaymentRequestMock {
    constructor(interfaceProvider) {
      interfaceProvider.addInterfaceOverrideForTesting(
        paymentRequest.PaymentRequest.name,
        handle => this.bindings_.addBinding(this, handle));

      this.interfaceProvider_ = interfaceProvider;
      this.pendingResponse_ = null;
      this.bindings_ = new bindings.BindingSet(paymentRequest.PaymentRequest);
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
