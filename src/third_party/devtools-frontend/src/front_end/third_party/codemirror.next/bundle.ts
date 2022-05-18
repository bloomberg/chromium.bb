// Base script used with Rollup to bundle the necessary CodeMirror
// components.
//
// Note that this file is also used as a TypeScript source to bundle
// the .d.ts files.

import {StreamLanguage} from "@codemirror/language";

export {
  acceptCompletion, autocompletion, closeBrackets, closeBracketsKeymap
, closeCompletion, completeAnyWord,
  Completion, CompletionContext, CompletionResult, CompletionSource, currentCompletions,
  ifNotIn, selectedCompletion, startCompletion} from '@codemirror/autocomplete';
export {
  cursorMatchingBracket, cursorSubwordBackward, cursorSubwordForward,
  history, historyKeymap,
  indentLess, indentMore, insertNewlineAndIndent, redo, redoSelection, selectMatchingBracket,
  selectSubwordBackward, selectSubwordForward,
  standardKeymap, toggleComment, undo, undoSelection
} from '@codemirror/commands';
export * as css from '@codemirror/lang-css';
export * as html from '@codemirror/lang-html';
export * as javascript from '@codemirror/lang-javascript';
export { bracketMatching,
  codeFolding,
  ensureSyntaxTree, foldGutter, foldKeymap, HighlightStyle, indentOnInput, indentUnit,Language, LanguageSupport,
  StreamLanguage, StreamParser, StringStream
, syntaxHighlighting, syntaxTree, TagStyle} from '@codemirror/language';
export {} from '@codemirror/rangeset';
export { highlightSelectionMatches,selectNextOccurrence} from '@codemirror/search';
export {
  Annotation, AnnotationType, ChangeDesc, ChangeSet, ChangeSpec, Compartment,
  EditorSelection, EditorState, EditorStateConfig, Extension, Facet,
  Line, MapMode, Prec, Range, RangeSet, RangeSetBuilder,
  SelectionRange, StateEffect, StateEffectType, StateField, Text, TextIterator
, Transaction,
  TransactionSpec} from '@codemirror/state';
export {} from '@codemirror/stream-parser';
export {
  Command, Decoration, DecorationSet, drawSelection, EditorView,
  gutter, GutterMarker, gutters,
  highlightSpecialChars, KeyBinding, keymap, lineNumberMarkers,lineNumbers, MatchDecorator, Panel, placeholder,
  repositionTooltips,
  scrollPastEnd, showPanel,showTooltip, Tooltip, tooltips, TooltipView
, ViewPlugin, ViewUpdate, WidgetType} from '@codemirror/view';
export {
  NodeProp, NodeSet, NodeType, Parser, SyntaxNode, Tree, TreeCursor
} from '@lezer/common';
export {highlightTree, Tag, tags} from '@lezer/highlight';
export {LRParser} from '@lezer/lr';
export {StyleModule} from 'style-mod';

export async function clojure() {
  return StreamLanguage.define((await import('@codemirror/legacy-modes/mode/clojure')).clojure);
}
export async function coffeescript() {
  return StreamLanguage.define((await import('@codemirror/legacy-modes/mode/coffeescript')).coffeeScript);
}
export function cpp() {
  return import('@codemirror/lang-cpp');
}
export function java() {
  return import('@codemirror/lang-java');
}
export function json() {
  return import('@codemirror/lang-json');
}
export function markdown() {
  return import('@codemirror/lang-markdown');
}
export function php() {
  return import('@codemirror/lang-php');
}
export function python() {
  return import('@codemirror/lang-python');
}
export async function shell() {
  return StreamLanguage.define((await import('@codemirror/legacy-modes/mode/shell')).shell);
}
export async function cssStreamParser() {
  return (await import('@codemirror/legacy-modes/mode/css') as any).sCSS;
}
export function wast() {
  return import('@codemirror/lang-wast');
}
export function xml() {
  return import('@codemirror/lang-xml');
}
