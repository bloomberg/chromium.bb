function assert_is_unit(unit, result) {
  assert_class_string(result, 'CSSUnitValue',
    'relative lengths must compute to a CSSUnitValue');
  assert_equals(result.unit, unit, 'unit');
}

function assert_is_calc_sum(result) {
  assert_class_string(result, 'CSSMathSum',
    'specified calc must be a CSSMathSum');
}

const gTestSyntaxExamples = {
  '<length>': {
    description: 'a length',
    examples: [
      {
        description: "zero px",
        input: new CSSUnitValue(0, 'px')
      },
      {
        description: "a negative em",
        input: new CSSUnitValue(-3.14, 'em'),
        // 'ems' are relative units, so just check that it computes to px
        defaultComputed: result => assert_is_unit('px', result)
      },
      {
        description: "a positive cm",
        input: new CSSUnitValue(3.14, 'cm'),
        // 'cms' are relative units, so just check that it computes to px
        defaultComputed: result => assert_is_unit('px', result)
      },
      {
        description: "a calc length",
        input: new CSSMathSum(new CSSUnitValue(0, 'px'), new CSSUnitValue(0, 'em')),
        // Specified/computed calcs are usually simplified.
        // FIXME: Test this properly
        defaultSpecified: assert_is_calc_sum,
        defaultComputed: result => assert_is_unit('px', result)
      }
    ],
  },
  '<percentage>': {
    description: 'a percent',
    examples: [
      {
        description: "zero percent",
        input: new CSSUnitValue(0, 'percent')
      },
      {
        description: "a negative percent",
        input: new CSSUnitValue(-3.14, 'percent')
      },
      {
        description: "a positive percent",
        input: new CSSUnitValue(3.14, 'percent')
      },
      {
        description: "a calc percent",
        input: new CSSMathSum(new CSSUnitValue(0, 'percent'), new CSSUnitValue(0, 'percent')),
        // Specified/computed calcs are usually simplified.
        // FIXME: Test this properly
        defaultSpecified: assert_is_calc_sum,
        defaultComputed: result => assert_is_unit('percent', result)
      }
    ],
  },
  '<time>': {
    description: 'a time',
    examples: [
      {
        description: "zero seconds",
        input: new CSSUnitValue(0, 's')
      },
      {
        description: "negative milliseconds",
        input: new CSSUnitValue(-3.14, 'ms')
      },
      {
        description: "positive seconds",
        input: new CSSUnitValue(3.14, 's')
      },
      {
        description: "a calc time",
        input: new CSSMathSum(new CSSUnitValue(0, 's'), new CSSUnitValue(0, 'ms')),
        // Specified/computed calcs are usually simplified.
        // FIXME: Test this properly
        defaultSpecified: assert_is_calc_sum,
        defaultComputed: result => assert_is_unit('s', result)
      }
    ],
  },
  '<position>': {
    description: 'a position',
    examples: [
      {
        decription: "origin position",
        input: new CSSPositionValue(new CSSUnitValue(0, 'px'), new CSSUnitValue(0, 'px'))
      }
    ],
  },
  '<image>': {
    description: 'an image',
    examples: [
      {
        description: "a PNG image",
        input: new CSSURLImageValue('/media/1x1.png'),
        defaultComputed: result => {
          // URLs compute to absolute URLs
          assert_true(result instanceof CSSURLImageValue,
            'Computed value should be a CSSURLImageValue');
          assert_true(result.url.endsWith('/media/1x1.png'),
            'Computed value should be an absolute URL');
        }
      }
    ],
  },
  '<transform>': {
    description: 'a transform',
    examples: [
      {
        description: 'a transform containing only a translate',
        input: new CSSTransformValue([
          new CSSTranslate(
            new CSSUnitValue(0, 'px'),
            new CSSUnitValue(1, 'px'),
            new CSSUnitValue(2, 'px'),
          )
        ]),
      }
    ],
  },
};

// Test setting a value in a style map and then getting it from the inline and
// computed styles.
function testPropertyValid(propertyName, examples, specified, computed, description) {
  test(t => {
    let element = createDivWithStyle(t);

    for (const example of examples) {
      element.attributeStyleMap.set(propertyName, example.input);

      // specified style
      const specifiedResult = element.attributeStyleMap.get(propertyName);
      if (specified || example.defaultSpecified) {
        (specified || example.defaultSpecified)(specifiedResult);
      } else {
        assert_not_equals(specifiedResult, null,
          'Specified value must not be null');
        assert_true(specifiedResult instanceof CSSStyleValue,
          'Specified value must be a CSSStyleValue');
        assert_style_value_equals(specifiedResult, example.input,
          `Setting ${example.description} and getting its specified value`);
      }

      // computed style
      const computedResult = element.computedStyleMap().get(propertyName);
      if (computed || example.defaultComputed) {
        (computed || example.defaultComputed)(computedResult);
      } else {
        assert_not_equals(computedResult, null,
          'Computed value must not be null');
        assert_true(computedResult instanceof CSSStyleValue,
          'Computed value must be a CSSStyleValue');
        assert_style_value_equals(computedResult, example.input,
          `Setting ${example.description} and getting its computed value`);
      }
    }
  }, `Can set '${propertyName}' to ${description}`);
}

// Test that styleMap.set throws for invalid values
function testPropertyInvalid(propertyName, examples, description) {
  test(t => {
    let styleMap = createInlineStyleMap(t);
    for (const example of examples) {
      assert_throws(new TypeError(), () => styleMap.set(propertyName, example.input));
    }
  }, `Setting '${propertyName}' to ${description} throws TypeError`);
}

function createKeywordExample(keyword) {
  return {
    description: `the '${keyword}' keyword`,
    examples: [ { input: new CSSKeywordValue(keyword) } ]
  };
}

// Run a battery of StylePropertyMap tests on |propertyName|.
// Second argument is a list of test cases. A test case has the form:
//
// {
//   syntax: "<length>",
//   specified: /* a callback */ (optional)
//   computed: /* a callback */ (optional)
// }
//
// If a callback is passed to |specified|, then the callback will be passed
// the result of calling get() on the inline style map (specified values).
// The callback should check if the result is expected using assert_* functions.
// If no callback is passed, then we assert that the result is the same as
// the input.
//
// Same goes for |computed|, but with the computed style map (computed values).
function runPropertyTests(propertyName, testCases) {
  let syntaxTested = new Set();

  for (const testCase of testCases) {
    // Retrieve test examples for this test case's syntax. If the syntax
    // looks like a keyword, then create an example on the fly.
    const syntaxExamples = testCase.syntax.match(/^[a-z\-]+$/) ?
      createKeywordExample(testCase.syntax) :
      gTestSyntaxExamples[testCase.syntax];

    if (!syntaxExamples)
      throw new Error(`'${testCase.syntax}' is not a valid CSS component`);

    testPropertyValid(propertyName,
      syntaxExamples.examples,
      testCase.specified,
      testCase.computed,
      syntaxExamples.description);

    syntaxTested.add(testCase.syntax);
  }

  // Also test that styleMap.set rejects invalid CSSStyleValues.
  for (const [syntax, syntaxExamples] of Object.entries(gTestSyntaxExamples)) {
    if (!syntaxTested.has(syntax)) {
      testPropertyInvalid(propertyName,
        syntaxExamples.examples,
        syntaxExamples.description);
    }
  }
}
