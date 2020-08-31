package PPI::XSAccessor;

# This is an experimental prototype, use at your own risk.
# Provides optional enhancement of PPI with Class::XSAccessor (if installed)

use 5.006;
use strict;
use PPI ();

our $VERSION = '1.269'; # VERSION





######################################################################
# Replacement Methods

# Packages are implemented here in alphabetical order

package #hide from indexer
	PPI::Document;

use Class::XSAccessor
	replace => 1,
	getters => {
		readonly => 'readonly',
	},
	true    => [
		'scope'
	];

package #hide from indexer
	PPI::Document::File;

use Class::XSAccessor
	replace => 1,
	getters => {
		filename => 'filename',
	};

package #hide from indexer
	PPI::Document::Fragment;

use Class::XSAccessor
	replace => 1,
	false   => [
		'scope',
	];

package #hide from indexer
	PPI::Document::Normalized;

use Class::XSAccessor
	replace => 1,
	getters => {
		'_Document' => 'Document',
		'version'   => 'version',
		'functions' => 'functions',
	};

package #hide from indexer
	PPI::Element;

use Class::XSAccessor
	replace => 1,
	true    => [
		'significant',
	];

package #hide from indexer
	PPI::Exception;

use Class::XSAccessor
	replace => 1,
	getters => {
		message => 'message',
	};

package #hide from indexer
	PPI::Node;

use Class::XSAccessor
	replace => 1,
	false   => [
		'scope',
	];

package #hide from indexer
	PPI::Normal;

use Class::XSAccessor
	replace => 1,
	getters => {
		'layer' => 'layer',
	};

package #hide from indexer
	PPI::Statement;

use Class::XSAccessor
	replace => 1,
	true    => [
		'__LEXER__normal',
	];

package #hide from indexer
	PPI::Statement::Compound;

use Class::XSAccessor
	replace => 1,
	true    => [
		'scope',
	],
	false   => [
		'__LEXER__normal',
	];

package #hide from indexer
	PPI::Statement::Data;

use Class::XSAccessor
	replace => 1,
	false   => [
		'_complete',
	];

package #hide from indexer
	PPI::Statement::End;

use Class::XSAccessor
	replace => 1,
	true    => [
		'_complete',
	];

package #hide from indexer
	PPI::Statement::Given;

use Class::XSAccessor
	replace => 1,
	true    => [
		'scope',
	],
	false   => [
		'__LEXER__normal',
	];

package #hide from indexer
	PPI::Token;

use Class::XSAccessor
	replace => 1,
	getters => {
		content => 'content',
	},
	setters => {
		set_content => 'content',
	},
	true => [
		'__TOKENIZER__on_line_start',
		'__TOKENIZER__on_line_end',
	];

1;
