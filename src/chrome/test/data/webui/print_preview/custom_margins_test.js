// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('custom_margins_test', function() {
  /** @enum {string} */
  const TestNames = {
    ControlsCheck: 'controls check',
    SetFromStickySettings: 'set from sticky settings',
    DragControls: 'drag controls',
    SetControlsWithTextbox: 'set controls with textbox',
    RestoreStickyMarginsAfterDefault: 'restore sticky margins after default',
    MediaSizeClearsCustomMargins: 'media size clears custom margins',
    LayoutClearsCustomMargins: 'layout clears custom margins',
    IgnoreDocumentMarginsFromPDF: 'ignore document margins from pdf',
    MediaSizeClearsCustomMarginsPDF: 'media size clears custom margins pdf',
  };

  const suiteName = 'CustomMarginsTest';
  suite(suiteName, function() {
    /** @type {?PrintPreviewMarginControlContainerElement} */
    let container = null;

    /** @type {!Array<!print_preview.ticket_items.CustomMarginsOrientation>} */
    let sides = [];

    // Only care about marginType, customMargins, mediaSize, and layout
    let settings = {
      margins: {
        value: 0,  // print_preview.ticket_items.MarginsTypeValue.DEFAULT,
        unavailableValue: 0,
        valid: true,
        available: true,
        key: 'marginsType',
      },
      customMargins: {
        value: {},
        unavailableValue: {},
        valid: true,
        available: true,
        key: 'customMargins',
      },
      layout: {
        value: false,
        unavailableValue: false,
        valid: true,
        available: true,
        key: 'isLandscapeEnabled',
      },
      mediaSize: {
        value: {
          width_microns: 215900,
          height_microns: 279400,
        },
        unavailableValue: {},
        valid: true,
        available: true,
        key: 'mediaSize',
      },
    };

    /** @type {number} */
    const pixelsPerInch = 100;

    /** @type {number} */
    const pointsPerInch = 72.0;

    /** @type {number} */
    const defaultMarginPts = 36;  // 0.5 inch

    // Keys for the custom margins setting, in order.
    const keys = ['marginTop', 'marginRight', 'marginBottom', 'marginLeft'];

    /** @override */
    setup(function() {
      PolymerTest.clearBody();
      sides = [
        print_preview.ticket_items.CustomMarginsOrientation.TOP,
        print_preview.ticket_items.CustomMarginsOrientation.RIGHT,
        print_preview.ticket_items.CustomMarginsOrientation.BOTTOM,
        print_preview.ticket_items.CustomMarginsOrientation.LEFT
      ];

      container =
          document.createElement('print-preview-margin-control-container');
      container.previewLoaded = false;
      // 8.5 x 11, in points
      container.pageSize = new print_preview.Size(612, 794);
      container.documentMargins = new print_preview.Margins(
          defaultMarginPts, defaultMarginPts, defaultMarginPts,
          defaultMarginPts);
      container.measurementSystem = new print_preview.MeasurementSystem(
          ',', '.', print_preview.MeasurementSystemUnitType.IMPERIAL);
      container.state = print_preview_new.State.NOT_READY;
    });

    /** @return {!Array<!PrintPreviewMarginControlElement>} */
    function getControls() {
      return container.shadowRoot.querySelectorAll(
          'print-preview-margin-control');
    }

    /*
     * Completes setup of the test by setting the settings and adding the
     * container to the document body.
     * @return {!Promise} Promise that resolves when all controls have been
     *     added and initialization is complete.
     */
    function finishSetup() {
      document.body.appendChild(container);

      // Wait for the control elements to be created before updating the state.
      container.$$('template').notifyDomChange = true;
      let controlsAdded = test_util.eventToPromise('dom-change', container);
      return controlsAdded.then(() => {
        // 8.5 x 11, in pixels
        const controls = getControls();
        assertEquals(4, controls.length);
        container.settings = settings;
        container.state = print_preview_new.State.READY;
        container.updateClippingMask(new print_preview.Size(850, 1100));
        container.updateScaleTransform(pixelsPerInch / pointsPerInch);
        container.previewLoaded = true;
        Polymer.dom.flush();
      });
    }

    /**
     * @param {!NodeList<!PrintPreviewMarginControlElement>} controls
     * @return {!Promise} Promise that resolves when transitionend has fired
     *     for all of the controls.
     */
    function getAllTransitions(controls) {
      return Promise.all(Array.from(controls).map(
          control => test_util.eventToPromise('transitionend', control)));
    }

    /**
     * Simulates dragging the margin control.
     * @param {!PrintPreviewMarginControlElement} control The control to move.
     * @param {number} start The starting position for the control in pixels.
     * @param {number} end The ending position for the control in pixels.
     */
    function dragControl(control, start, end) {
      if (window.getComputedStyle(control)['pointer-events'] === 'none')
        return;

      let xStart = 0;
      let yStart = 0;
      let xEnd = 0;
      let yEnd = 0;
      switch (control.side) {
        case print_preview.ticket_items.CustomMarginsOrientation.TOP:
          yStart = start;
          yEnd = end;
          break;
        case print_preview.ticket_items.CustomMarginsOrientation.RIGHT:
          xStart = control.clipSize.width - start;
          xEnd = control.clipSize.width - end;
          break;
        case print_preview.ticket_items.CustomMarginsOrientation.BOTTOM:
          yStart = control.clipSize.height - start;
          yEnd = control.clipSize.height - end;
          break;
        case print_preview.ticket_items.CustomMarginsOrientation.LEFT:
          xStart = start;
          xEnd = end;
          break;
        default:
          assertNotReached();
      }
      // Simulate events in the same order they are fired by the browser.
      // Need to provide a valid |pointerId| for setPointerCapture() to not
      // throw an error.
      control.dispatchEvent(new PointerEvent(
          'pointerdown', {pointerId: 1, clientX: xStart, clientY: yStart}));
      control.dispatchEvent(new PointerEvent(
          'pointermove', {pointerId: 1, clientX: xEnd, clientY: yEnd}));
      control.dispatchEvent(new PointerEvent(
          'pointerup', {pointerId: 1, clientX: xEnd, clientY: yEnd}));
    }

    /**
     * Tests setting the margin control with its textbox.
     * @param {!PrintPreviewMarginControlElement} control The control.
     * @param {string} key The control's key in the custom margin setting.
     * @param {number} currentValue The current margin value in points.
     * @param {string} input The new textbox input for the margin.
     * @param {boolean} invalid Whether the new value is invalid.
     * @return {!Promise} Promise that resolves when the test is complete.
     */
    function testControlTextbox(control, key, currentValuePts, input, invalid) {
      const newValuePts = invalid ?
          currentValuePts :
          Math.round(parseFloat(input) * pointsPerInch);
      assertEquals(
          currentValuePts, container.getSettingValue('customMargins')[key]);
      controlTextbox = control.$.textbox.inputElement;
      controlTextbox.value = input;
      controlTextbox.dispatchEvent(
          new CustomEvent('input', {composed: true, bubbles: true}));

      return test_util.eventToPromise('text-change', control).then(() => {
        assertEquals(
            newValuePts, container.getSettingValue('customMargins')[key]);
        assertEquals(invalid, control.invalid);
      });
    }

    /*
     * Initializes the settings custom margins to some test values, and returns
     * a map with the values.
     * @return {!Map<!print_preview.ticket_items.CustomMarginsOrientation,
     *               number>}
     */
    function setupCustomMargins() {
      const orientationEnum =
          print_preview.ticket_items.CustomMarginsOrientation;
      const marginValues = new Map([
        [orientationEnum.TOP, 72], [orientationEnum.RIGHT, 36],
        [orientationEnum.BOTTOM, 108], [orientationEnum.LEFT, 18]
      ]);
      settings.customMargins.value = {
        marginTop: marginValues.get(orientationEnum.TOP),
        marginRight: marginValues.get(orientationEnum.RIGHT),
        marginBottom: marginValues.get(orientationEnum.BOTTOM),
        marginLeft: marginValues.get(orientationEnum.LEFT),
      };
      return marginValues;
    }

    /*
     * Tests that the custom margins and margin value are cleared when the
     * setting |settingName| is set to have value |newValue|.
     * @param {string} settingName The name of the setting to check.
     * @param {*} newValue The value to set the setting to.
     * @return {!Promise} Promise that resolves when the check is complete.
     */
    function validateMarginsClearedForSetting(settingName, newValue) {
      const marginValues = setupCustomMargins();
      return finishSetup().then(() => {
        // Simulate setting custom margins.
        container.set(
            'settings.margins.value',
            print_preview.ticket_items.MarginsTypeValue.CUSTOM);

        // Validate control positions are set based on the custom values.
        const controls = getControls();
        controls.forEach((control, index) => {
          const side = sides[index];
          assertEquals(side, control.side);
          assertEquals(marginValues.get(side), control.getPositionInPts());
        });

        // Simulate setting the media size.
        container.setSetting(settingName, newValue);
        container.previewLoaded = false;

        // Margins should be reset to default and custom margins values should
        // be cleared.
        expectEquals(
            print_preview.ticket_items.MarginsTypeValue.DEFAULT,
            container.getSettingValue('margins'));
        expectEquals(
            '{}', JSON.stringify(container.getSettingValue('customMargins')));

        // When preview loads, custom margins should still be empty, since
        // custom margins are not selected. We do not want to set the sticky
        // values until the user has selected custom margins.
        container.previewLoaded = true;
        expectEquals(
            '{}', JSON.stringify(container.getSettingValue('customMargins')));
      });
    }

    // Test that controls correctly appear when custom margins are selected and
    // disappear when the preview is loading.
    test(assert(TestNames.ControlsCheck), function() {
      return finishSetup()
          .then(() => {
            const controls = getControls();
            assertEquals(4, controls.length);

            // Controls are not visible when margin type DEFAULT is selected.
            controls.forEach(control => {
              assertEquals('0', window.getComputedStyle(control).opacity);
            });

            let onTransitionEnd = getAllTransitions(controls);
            // Controls become visible when margin type CUSTOM is selected.
            container.set(
                'settings.margins.value',
                print_preview.ticket_items.MarginsTypeValue.CUSTOM);

            // Wait for the opacity transitions to finish.
            return onTransitionEnd;
          })
          .then(function() {
            // Verify margins are correctly set based on previous value.
            assertEquals(
                defaultMarginPts,
                container.settings.customMargins.value.marginTop);
            assertEquals(
                defaultMarginPts,
                container.settings.customMargins.value.marginLeft);
            assertEquals(
                defaultMarginPts,
                container.settings.customMargins.value.marginRight);
            assertEquals(
                defaultMarginPts,
                container.settings.customMargins.value.marginBottom);

            // Verify there is one control for each side and that controls are
            // visible and positioned correctly.
            const controls = getControls();
            controls.forEach((control, index) => {
              assertFalse(control.invisible);
              assertEquals('1', window.getComputedStyle(control).opacity);
              assertEquals(sides[index], control.side);
              assertEquals(defaultMarginPts, control.getPositionInPts());
            });

            let onTransitionEnd = getAllTransitions(controls);

            // Disappears when preview is loading or an error message is shown.
            // Check that all the controls also disappear.
            container.previewLoaded = false;
            // Wait for the opacity transitions to finish.
            return onTransitionEnd;
          })
          .then(function() {
            const controls = getControls();
            controls.forEach((control, index) => {
              assertEquals('0', window.getComputedStyle(control).opacity);
              assertTrue(control.invisible);
            });
          });
    });

    // Tests that the margin controls can be correctly set from the sticky
    // settings.
    test(assert(TestNames.SetFromStickySettings), function() {
      return finishSetup().then(() => {
        const controls = getControls();

        // Simulate setting custom margins from sticky settings.
        container.set(
            'settings.margins.value',
            print_preview.ticket_items.MarginsTypeValue.CUSTOM);
        const marginValues = setupCustomMargins();
        container.notifyPath('settings.customMargins.value');
        Polymer.dom.flush();

        // Validate control positions have been updated.
        controls.forEach((control, index) => {
          const side = sides[index];
          assertEquals(side, control.side);
          assertEquals(marginValues.get(side), control.getPositionInPts());
        });
      });
    });

    // Test that dragging margin controls updates the custom margins setting.
    test(assert(TestNames.DragControls), function() {
      /**
       * Tests that the control can be moved from its current position (assumed
       * to be the default margins) to newPositionInPts by dragging it.
       * @param {!PrintPreviewMarginControlElement} control The control to test.
       * @param {number} index The index of this control in the container's list
       *     of controls.
       * @param {number} newPositionInPts The new position in points.
       */
      const testControl = function(control, index, newPositionInPts) {
        const oldValue = container.getSettingValue('customMargins');
        assertEquals(defaultMarginPts, oldValue[keys[index]]);

        // Compute positions in pixels.
        const oldPositionInPixels =
            defaultMarginPts * pixelsPerInch / pointsPerInch;
        const newPositionInPixels =
            newPositionInPts * pixelsPerInch / pointsPerInch;

        const whenDragChanged =
            test_util.eventToPromise('margin-drag-changed', container);
        dragControl(control, oldPositionInPixels, newPositionInPixels);
        return whenDragChanged.then(function() {
          const newValue = container.getSettingValue('customMargins');
          assertEquals(newPositionInPts, newValue[keys[index]]);
        });
      };

      return finishSetup().then(() => {
        const controls = getControls();
        container.set(
            'settings.margins.value',
            print_preview.ticket_items.MarginsTypeValue.CUSTOM);
        Polymer.dom.flush();


        // Wait for an animation frame. The position of the controls is set in
        // an animation frame, and this needs to be initialized before dragging
        // the control so that the computation of the new location is performed
        // with the correct initial margin offset.
        // Set all controls to 108 = 1.5" in points.
        window.requestAnimationFrame(function() {
          return testControl(controls[0], 0, 108)
              .then(testControl(controls[1], 1, 108))
              .then(testControl(controls[2], 2, 108))
              .then(testControl(controls[3], 3, 108));
        });
      });
    });

    // Test that setting the margin controls with their textbox inputs updates
    // the custom margins setting.
    test(assert(TestNames.SetControlsWithTextbox), function() {
      return finishSetup().then(() => {
        const controls = getControls();
        container.set(
            'settings.margins.value',
            print_preview.ticket_items.MarginsTypeValue.CUSTOM);
        Polymer.dom.flush();

        // Verify entering a new value updates the settings.
        // Then verify entering an invalid value invalidates the control and
        // does not update the settings.
        const newValue = '1.75';  // 1.75 inches
        const invalidValue = 'abc';
        const newMarginPts = Math.round(parseFloat(newValue) * pointsPerInch);
        return testControlTextbox(
                   controls[0], keys[0], defaultMarginPts, newValue, false)
            .then(
                () => testControlTextbox(
                    controls[1], keys[1], defaultMarginPts, newValue, false))
            .then(
                () => testControlTextbox(
                    controls[2], keys[2], defaultMarginPts, newValue, false))
            .then(
                () => testControlTextbox(
                    controls[3], keys[3], defaultMarginPts, newValue, false))
            .then(
                () => testControlTextbox(
                    controls[0], keys[0], newMarginPts, invalidValue, true))
            .then(
                () => testControlTextbox(
                    controls[1], keys[1], newMarginPts, invalidValue, true))
            .then(
                () => testControlTextbox(
                    controls[2], keys[2], newMarginPts, invalidValue, true))
            .then(
                () => testControlTextbox(
                    controls[3], keys[3], newMarginPts, invalidValue, true));
      });
    });

    // Test that if there is a custom margins sticky setting, it is restored
    // when margin setting changes.
    test(assert(TestNames.RestoreStickyMarginsAfterDefault), function() {
      const marginValues = setupCustomMargins();
      return finishSetup().then(() => {
        // Simulate setting custom margins.
        const controls = getControls();
        container.set(
            'settings.margins.value',
            print_preview.ticket_items.MarginsTypeValue.CUSTOM);

        // Validate control positions are set based on the custom values.
        controls.forEach((control, index) => {
          const side = sides[index];
          assertEquals(side, control.side);
          assertEquals(marginValues.get(side), control.getPositionInPts());
        });

        // Simulate setting minimum margins.
        container.set(
            'settings.margins.value',
            print_preview.ticket_items.MarginsTypeValue.MINIMUM);

        // Validate control positions still reflect the custom values.
        controls.forEach((control, index) => {
          const side = sides[index];
          assertEquals(side, control.side);
          assertEquals(marginValues.get(side), control.getPositionInPts());
        });
      });
    });

    // Test that if the media size changes, the custom margins are cleared.
    test(assert(TestNames.MediaSizeClearsCustomMargins), function() {
      return validateMarginsClearedForSetting(
                 'mediaSize', {height_microns: 200000, width_microns: 200000})
          .then(() => {
            // Simulate setting custom margins again.
            container.set(
                'settings.margins.value',
                print_preview.ticket_items.MarginsTypeValue.CUSTOM);

            // Validate control positions are initialized based on the default
            // values.
            const controls = getControls();
            controls.forEach((control, index) => {
              const side = sides[index];
              assertEquals(side, control.side);
              assertEquals(defaultMarginPts, control.getPositionInPts());
            });
          });
    });

    // Test that if the orientation changes, the custom margins are cleared.
    test(assert(TestNames.LayoutClearsCustomMargins), function() {
      return validateMarginsClearedForSetting('layout', true).then(() => {
        // Simulate setting custom margins again
        container.set(
            'settings.margins.value',
            print_preview.ticket_items.MarginsTypeValue.CUSTOM);

        // Validate control positions are initialized based on the default
        // values.
        const controls = getControls();
        controls.forEach((control, index) => {
          const side = sides[index];
          assertEquals(side, control.side);
          assertEquals(defaultMarginPts, control.getPositionInPts());
        });
      });
    });

    // Test that if the margins are not available, the custom margins setting is
    // not updated based on the document margins - i.e. PDFs do not change the
    // custom margins state.
    test(assert(TestNames.IgnoreDocumentMarginsFromPDF), function() {
      settings.margins.available = false;
      return finishSetup().then(() => {
        assertEquals(
            '{}', JSON.stringify(container.getSettingValue('customMargins')));
      });
    });

    // Test that if margins are not available but the user changes the media
    // size, the custom margins are cleared.
    test(assert(TestNames.MediaSizeClearsCustomMarginsPDF), function() {
      settings.margins.available = false;
      return validateMarginsClearedForSetting(
          'mediaSize', {height_microns: 200000, width_microns: 200000});
    });

  });

  return {
    suiteName: suiteName,
    TestNames: TestNames,
  };
});
