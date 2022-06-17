// Copyright 2021 The Tint Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Package parser provides a basic parser for the Tint builtin definition
// language
package parser

import (
	"fmt"

	"dawn.googlesource.com/dawn/tools/src/cmd/intrinsic-gen/ast"
	"dawn.googlesource.com/dawn/tools/src/cmd/intrinsic-gen/lexer"
	"dawn.googlesource.com/dawn/tools/src/cmd/intrinsic-gen/tok"
)

// Parse produces a list of tokens for the given source code
func Parse(source, filepath string) (*ast.AST, error) {
	runes := []rune(source)
	tokens, err := lexer.Lex(runes, filepath)
	if err != nil {
		return nil, err
	}

	p := parser{tokens: tokens}
	return p.parse()
}

type parser struct {
	tokens []tok.Token
	err    error
}

func (p *parser) parse() (*ast.AST, error) {
	out := ast.AST{}
	var attributes ast.Attributes
	for p.err == nil {
		t := p.peek(0)
		if t == nil {
			break
		}
		switch t.Kind {
		case tok.Attr:
			attributes = append(attributes, p.attributes()...)
		case tok.Enum:
			if len(attributes) > 0 {
				p.err = fmt.Errorf("%v unexpected attribute", attributes[0].Source)
			}
			out.Enums = append(out.Enums, p.enumDecl())
		case tok.Match:
			if len(attributes) > 0 {
				p.err = fmt.Errorf("%v unexpected attribute", attributes[0].Source)
			}
			out.Matchers = append(out.Matchers, p.matcherDecl())
		case tok.Type:
			out.Types = append(out.Types, p.typeDecl(attributes))
			attributes = nil
		case tok.Function:
			out.Builtins = append(out.Builtins, p.builtinDecl(attributes))
			attributes = nil
		case tok.Operator:
			out.Operators = append(out.Operators, p.operatorDecl(attributes))
			attributes = nil
		case tok.Constructor:
			out.Constructors = append(out.Constructors, p.constructorDecl(attributes))
			attributes = nil
		case tok.Converter:
			out.Converters = append(out.Converters, p.converterDecl(attributes))
			attributes = nil
		default:
			p.err = fmt.Errorf("%v unexpected token '%v'", t.Source, t.Kind)
		}
		if p.err != nil {
			return nil, p.err
		}
	}
	return &out, nil
}

func (p *parser) enumDecl() ast.EnumDecl {
	p.expect(tok.Enum, "enum declaration")
	name := p.expect(tok.Identifier, "enum name")
	e := ast.EnumDecl{Source: name.Source, Name: string(name.Runes)}
	p.expect(tok.Lbrace, "enum declaration")
	for p.err == nil && p.match(tok.Rbrace) == nil {
		e.Entries = append(e.Entries, p.enumEntry())
	}
	return e
}

func (p *parser) enumEntry() ast.EnumEntry {
	decos := p.attributes()
	name := p.expect(tok.Identifier, "enum entry")
	return ast.EnumEntry{Source: name.Source, Attributes: decos, Name: string(name.Runes)}
}

func (p *parser) matcherDecl() ast.MatcherDecl {
	p.expect(tok.Match, "matcher declaration")
	name := p.expect(tok.Identifier, "matcher name")
	m := ast.MatcherDecl{Source: name.Source, Name: string(name.Runes)}
	p.expect(tok.Colon, "matcher declaration")
	for p.err == nil {
		m.Options = append(m.Options, p.templatedName())
		if p.match(tok.Or) == nil {
			break
		}
	}
	return m
}

func (p *parser) typeDecl(decos ast.Attributes) ast.TypeDecl {
	p.expect(tok.Type, "type declaration")
	name := p.expect(tok.Identifier, "type name")
	m := ast.TypeDecl{
		Source:     name.Source,
		Attributes: decos,
		Name:       string(name.Runes),
	}
	if p.peekIs(0, tok.Lt) {
		m.TemplateParams = p.templateParams()
	}
	return m
}

func (p *parser) attributes() ast.Attributes {
	var out ast.Attributes
	for p.match(tok.Attr) != nil && p.err == nil {
		name := p.expect(tok.Identifier, "attribute name")
		values := []string{}
		if p.match(tok.Lparen) != nil {
			for p.err == nil {
				values = append(values, string(p.next().Runes))
				if p.match(tok.Comma) == nil {
					break
				}
			}
			p.expect(tok.Rparen, "attribute values")
		}
		out = append(out, ast.Attribute{
			Source: name.Source,
			Name:   string(name.Runes),
			Values: values,
		})
	}
	return out
}

func (p *parser) builtinDecl(decos ast.Attributes) ast.IntrinsicDecl {
	p.expect(tok.Function, "function declaration")
	name := p.expect(tok.Identifier, "function name")
	f := ast.IntrinsicDecl{
		Source:     name.Source,
		Kind:       ast.Builtin,
		Attributes: decos,
		Name:       string(name.Runes),
	}
	if p.peekIs(0, tok.Lt) {
		f.TemplateParams = p.templateParams()
	}
	f.Parameters = p.parameters()
	if p.match(tok.Arrow) != nil {
		ret := p.templatedName()
		f.ReturnType = &ret
	}
	return f
}

func (p *parser) operatorDecl(decos ast.Attributes) ast.IntrinsicDecl {
	p.expect(tok.Operator, "operator declaration")
	name := p.next()
	f := ast.IntrinsicDecl{
		Source:     name.Source,
		Kind:       ast.Operator,
		Attributes: decos,
		Name:       string(name.Runes),
	}
	if p.peekIs(0, tok.Lt) {
		f.TemplateParams = p.templateParams()
	}
	f.Parameters = p.parameters()
	if p.match(tok.Arrow) != nil {
		ret := p.templatedName()
		f.ReturnType = &ret
	}
	return f
}

func (p *parser) constructorDecl(decos ast.Attributes) ast.IntrinsicDecl {
	p.expect(tok.Constructor, "constructor declaration")
	name := p.next()
	f := ast.IntrinsicDecl{
		Source:     name.Source,
		Kind:       ast.Constructor,
		Attributes: decos,
		Name:       string(name.Runes),
	}
	if p.peekIs(0, tok.Lt) {
		f.TemplateParams = p.templateParams()
	}
	f.Parameters = p.parameters()
	if p.match(tok.Arrow) != nil {
		ret := p.templatedName()
		f.ReturnType = &ret
	}
	return f
}

func (p *parser) converterDecl(decos ast.Attributes) ast.IntrinsicDecl {
	p.expect(tok.Converter, "converter declaration")
	name := p.next()
	f := ast.IntrinsicDecl{
		Source:     name.Source,
		Kind:       ast.Converter,
		Attributes: decos,
		Name:       string(name.Runes),
	}
	if p.peekIs(0, tok.Lt) {
		f.TemplateParams = p.templateParams()
	}
	f.Parameters = p.parameters()
	if p.match(tok.Arrow) != nil {
		ret := p.templatedName()
		f.ReturnType = &ret
	}
	return f
}

func (p *parser) parameters() ast.Parameters {
	l := ast.Parameters{}
	p.expect(tok.Lparen, "function parameter list")
	if p.match(tok.Rparen) == nil {
		for p.err == nil {
			l = append(l, p.parameter())
			if p.match(tok.Comma) == nil {
				break
			}
		}
		p.expect(tok.Rparen, "function parameter list")
	}
	return l
}

func (p *parser) parameter() ast.Parameter {
	attributes := p.attributes()
	if p.peekIs(1, tok.Colon) {
		// name type
		name := p.expect(tok.Identifier, "parameter name")
		p.expect(tok.Colon, "parameter type")
		return ast.Parameter{
			Source:     name.Source,
			Name:       string(name.Runes),
			Attributes: attributes,
			Type:       p.templatedName(),
		}
	}
	// type
	ty := p.templatedName()
	return ast.Parameter{
		Source:     ty.Source,
		Attributes: attributes,
		Type:       ty,
	}
}

func (p *parser) string() string {
	s := p.expect(tok.String, "string")
	return string(s.Runes)
}

func (p *parser) templatedName() ast.TemplatedName {
	name := p.expect(tok.Identifier, "type name")
	m := ast.TemplatedName{Source: name.Source, Name: string(name.Runes)}
	if p.match(tok.Lt) != nil {
		for p.err == nil {
			m.TemplateArgs = append(m.TemplateArgs, p.templatedName())
			if p.match(tok.Comma) == nil {
				break
			}
		}
		p.expect(tok.Gt, "template argument type list")
	}
	return m
}

func (p *parser) templateParams() ast.TemplateParams {
	t := ast.TemplateParams{}
	p.expect(tok.Lt, "template parameter list")
	for p.err == nil && p.peekIs(0, tok.Identifier) {
		t = append(t, p.templateParam())
	}
	p.expect(tok.Gt, "template parameter list")
	return t
}

func (p *parser) templateParam() ast.TemplateParam {
	name := p.match(tok.Identifier)
	t := ast.TemplateParam{
		Source: name.Source,
		Name:   string(name.Runes),
	}
	if p.match(tok.Colon) != nil {
		t.Type = p.templatedName()
	}
	p.match(tok.Comma)
	return t
}

func (p *parser) expect(kind tok.Kind, use string) tok.Token {
	if p.err != nil {
		return tok.Invalid
	}
	t := p.match(kind)
	if t == nil {
		if len(p.tokens) > 0 {
			p.err = fmt.Errorf("%v expected '%v' for %v, got '%v'",
				p.tokens[0].Source, kind, use, p.tokens[0].Kind)
		} else {
			p.err = fmt.Errorf("expected '%v' for %v, but reached end of file", kind, use)
		}
		return tok.Invalid
	}
	return *t
}

func (p *parser) ident(use string) string {
	return string(p.expect(tok.Identifier, use).Runes)
}

func (p *parser) match(kind tok.Kind) *tok.Token {
	if p.err != nil || len(p.tokens) == 0 {
		return nil
	}
	t := p.tokens[0]
	if t.Kind != kind {
		return nil
	}
	p.tokens = p.tokens[1:]
	return &t
}

func (p *parser) next() *tok.Token {
	if p.err != nil {
		return nil
	}
	if len(p.tokens) == 0 {
		p.err = fmt.Errorf("reached end of file")
	}
	t := p.tokens[0]
	p.tokens = p.tokens[1:]
	return &t
}

func (p *parser) peekIs(i int, kind tok.Kind) bool {
	t := p.peek(i)
	if t == nil {
		return false
	}
	return t.Kind == kind
}

func (p *parser) peek(i int) *tok.Token {
	if len(p.tokens) <= i {
		return nil
	}
	return &p.tokens[i]
}
