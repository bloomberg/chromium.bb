// Test that constructor for the node with name |nodeName| handles the
// various possible values for channelCount, channelCountMode, and
// channelInterpretation.

// The |should| parameter is the test function from new |Audit|.
function testAudioNodeOptions(should, context, nodeName, expectedNodeOptions) {
  if (expectedNodeOptions === undefined)
    expectedNodeOptions = {};
  let node;

  // Test that we can set channelCount and that errors are thrown for
  // invalid values
  let testChannelCount = 17;
  if (expectedNodeOptions.channelCount) {
    testChannelCount = expectedNodeOptions.channelCount.value;
  }
  should(
      () => {
        node = new window[nodeName](
            context, Object.assign({}, expectedNodeOptions.additionalOptions, {
              channelCount: testChannelCount
            }));
      },
      'new ' + nodeName + '(c, {channelCount: ' + testChannelCount + '}}')
      .notThrow();
  should(node.channelCount, 'node.channelCount').beEqualTo(testChannelCount);

  if (expectedNodeOptions.channelCount &&
      expectedNodeOptions.channelCount.isFixed) {
    // The channel count is fixed.  Verify that we throw an error if
    // we try to change it. Arbitrarily set the count to be one more
    // than the expected value.
    testChannelCount = expectedNodeOptions.channelCount.value + 1;
    should(
        () => {
          node = new window[nodeName](
              context,
              Object.assign(
                  {}, expectedNodeOptions.additionalOptions,
                  {channelCount: testChannelCount}));
        },
        'new ' + nodeName + '(c, {channelCount: ' + testChannelCount + '}}')
        .throw(expectedNodeOptions.channelCount.errorType || 'TypeError');
  } else {
    // The channel count is not fixed.  Try to set the count to invalid
    // values and make sure an error is thrown.
    let errorType = 'NotSupportedError';

    should(() => {
      node = new window[nodeName](
          context,
          Object.assign(
              {}, expectedNodeOptions.additionalOptions, {channelCount: 0}));
    }, 'new ' + nodeName + '(c, {channelCount: 0}}', ).throw(errorType);

    should(() => {
      node = new window[nodeName](
          context,
          Object.assign(
              {}, expectedNodeOptions.additionalOptions, {channelCount: 99}));
    }, 'new ' + nodeName + '(c, {channelCount: 99}}').throw(errorType);
  }

  // Test channelCountMode
  let testChannelCountMode = 'max';
  if (expectedNodeOptions.channelCountMode) {
    testChannelCountMode = expectedNodeOptions.channelCountMode.value;
  }
  should(
      () => {
        node = new window[nodeName](
            context, Object.assign({}, expectedNodeOptions.additionalOptions, {
              channelCountMode: testChannelCountMode
            }));
      },
      'new ' + nodeName + '(c, {channelCountMode: "' + testChannelCountMode +
          '"}')
      .notThrow();
  should(node.channelCountMode, 'node.channelCountMode')
      .beEqualTo(testChannelCountMode);

  if (expectedNodeOptions.channelCountMode &&
      expectedNodeOptions.channelCountMode.isFixed) {
    // Channel count mode is fixed.  Test setting to something else throws.
    let testChannelCountModeMap = {
      'max': 'clamped-max',
      'clamped-max': 'explicit',
      'explicit': 'max'
    };
    testChannelCountMode =
        testChannelCountModeMap[expectedNodeOptions.channelCountMode.value];
    should(
        () => {
          node = new window[nodeName](
              context,
              Object.assign(
                  {}, expectedNodeOptions.additionalOptions,
                  {channelCountMode: testChannelCountMode}));
        },
        'new ' + nodeName + '(c, {channelCountMode: "' + testChannelCountMode +
            '"}')
        .throw(expectedNodeOptions.channelCountMode.errorType);
  } else {
    // Mode is not fixed. Verify that we can set the mode to all valid
    // values, and that we throw for invalid values.

    should(() => {
      node = new window[nodeName](
          context, Object.assign({}, expectedNodeOptions.additionalOptions, {
            channelCountMode: 'clamped-max'
          }));
    }, 'new ' + nodeName + '(c, {channelCountMode: "clamped-max"}').notThrow();
    should(node.channelCountMode, 'node.channelCountMode after invalid setter')
        .beEqualTo('clamped-max');

    should(() => {
      node = new window[nodeName](
          context, Object.assign({}, expectedNodeOptions.additionalOptions, {
            channelCountMode: 'explicit'
          }));
    }, 'new ' + nodeName + '(c, {channelCountMode: "explicit"}').notThrow();
    should(node.channelCountMode, 'node.channelCountMode')
        .beEqualTo('explicit');

    should(
        () => {
          node = new window[nodeName](
              context,
              Object.assign(
                  {}, expectedNodeOptions.additionalOptions,
                  {channelCountMode: 'foobar'}));
        },
        'new ' + nodeName + '(c, {channelCountMode: "foobar"}')
        .throw('TypeError');
    should(node.channelCountMode, 'node.channelCountMode after invalid setter')
        .beEqualTo('explicit');
  }

  // Test channelInterpretation
  should(() => {
    node = new window[nodeName](
        context, Object.assign({}, expectedNodeOptions.additionalOptions, {
          channelInterpretation: 'speakers'
        }));
  }, 'new ' + nodeName + '(c, {channelInterpretation: "speakers"})').notThrow();
  should(node.channelInterpretation, 'node.channelInterpretation')
      .beEqualTo('speakers');

  should(() => {
    node = new window[nodeName](
        context, Object.assign({}, expectedNodeOptions.additionalOptions, {
          channelInterpretation: 'discrete'
        }));
  }, 'new ' + nodeName + '(c, {channelInterpretation: "discrete"})').notThrow();
  should(node.channelInterpretation, 'node.channelInterpretation')
      .beEqualTo('discrete');

  should(
      () => {
        node = new window[nodeName](
            context, Object.assign({}, expectedNodeOptions.additionalOptions, {
              channelInterpretation: 'foobar'
            }));
      },
      'new ' + nodeName + '(c, {channelInterpretation: "foobar"})')
      .throw('TypeError');
  should(
      node.channelInterpretation,
      'node.channelInterpretation after invalid setter')
      .beEqualTo('discrete');
}

function initializeContext(should) {
  let c;
  should(() => {
    c = new OfflineAudioContext(1, 1, 48000);
  }, 'context = new OfflineAudioContext(...)').notThrow();

  return c;
}

function testInvalidConstructor(should, name, context) {
  should(() => {
    new window[name]();
  }, 'new ' + name + '()').throw('TypeError');
  should(() => {
    new window[name](1);
  }, 'new ' + name + '(1)').throw('TypeError');
  should(() => {
    new window[name](context, 42);
  }, 'new ' + name + '(context, 42)').throw('TypeError');
}

function testDefaultConstructor(should, name, context, options) {
  let node;

  let message = options.prefix + ' = new ' + name + '(context';
  if (options.constructorOptions)
    message += ', ' + JSON.stringify(options.constructorOptions);
  message += ')'

  should(() => {
    node = new window[name](context, options.constructorOptions);
  }, message).notThrow();

  should(node instanceof window[name], options.prefix + ' instanceof ' + name)
      .beEqualTo(true);
  should(node.numberOfInputs, options.prefix + '.numberOfInputs')
      .beEqualTo(options.numberOfInputs);
  should(node.numberOfOutputs, options.prefix + '.numberOfOutputs')
      .beEqualTo(options.numberOfOutputs);
  should(node.channelCount, options.prefix + '.channelCount')
      .beEqualTo(options.channelCount);
  should(node.channelCountMode, options.prefix + '.channelCountMode')
      .beEqualTo(options.channelCountMode);
  should(node.channelInterpretation, options.prefix + '.channelInterpretation')
      .beEqualTo(options.channelInterpretation);

  return node;
}

function testDefaultAttributes(should, node, prefix, items) {
  items.forEach((item) => {
    let attr = node[item.name];
    if (attr instanceof AudioParam) {
      should(attr.value, prefix + '.' + item.name + '.value')
          .beEqualTo(item.value);
    } else {
      should(attr, prefix + '.' + item.name).beEqualTo(item.value);
    }
  });
}
