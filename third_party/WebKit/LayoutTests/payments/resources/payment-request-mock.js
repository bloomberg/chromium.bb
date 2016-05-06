/*
 * payment-request-mock contains a mock implementation of PaymentRequest.
 */

"use strict";

let paymentRequestMock = loadMojoModules(
    'paymentRequestMock',
    ['third_party/WebKit/public/platform/modules/payments/payment_request.mojom',
     'mojo/public/js/router',
    ]).then(mojo => {
  let [paymentRequest, router] =  mojo.modules;

  class PaymentRequestMock {
    constructor(serviceRegistry) {
      serviceRegistry.addServiceOverrideForTesting(
        paymentRequest.PaymentRequest.name,
        handle => this.connectPaymentRequest_(handle));

      this.serviceRegistry_ = serviceRegistry;
      this.pendingResponse_ = null;
    }

    connectPaymentRequest_(handle) {
      this.paymentRequestStub_ = new paymentRequest.PaymentRequest.stubClass(this);
      this.paymentRequestRouter_ = new router.Router(handle);
      this.paymentRequestRouter_.setIncomingReceiver(this.paymentRequestStub_);
    }

    setClient(client) {
      this.client_ = client;
      if (this.pendingResponse_) {
        let response = this.pendingResponse_;
        this.pendingResponse_ = null;
        this.onPaymentResponse(response);
      }
    }

    show(supportedMethods, details, options, stringifiedData) {
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
  return new PaymentRequestMock(mojo.frameServiceRegistry);
});
