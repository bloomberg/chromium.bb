" Vim syntax file " Language: Mojom
" To get syntax highlighting for .mojom files, add the following to your .vimrc
" file:
"     source /path/to/src/tools/vim/mojom.vim

if !exists("g:main_syntax")
  if version < 600
    syntax clear
  elseif exists("b:current_syntax")
    finish
  endif
  let g:main_syntax = 'mojom'
  syntax region mojomFold start="{" end="}" transparent fold
endif

" keyword definitions
syntax keyword mojomType        bool int8 int16 int32 int64 uint8 uint16 uint32 uint64 float double
syntax match mojomImport        "^\(import\)\s"
syntax keyword mojomModule      module
syntax keyword mojomKeyword     interface enum struct union

" Comments
syntax keyword mojomTodo           contained TODO FIXME XXX
syntax region  mojomComment        start="/\*"  end="\*/" contains=mojomTodo,mojomDocLink,@Spell
syntax match   mojomLineComment    "//.*" contains=mojomTodo,@Spell
syntax match   mojomLineDocComment "///.*" contains=mojomTodo,mojomDocLink,@Spell
syntax region  mojomDocLink        contained start=+\[+ end=+\]+

" The default highlighting.
highlight default link mojomTodo            Todo
highlight default link mojomComment         Comment
highlight default link mojomLineComment     Comment
highlight default link mojomLineDocComment  Comment
highlight default link mojomDocLink         SpecialComment
highlight default link mojomType            Type
highlight default link mojomImport          Include
highlight default link mojomKeyword         Keyword

let b:current_syntax = "mojom"
let b:spell_options = "contained"

if g:main_syntax is# 'mojom'
  unlet g:main_syntax
endif
