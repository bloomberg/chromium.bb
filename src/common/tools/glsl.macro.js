const { createMacro, MacroError } = require('babel-plugin-macros');
const glslangModule = require('@webgpu/glslang');

let glslang;
function initializeGlslang() {
  if (!glslang) {
    glslang = glslangModule();
  }
}

module.exports = createMacro(({ references, state, babel }) => {
  for (const path of references.default) {
    if (path.parent.arguments.length !== 2) {
      throw new MacroError('GLSL() must have 2 arguments: stage, source');
    }

    const argStage = path.parent.arguments[0];
    if (argStage.type !== 'StringLiteral') {
      throw new MacroError('GLSL() stage must be a string literal');
    }
    const stage = argStage.value;

    const argSource = path.parent.arguments[1];
    if (argSource.type !== 'TemplateLiteral') {
      throw new MacroError('GLSL() source must be a template string (``)');
    }
    if (argSource.expressions.length !== 0 || argSource.quasis.length !== 1) {
      throw new MacroError('GLSL() source must not have template substitutions');
    }
    const source = argSource.quasis[0].value.raw;

    initializeGlslang();

    const t = babel.types;
    const code = glslang.compileGLSL(source, stage, false);
    const codeAsArray = Array.from(code, w => t.numericLiteral(w));
    const newPath = t.newExpression(t.identifier('Uint32Array'), [t.arrayExpression(codeAsArray)]);
    path.context.parentPath.replaceWith(newPath);

    let glslMacroText = path.hub.file.code
      .substring(path.parent.start, path.parent.end)
      .trim()
      .split('\n')
      .map(x => x.trim().length === 0 ? '' : ' ' + x)
      .join('\n *');
    path.context.parentPath.addComment('leading', glslMacroText + '\n ');
  }
});
