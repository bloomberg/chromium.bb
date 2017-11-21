/**
 * A standard set of tests for using a single property in an
 *     inline StylePropertyMap
 *     (https://www.w3.org/TR/css-typed-om-1/#the-stylepropertymap).
 *
 * Create a config object containing {
 *   validKeywords: array of strings,
 *   validObjects: array of CSSStyleValue instances that are valid for the
 *       property,
 *   validStringMappings: object containing a mapping of string to
 *       CSSStyleValues for things expressable in string CSS, but are expressed
 *       the same as something else in Typed OM.
 *   supportsMultiple: boolean; whether the property supports a list of
 *       properties,
 *   invalidObjects: array of CSSStyleValue instances that are invalid for the
 *       property
 * }
 *
 * Then, call runInlineStylePropertyMapTests to test the behavior of an inline
 * StylePropertyMap for one CSS property for an element.
 *
 * Note that CSS-wide keywords, and non-CSSStyleValue invalid types do not need
 * to be specified.
 *
 * If necessary, you can continue to define non-standard tests in your html
 * file as usual.
 */

function runInlineStylePropertyMapTests(config) {
  let element = document.createElement('div');
  document.documentElement.appendChild(element);
  if (!config.validKeywords) {
    throw new Error('Must specify valid keywords (may be the empty list if ' +
          'only css-wide keywords apply)');
  }
  let validKeywords = config.validKeywords.concat([
    // CSS-wide keywords
    'initial',
    'inherit',
    'unset'
  ]);
  let validObjects = config.validObjects;
  let validStringMappings = config.validStringMappings ?
      config.validStringMappings : {};
  let invalidObjects = config.invalidObjects.concat([
    // No properties should take these values
    null,
    undefined,
    true,
    false,
    1,
    'hello',
    {},
    new CSSKeywordValue('notAKeyword')
  ]);
  let validObject = validObjects.length ?
      validObjects[0] : new CSSKeywordValue(validKeywords[0]);

  let styleMap = element.attributeStyleMap;
  runSetterTests(
      config.property, validKeywords, validObjects, invalidObjects, element);
  runGetterTests(config.property, validKeywords, validObjects,
      validStringMappings, element);
  runGetAllTests(
      config.property, validObject, element, config.supportsMultiple);
  runDeletionTests(config.property, validObject, element);
  runGetPropertiesTests(config.property, validObject, element);
  if (config.supportsMultiple) {
    runSequenceSetterTests(
        config.property, validObject, invalidObjects[0], element);
    runAppendTests(
        config.property, validObject, invalidObjects[0], element);
  } else {
    runMultipleValuesNotSupportedTests(
        config.property, validObject, element);
  }
}

function runSetterTests(
    propertyName, validKeywords, validObjects, invalidObjects, element) {
  for (let keyword of validKeywords) {
    test(function() {
      element.style = '';
      element.attributeStyleMap.set(propertyName, new CSSKeywordValue(keyword));
      assert_equals(element.style[propertyName], keyword);
      // Force a style recalc to check for crashes in style recalculation.
      getComputedStyle(element)[propertyName];
      assert_equals(element.style[propertyName], keyword);
    }, 'Setting ' + propertyName + ' to ' + keyword);
  }
  for (let validObject of validObjects) {
    test(function() {
      element.style = '';
      element.attributeStyleMap.set(propertyName, validObject);
      assert_equals(element.style[propertyName], validObject.toString());
      // Force a style recalc to check for crashes in style recalculation.
      getComputedStyle(element)[propertyName];
      assert_equals(element.style[propertyName], validObject.toString());
    }, 'Setting ' + propertyName + ' to ' + validObject.constructor.name +
        ' with value ' +  validObject.toString());
  }

  // Negative tests
  for (let invalidObject of invalidObjects) {
    let name = invalidObject instanceof CSSStyleValue ?
        invalidObject.constructor.name + ' "' + invalidObject.toString() + '"' :
        invalidObject;
    test(function() {
      assert_throws(new TypeError(), function() {
        element.attributeStyleMap.set(propertyName, invalidObject);
      });
    }, 'Setting ' + propertyName + ' to invalid value ' + name + ' throws');
  }
}

function runGetterTests(
    propertyName, validKeywords, validObjects, validStringMappings, element) {
  for (let keyword of validKeywords) {
    test(function() {
      element.style[propertyName] = keyword;

      let result = element.attributeStyleMap.get(propertyName);
      assert_equals(result.constructor.name, CSSKeywordValue.name);
      assert_equals(result.toString(), keyword);
    }, 'Getting ' + propertyName + ' when it is set to ' + keyword);
  }
  for (let validObject of validObjects) {
    test(function() {
      element.style[propertyName] = validObject.toString();

      let result = element.attributeStyleMap.get(propertyName);
      assert_equals(result.constructor.name, validObject.constructor.name,
          'typeof result');
      assert_equals(result.toString(), validObject.toString());
    }, 'Getting ' + propertyName + ' with a ' + validObject.constructor.name +
        ' whose value is ' + validObject.toString());
  }
  for (let cssText in validStringMappings) {
    test(function() {
      element.style[propertyName] = cssText;

      let result = element.attributeStyleMap.get(propertyName);
      assert_equals(result.constructor.name,
          validStringMappings[cssText].constructor.name,
          'typeof result');
      assert_equals(result.toString(), validStringMappings[cssText].toString());
    }, 'Getting ' + propertyName + ' when it is set to "' +
        cssText + '" via a string');
  }
}

function runSequenceSetterTests(
    propertyName, validObject, invalidObject, element) {
  test(function() {
    element.style = '';
    element.attributeStyleMap.set(propertyName, validObject, validObject);
    assert_equals(
        element.style[propertyName], validObject.toString() + ', ' +
        validObject.toString());
    // Force a style recalc to check for crashes in style recalculation.
    getComputedStyle(element)[propertyName];
    assert_equals(
        element.style[propertyName], validObject.toString() + ', ' +
        validObject.toString());
  }, 'Set ' + propertyName + ' to a sequence');

  test(function() {
    assert_throws(new TypeError(), function() {
      element.attributeStyleMap.set(propertyName, validObject, invalidObject);
    });
  }, 'Set ' + propertyName + ' to a sequence containing an invalid type');
}

function runAppendTests(
    propertyName, validObject, invalidObject, element) {
  test(function() {
    element.style = '';

    element.attributeStyleMap.append(propertyName, validObject);
    assert_equals(element.style[propertyName], validObject.toString());

    element.attributeStyleMap.append(propertyName, validObject);
    assert_equals(
        element.style[propertyName], validObject.toString() + ', ' +
        validObject.toString());
    // Force a style recalc to check for crashes in style recalculation.
    getComputedStyle(element)[propertyName];
    assert_equals(
        element.style[propertyName], validObject.toString() + ', ' +
        validObject.toString());
  }, 'Appending a ' + validObject.constructor.name + ' to ' + propertyName);

  test(function() {
    element.style = '';

    element.attributeStyleMap.append(propertyName, validObject, validObject);
    assert_equals(
        element.style[propertyName], validObject.toString() + ', ' +
        validObject.toString());
    // Force a style recalc to check for crashes in style recalculation.
    getComputedStyle(element)[propertyName];
    assert_equals(
        element.style[propertyName], validObject.toString() + ', ' +
        validObject.toString());
  }, 'Append a sequence to ' + propertyName);

  // Negative tests
  test(function() {
    assert_throws(new TypeError(), function() {
      element.attributeStyleMap.append(propertyName, invalidObject);
    });
  }, 'Appending an invalid value to ' + propertyName);

  test(function() {
    assert_throws(new TypeError(), function() {
      element.attributeStyleMap.append(propertyName, validObject, invalidObject);
    });
  }, 'Append a sequence containing an invalid value to ' + propertyName);
}

function runGetAllTests(
    propertyName, validObject, element, supportsMultiple) {
  test(function() {
    element.style = '';
    assert_array_equals(element.attributeStyleMap.getAll(propertyName), []);

    element.style[propertyName] = validObject.toString();
    let result = element.attributeStyleMap.getAll(propertyName);
    assert_equals(result.length, 1,
        'Expected getAll to retrieve an array containing a ' +
        'single CSSStyleValue');
    assert_equals(result[0].constructor.name, validObject.constructor.name,
        'Returned type is incorrect:');
    assert_equals(result[0].toString(), validObject.toString());
  }, 'getAll for single-valued ' + propertyName);

  if (supportsMultiple) {
    test(function() {
      element.style = '';
      element.attributeStyleMap.set(propertyName, validObject, validObject);
      let result = element.attributeStyleMap.getAll(propertyName);
      assert_equals(result.length, 2,
          'Expected getAll to return an array containing two instances ' +
          'of ' + validObject.constructor.name);
      assert_equals(result[0].constructor.name, validObject.constructor.name);
      assert_equals(result[1].constructor.name, validObject.constructor.name);
      assert_equals(result[0].toString(), validObject.toString());
      assert_equals(result[1].toString(), validObject.toString());
    }, 'getAll for list-valued ' + propertyName);
  }
}

function runDeletionTests(propertyName, validObject, element) {
  test(function() {
    element.style[propertyName] = validObject.toString();

    assert_not_equals(element.attributeStyleMap.get(propertyName), null);

    element.attributeStyleMap.delete(propertyName);
    assert_equals(element.style[propertyName], '');
    assert_equals(element.attributeStyleMap.get(propertyName), null);
    // Force a style recalc to check for crashes in style recalculation.
    getComputedStyle(element)[propertyName];
    assert_equals(element.style[propertyName], '');
    assert_equals(element.attributeStyleMap.get(propertyName), null);
  }, 'Delete ' + propertyName + ' removes the value from the styleMap');
}

function runGetPropertiesTests(propertyName, validObject, element) {
  test(function() {
    element.style = '';
    assert_array_equals(element.attributeStyleMap.getProperties(), []);
    element.attributeStyleMap.set(propertyName, validObject);
    assert_array_equals(element.attributeStyleMap.getProperties(), [propertyName]);
    // Force a style recalc to check for crashes in style recalculation.
    getComputedStyle(element)[propertyName];
    assert_array_equals(element.attributeStyleMap.getProperties(), [propertyName]);
  }, propertyName + ' shows up in getProperties');
}

function runMultipleValuesNotSupportedTests(
    propertyName, validObject, element) {
  test(function() {
    element.style = '';
    assert_throws(new TypeError(), function() {
      element.attributeStyleMap.set(propertyName, validObject, validObject);
    });
  }, 'Setting ' + propertyName + ' to a sequence throws');

  test(function() {
    element.style = '';
    assert_throws(new TypeError(), function() {
      element.attributeStyleMap.append(propertyName, validObject);
    });
  }, 'Appending to ' + propertyName + ' throws');
}

