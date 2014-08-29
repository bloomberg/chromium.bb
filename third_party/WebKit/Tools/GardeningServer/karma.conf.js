// Karma configuration
// Generated on Mon Jul 21 2014 15:27:46 GMT-0700 (PDT)

module.exports = function(config) {
  config.set({

    // base path that will be used to resolve all patterns (eg. files, exclude)
    basePath: '.',

    // frameworks to use
    // available frameworks: https://npmjs.org/browse/keyword/karma-adapter
    frameworks: ['mocha'],

    // mocha setup
    client: {
      mocha: {
        ui: 'bdd',
        checkLeaks: true,
        globals: ['net']
      },
      captureConsole: true
    },

    // list of files / patterns to load in the browser
    files: [
      // dependencies
      'bower_components/platform/platform.js',
      'test/karma-loader.html',
      'bower_components/chai/chai.js',
      'bower_components/sugar/release/sugar-full.development.js',
      {pattern: 'bower_components/**/*.{js,html,css,map}', included: false},
      'node_modules/mocha/mocha.js',
      // sources
      'polymer-load-warning.html',
      {pattern: 'base/*.html', included: false},
      {pattern: 'lib/*.html', included: false},
      {pattern: 'model/*.html', included: false},
      'scripts/*.js',
      {pattern: 'ui/*.html', included: false},
      // tests
      'lib/test/*.html',
      'model/test/*.html',
      'scripts/test/*.html',
      'ui/test/*',
      'bower_components/polymer/polymer.html'
    ],

    // list of files to exclude
    exclude: [
    ],

    // preprocess matching files before serving them to the browser
    // available preprocessors: https://npmjs.org/browse/keyword/karma-preprocessor
    preprocessors: {
    },

    // test results reporter to use
    // possible values: 'dots', 'progress'
    // available reporters: https://npmjs.org/browse/keyword/karma-reporter
    reporters: ['progress'],

    // web server port
    port: 9876,

    // enable / disable colors in the output (reporters and logs)
    colors: true,

    // level of logging
    // possible values: config.LOG_DISABLE || config.LOG_ERROR || config.LOG_WARN || config.LOG_INFO || config.LOG_DEBUG
    logLevel: config.LOG_ERROR,

    // enable / disable watching file and executing tests whenever any file changes
    autoWatch: true,

    // start these browsers
    // available browser launchers: https://npmjs.org/browse/keyword/karma-launcher
    browsers: ['Chrome'],

    // Continuous Integration mode
    // if true, Karma captures browsers, runs the tests and exits
    singleRun: false,

    plugins: [
      'karma-mocha',
      'karma-chrome-launcher'
    ]
  });
};
