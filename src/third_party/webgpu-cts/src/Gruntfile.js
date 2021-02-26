/* eslint-disable node/no-unpublished-require */
/* eslint-disable prettier/prettier */
/* eslint-disable no-console */

module.exports = function (grunt) {
  // Project configuration.
  grunt.initConfig({
    pkg: grunt.file.readJSON('package.json'),

    clean: {
      out: ['out/', 'out-wpt/'],
    },

    run: {
      'generate-version': {
        cmd: 'node',
        args: ['tools/gen_version'],
      },
      'generate-listings': {
        cmd: 'node',
        args: ['tools/gen_listings', 'webgpu', 'unittests', 'demo'],
      },
      'generate-wpt-cts-html': {
        cmd: 'node',
        args: ['tools/gen_wpt_cts_html', 'out-wpt/cts.html', 'src/common/templates/cts.html'],
      },
      unittest: {
        cmd: 'node',
        args: ['tools/run', 'unittests:*'],
      },
      'build-out': {
        cmd: 'node',
        args: [
          'node_modules/@babel/cli/bin/babel',
          '--extensions=.ts',
          '--source-maps=true',
          '--out-dir=out/',
          'src/',
        ],
      },
      'build-out-wpt': {
        cmd: 'node',
        args: [
          'node_modules/@babel/cli/bin/babel',
          '--extensions=.ts',
          '--source-maps=false',
          '--delete-dir-on-start',
          '--out-dir=out-wpt/',
          'src/',
          '--only=src/common/framework/',
          '--only=src/common/runtime/helper/',
          '--only=src/common/runtime/wpt.ts',
          '--only=src/webgpu/',
          // These files will be generated, instead of compiled from TypeScript.
          '--ignore=src/common/framework/version.ts',
          '--ignore=src/webgpu/listing.ts',
        ],
      },
      lint: {
        cmd: 'node',
        args: ['node_modules/eslint/bin/eslint', 'src/**/*.ts', '--max-warnings=0'],
      },
      fix: {
        cmd: 'node',
        args: ['node_modules/eslint/bin/eslint', 'src/**/*.ts', '--fix'],
      },
      'autoformat-out-wpt': {
        cmd: 'node',
        args: ['node_modules/prettier/bin-prettier', '--loglevel=warn', '--write', 'out-wpt/**/*.js'],
      }
    },

    watch: {
      src: {
        files: ['src/**/*'],
        tasks: ['build-standalone', 'ts:check', 'run:lint'],
        options: {
          spawn: false,
        }
      }
    },

    copy: {
      'out-wpt-generated': {
        files: [
          { expand: true, cwd: 'out', src: 'common/framework/version.js', dest: 'out-wpt/' },
          { expand: true, cwd: 'out', src: 'webgpu/listing.js', dest: 'out-wpt/' },
        ],
      },
      glslang: {
        files: [
          {
            expand: true,
            cwd: 'node_modules/@webgpu/glslang/dist/web-devel',
            src: 'glslang.{js,wasm}',
            dest: 'out/third_party/glslang_js/lib/',
          },
        ],
      },
      'out-wpt-htmlfiles': {
        files: [
          { expand: true, cwd: 'src', src: 'webgpu/**/*.html', dest: 'out-wpt/' },
        ],
      },
    },

    'http-server': {
      '.': {
        root: '.',
        port: 8080,
        host: '127.0.0.1',
        cache: -1,
      },
      'background': {
        root: '.',
        port: 8080,
        host: '127.0.0.1',
        cache: -1,
        runInBackground: true,
        logFn(req, res, error) {
          // Only log errors to not spam the console.
          if (error) {
            console.error(error);
          }
        },
      },
    },

    ts: {
      check: {
        tsconfig: {
          tsconfig: 'tsconfig.json',
          passThrough: true,
        },
      },
    },
  });

  grunt.loadNpmTasks('grunt-contrib-clean');
  grunt.loadNpmTasks('grunt-contrib-copy');
  grunt.loadNpmTasks('grunt-http-server');
  grunt.loadNpmTasks('grunt-run');
  grunt.loadNpmTasks('grunt-ts');
  grunt.loadNpmTasks('grunt-contrib-watch');

  grunt.event.on('watch', (action, filepath) => {
    const buildArgs = grunt.config(['run', 'build-out', 'args']);
    buildArgs[buildArgs.length - 1] = filepath;
    grunt.config(['run', 'build-out', 'args'], buildArgs);

    const lintArgs = grunt.config(['run', 'lint', 'args']);
    lintArgs[lintArgs.length - 1] = filepath;
    grunt.config(['run', 'lint', 'args'], lintArgs);
  });

  const helpMessageTasks = [];
  function registerTaskAndAddToHelp(name, desc, deps) {
    grunt.registerTask(name, deps);
    addExistingTaskToHelp(name, desc);
  }
  function addExistingTaskToHelp(name, desc) {
    helpMessageTasks.push({ name, desc });
  }

  grunt.registerTask('set-quiet-mode', () => {
    grunt.log.write('Running tasks');
    require('quiet-grunt');
  });

  grunt.registerTask('build-standalone', 'Build out/ (no checks, no WPT)', [
    'run:build-out',
    'copy:glslang',
    'run:generate-version',
    'run:generate-listings',
  ]);
  grunt.registerTask('build-wpt', 'Build out/ (no checks)', [
    'run:build-out-wpt',
    'run:autoformat-out-wpt',
    'copy:glslang',
    'run:generate-version',
    'run:generate-listings',
    'copy:out-wpt-generated',
    'copy:out-wpt-htmlfiles',
    'run:generate-wpt-cts-html',
  ]);
  grunt.registerTask('build-done-message', () => {
    process.stderr.write('\nBuild completed! Running checks/tests');
  });

  registerTaskAndAddToHelp('pre', 'Run all presubmit checks: standalone+wpt+typecheck+unittest+lint', [
    'set-quiet-mode',
    'clean',
    'build-standalone',
    'build-wpt',
    'build-done-message',
    'ts:check',
    'run:unittest',
    'run:lint',
  ]);
  registerTaskAndAddToHelp('test', 'Quick development build: standalone+typecheck+unittest', [
    'set-quiet-mode',
    'build-standalone',
    'build-done-message',
    'ts:check',
    'run:unittest',
  ]);
  registerTaskAndAddToHelp('wpt', 'Build for WPT: wpt+typecheck+unittest', [
    'set-quiet-mode',
    'build-wpt',
    'build-done-message',
    'ts:check',
    'run:unittest',
  ]);
  registerTaskAndAddToHelp('check', 'Typecheck and lint', [
    'set-quiet-mode',
    'ts:check',
    'run:lint',
  ]);

  registerTaskAndAddToHelp('dev', 'Start the dev server, and watch for changes', [
    'build-standalone',
    'http-server:background',
    'watch',
  ]);

  registerTaskAndAddToHelp('serve', 'Serve out/ on 127.0.0.1:8080', ['http-server:.']);
  registerTaskAndAddToHelp('fix', 'Fix lint and formatting', ['run:fix']);

  addExistingTaskToHelp('clean', 'Clean out/ and out-wpt/');

  grunt.registerTask('default', '', () => {
    console.error('\nAvailable tasks (see grunt --help for info):');
    for (const { name, desc } of helpMessageTasks) {
      console.error(`$ grunt ${name}`);
      console.error(`  ${desc}`);
    }
  });
};
