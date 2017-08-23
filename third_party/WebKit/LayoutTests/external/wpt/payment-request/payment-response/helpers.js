setup({ explicit_done: true, explicit_timeout: true });

/**
 * Pops up a payment sheet, allowing options to be
 * passed in if particular values are needed.
 *
 * @param PaymentOptions options
 */
async function getPaymentResponse(options, id) {
  const { response } = getPaymentRequestResponse(options, id);
  return response;
}

/**
 * Creates a payment request and response pair.
 * It also shows the payment sheet.
 *
 * @param {PaymentOptions?} options
 * @param {String?} id
 */
async function getPaymentRequestResponse(options, id) {
  const methods = [{ supportedMethods: "basic-card" }];
  const details = {
    id,
    total: {
      label: "Total due",
      amount: { currency: "USD", value: "1.0" },
    },
    shippingOptions: [
      {
        id: "fail1",
        label: "Fail option 1",
        amount: { currency: "USD", value: "5.00" },
        selected: false,
      },
      {
        id: "pass",
        label: "Pass option",
        amount: { currency: "USD", value: "5.00" },
        selected: true,
      },
      {
        id: "fail2",
        label: "Fail option 2",
        amount: { currency: "USD", value: "5.00" },
        selected: false,
      },
    ],
  };
  const request = new PaymentRequest(methods, details, options);
  request.onshippingaddresschange = ev => ev.updateWith(details);
  request.onshippingoptionchange = ev => ev.updateWith(details);
  const response = await request.show();
  return { request, response };
}

/**
 * Runs a manual test for payment
 *
 * @param {HTMLButtonElement} button The HTML button.
 * @param {PaymentOptions?} options.
 * @param {Object} expected What property values are expected to pass the test.
 * @param {String?} id And id for the request/response pair.
 */
async function runManualTest(button, options, expected = {}, id = undefined) {
  button.disabled = true;
  const { request, response } = await getPaymentRequestResponse(options, id);
  await response.complete();
  test(() => {
    assert_equals(response.requestId, request.id, `Expected ids to match`);
    for (const [attribute, value] of Object.entries(expected)) {
      assert_equals(
        response[attribute],
        value,
        `Expected response ${attribute} attribute to be ${value}`
      );
    }
  }, button.textContent.trim());
}
