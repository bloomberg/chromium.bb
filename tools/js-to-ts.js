// Redirect imports of .js files to .ts files
const Module = require('module');
const resolveFilename = Module._resolveFilename;
Module._resolveFilename = (request, parentModule, isMain) => {
  if (parentModule.filename.endsWith('.ts') && request.endsWith('.js')) {
    request = request.substring(0, request.length - '.ts'.length) + '.ts';
  }
  return resolveFilename.call(this, request, parentModule, isMain);
};
