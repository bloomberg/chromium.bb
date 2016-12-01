/**
 * @fileoverview A standard set of tests for using a single property in an
 *     inline StylePropertyMap
 *     (https://www.w3.org/TR/css-typed-om-1/#the-stylepropertymap).
 *
 * Create a config object containing {
 *   validKeywords: array of strings,
 *   validObjects: array of CSSStyleValue instances that are valid for the
 *       property,
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
  let validKeywords = config.validKeywords.concat([
    // CSS-wide keywords
    'initial',
    'inherit',
    'unset'
  ]);
  let validObjects = config.validObjects;
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

  let styleMap = element.styleMap;
  runSetterTests(
      config.property, validKeywords, validObjects, invalidObjects, element);
  runGetterTests(config.property, validKeywords, validObjects, element);
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
      element.styleMap.set(propertyName, new CSSKeywordValue(keyword));
      assert_equals(element.style[propertyName], keyword);
    }, 'Setting ' + propertyName + ' to ' + keyword);
  }
  for (let validObject of validObjects) {
    test(function() {
      element.style = '';

      element.styleMap.set(propertyName, validObject);
      assert_equals(element.style[propertyName], validObject.cssText);
    }, 'Setting ' + propertyName + ' to ' + validObject.constructor.name +
        ' with value ' +  validObject.cssText);
  }

  // Negative tests
  for (let invalidObject of invalidObjects) {
    let name = invalidObject instanceof CSSStyleValue ?
        invalidObject.constructor.name : invalidObject;
    test(function() {
      assert_throws(new TypeError(), function() {
        element.styleMap.set(propertyName, invalidObject);
      });
    }, 'Setting ' + propertyName + ' to invalid value ' + name + ' throws');
  }
}

function runGetterTests(
    propertyName, validKeywords, validObjects, element) {
  for (let keyword of validKeywords) {
    test(function() {
      element.style[propertyName] = keyword;

      let result = element.styleMap.get(propertyName);
      assert_true(result instanceof CSSKeywordValue,
          'result instanceof CSSKeywordValue:');
      assert_equals(result.cssText, keyword);
    }, 'Getting ' + propertyName + ' when it is set to ' + keyword);
  }
  for (let validObject of validObjects) {
    test(function() {
      element.style[propertyName] = validObject.cssText;

      let result = element.styleMap.get(propertyName);
      assert_equals(result.constructor.name, validObject.constructor.name,
          'typeof result');
      assert_equals(result.cssText, validObject.cssText);
    }, 'Getting ' + propertyName + ' with a ' + validObject.constructor.name +
        ' whose value is ' + validObject.cssText);
  }
}

function runSequenceSetterTests(
    propertyName, validObject, invalidObject, element) {
  test(function() {
    element.style = '';

    element.styleMap.set(propertyName, [validObject, validObject]);
    assert_equals(
        element.style[propertyName], validObject.cssText + ', ' +
        validObject.cssText);
  }, 'Set ' + propertyName + ' to a sequence');

  test(function() {
    let sequence = [validObject, invalidObject];
    assert_throws(new TypeError(), function() {
      element.styleMap.set(propertyName, sequence);
    });
  }, 'Set ' + propertyName + ' to a sequence containing an invalid type');
}

function runAppendTests(
    propertyName, validObject, invalidObject, element) {
  test(function() {
    element.style = '';

    element.styleMap.append(propertyName, validObject);
    assert_equals(element.style[propertyName], validObject.cssText);

    element.styleMap.append(propertyName, validObject);
    assert_equals(
        element.style[propertyName], validObject.cssText + ', ' +
        validObject.cssText);
  }, 'Appending a ' + validObject.constructor.name + ' to ' + propertyName);

  test(function() {
    element.style = '';

    element.styleMap.append(propertyName, [validObject, validObject]);
    assert_equals(
        element.style[propertyName], validObject.cssText + ', ' +
        validObject.cssText);
  }, 'Append a sequence to ' + propertyName);

  // Negative tests
  test(function() {
    assert_throws(new TypeError(), function() {
      element.styleMap.append(propertyName, invalidObject);
    });
  }, 'Appending an invalid value to ' + propertyName);

  test(function() {
    let sequence = [validObject, invalidObject];
    assert_throws(new TypeError(), function() {
      element.styleMap.append(propertyName, sequence);
    });
  }, 'Append a sequence containing an invalid value to ' + propertyName);
}

function runGetAllTests(
    propertyName, validObject, element, supportsMultiple) {
  test(function() {
    element.style = '';
    assert_array_equals(element.styleMap.getAll(propertyName), []);

    element.style[propertyName] = validObject.cssText;
    let result = element.styleMap.getAll(propertyName);
    assert_equals(result.length, 1,
        'Expected getAll to retrieve an array containing a ' +
        'single CSSStyleValue');
    assert_equals(result[0].constructor.name, validObject.constructor.name,
        'Returned type is incorrect:');
    assert_equals(result[0].cssText, validObject.cssText);
  }, 'getAll for single-valued ' + propertyName);

  if (supportsMultiple) {
    test(function() {
      element.style = '';
      element.styleMap.set(propertyName, [validObject, validObject]);
      let result = element.styleMap.getAll(propertyName);
      assert_equals(result.length, 2,
          'Expected getAll to return an array containing two instances ' +
          'of CSSStyleValue');
      assert_true(result[0] instanceof CSSStyleValue,
          'Expected first result to be an instance of CSSStyleValue');
      assert_true(result[1] instanceof CSSStyleValue,
          'Expected second result to be an instance of CSSStyleValue');
      assert_equals(result[0].constructor.name, validObject.constructor.name);
      assert_equals(result[1].constructor.name, validObject.constructor.name);
      assert_equals(result[0].cssText, validObject.cssText);
      assert_equals(result[1].cssText, validObject.cssText);
    }, 'getAll for list-valued ' + propertyName);
  }
}

function runDeletionTests(propertyName, validObject, element) {
  test(function() {
    element.style[propertyName] = validObject.cssText;

    assert_not_equals(element.styleMap.get(propertyName), null);

    element.styleMap.delete(propertyName);
    assert_equals(element.style[propertyName], '');
    assert_equals(element.styleMap.get(propertyName), null);
  }, 'Delete ' + propertyName + ' removes the value form the styleMap');
}

function runGetPropertiesTests(propertyName, validObject, element) {
  test(function() {
    element.style = '';
    assert_array_equals(element.styleMap.getProperties(), []);

    element.styleMap.set(propertyName, validObject);

    assert_array_equals(element.styleMap.getProperties(), [propertyName]);
  }, propertyName + ' shows up in getProperties');
}

function runMultipleValuesNotSupportedTests(
    propertyName, validObject, element) {
  test(function() {
    element.style = '';
    assert_throws(new TypeError(), function() {
      element.styleMap.set(propertyName, [validObject, validObject]);
    });
  }, 'Setting ' + propertyName + ' to a sequence throws');

  test(function() {
    element.style = '';
    assert_throws(new TypeError(), function() {
      element.styleMap.append(propertyName, validObject);
    });
  }, 'Appending to ' + propertyName + ' throws');
}

