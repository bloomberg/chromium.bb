/*
 * Copyright 2016 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

const basicCardMethod = {supportedMethods: 'basic-card'};

const basicMastercardVisaMethod = {
  supportedMethods: 'basic-card',
  data: {
    supportedNetworks: ['mastercard', 'visa'],
  },
};

const basicVisaMethod = {
  supportedMethods: 'basic-card',
  data: {
    supportedNetworks: ['visa'],
  },
};

const basicMastercardMethod = {
  supportedMethods: 'basic-card',
  data: {
    supportedNetworks: ['mastercard'],
  },
};

const basicDebitMethod = {
  supportedMethods: 'basic-card',
  data: {
    supportedTypes: ['debit'],
  },
};

const alicePayMethod = {supportedMethods: 'https://alicepay.com/webpay'};
const bobPayMethod = {supportedMethods: 'https://bobpay.com/webpay'};

const defaultDetails = {
  total: {
    label: 'Total',
    amount: {
      currency: 'USD',
      value: '5.00',
    },
  },
};

/**
 * Runs |testFunction| and prints any result or error.
 *
 * @param {function} testFunction A function with no argument and returns a
 * Promise.
 */
function run(testFunction) {
  try {
    testFunction().then(print).catch(print);
  } catch (error) {
    print(error);
  }
}

/**
 * Calls PaymentRequest.canMakePayment() and prints out the result.
 * @param {sequence<PaymentMethodData>} methodData The supported methods.
 * @private
 */
function checkCanMakePayment(methodData) {
  run(() => {
    const request = new PaymentRequest(methodData, defaultDetails);
    return request.canMakePayment();
  });
}

/**
 * Calls PaymentRequest.hasEnrolledInstrument() and prints out the result.
 * @param {sequence<PaymentMethodData>} methodData The supported methods.
 */
function checkHasEnrolledInstrument(methodData) {  // eslint-disable-line no-unused-vars, max-len
  run(() => {
    const request = new PaymentRequest(methodData, defaultDetails);
    return request.hasEnrolledInstrument();
  });
}

/**
 * Merchant checks for ability to pay using "basic-card" regardless of issuer
 * network.
 */
function checkBasicCard() {  // eslint-disable-line no-unused-vars
  checkCanMakePayment([basicCardMethod]);
}

/**
 * Merchant checks for ability to pay using debit cards.
 */
function checkBasicDebit() {  // eslint-disable-line no-unused-vars
  checkCanMakePayment([basicDebitMethod]);
}

/**
 * Merchant checks for ability to pay using "basic-card" with "mastercard" as
 * the supported network.
 */
function checkBasicMasterCard() {  // eslint-disable-line no-unused-vars
  checkCanMakePayment([basicMastercardMethod]);
}

/**
 * Merchant checks for ability to pay using "basic-card" with "visa" as the
 * supported network.
 */
function checkBasicVisa() {  // eslint-disable-line no-unused-vars
  checkCanMakePayment([basicVisaMethod]);
}

/**
 * Merchant checks for ability to pay using "https://alicepay.com/webpay".
 */
function checkAlicePay() {  // eslint-disable-line no-unused-vars
  checkCanMakePayment([alicePayMethod]);
}

/**
 * Merchant checks for ability to pay using "https://bobpay.com/webpay".
 */
function checkBobPay() {  // eslint-disable-line no-unused-vars
  checkCanMakePayment([bobPayMethod]);
}

/**
 * Merchant checks for ability to pay using "https://bobpay.com/webpay" or
 * "basic-card".
 */
function checkBobPayAndBasicCard() {  // eslint-disable-line no-unused-vars
  checkCanMakePayment([bobPayMethod, basicCardMethod]);
}

/**
 * Merchant checks for ability to pay using "https://bobpay.com/webpay" or
 * "visa".
 */
function checkBobPayAndVisa() {  // eslint-disable-line no-unused-vars
  checkCanMakePayment([bobPayMethod, basicVisaMethod]);
}

/**
 * Calls PaymentRequest.show() and prints out the result.
 * @param {sequence<PaymentMethodData>} methodData The supported methods.
 * @private
 */
function buyHelper(methodData) {
  try {
    new PaymentRequest(methodData, defaultDetails)
        .show()
        .then(function(response) {
          response.complete('success')
              .then(function() {
                print(JSON.stringify(response, undefined, 2));
              })
              .catch(function(error) {
                print(error);
              });
        })
        .catch(function(error) {
          print(error);
        });
  } catch (error) {
    print(error);
  }
}

/**
 * Merchant requests payment via either "mastercard" or "basic-card" with "visa"
 * as the supported network.
 */
function buy() {  // eslint-disable-line no-unused-vars
  buyHelper([basicMastercardVisaMethod]);
}

/**
 * Merchant requests payment via "basic-card" with any issuer network.
 */
function buyBasicCard() {  // eslint-disable-line no-unused-vars
  buyHelper([basicCardMethod]);
}

/**
 * Merchant requests payment via "basic-card" with "debit" as the supported card
 * type.
 */
function buyBasicDebit() {  // eslint-disable-line no-unused-vars
  buyHelper([basicDebitMethod]);
}

/**
 * Merchant requests payment via "basic-card" payment method with "mastercard"
 * as the only supported network.
 */
function buyBasicMasterCard() {  // eslint-disable-line no-unused-vars
  buyHelper([basicMastercardMethod]);
}
