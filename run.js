const process = require('process');
const moduleAlias = require('module-alias')

moduleAlias.addAliases({
  'framework': __dirname + '/src/framework',
});
moduleAlias();

// 'node' 'run.js' '...'
require(process.argv[2]);
