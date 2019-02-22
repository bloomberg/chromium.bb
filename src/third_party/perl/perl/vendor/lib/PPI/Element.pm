package PPI::Element;

=pod

=head1 NAME

PPI::Element - The abstract Element class, a base for all source objects

=head1 INHERITANCE

  PPI::Element is the root of the PDOM tree

=head1 DESCRIPTION

The abstract C<PPI::Element> serves as a base class for all source-related
objects, from a single whitespace token to an entire document. It provides
a basic set of methods to provide a common interface and basic
implementations.

=head1 METHODS

=cut

use strict;
use Clone           ();
use Scalar::Util    qw{refaddr};
use Params::Util    qw{_INSTANCE _ARRAY};
use List::MoreUtils ();
use PPI::Util       ();
use PPI::Node       ();

use vars qw{$VERSION $errstr %_PARENT};
BEGIN {
	$VERSION = '1.215';
	$errstr  = '';

	# Master Child -> Parent index
	%_PARENT = ();
}

use overload 'bool' => \&PPI::Util::TRUE;
use overload '""'   => 'content';
use overload '=='   => '__equals';
use overload '!='   => '__nequals';
use overload 'eq'   => '__eq';
use overload 'ne'   => '__ne';





#####################################################################
# General Properties

=pod

=head2 significant

Because we treat whitespace and other non-code items as Tokens (in order to
be able to "round trip" the L<PPI::Document> back to a file) the
C<significant> method allows us to distinguish between tokens that form a
part of the code, and tokens that aren't significant, such as whitespace,
POD, or the portion of a file after (and including) the C<__END__> token.

Returns true if the Element is significant, or false it not.

=cut

### XS -> PPI/XS.xs:_PPI_Element__significant 0.845+
sub significant { 1 }

=pod

=head2 class

The C<class> method is provided as a convenience, and really does nothing
more than returning C<ref($self)>. However, some people have found that
they appreciate the laziness of C<$Foo-E<gt>class eq 'whatever'>, so I
have caved to popular demand and included it.

Returns the class of the Element as a string

=cut

sub class { ref($_[0]) }

=pod

=head2 tokens

The C<tokens> method returns a list of L<PPI::Token> objects for the
Element, essentially getting back that part of the document as if it had
not been lexed.

This also means there are no Statements and no Structures in the list,
just the Token classes.

=cut

sub tokens { $_[0] }

=pod

=head2 content

For B<any> C<PPI::Element>, the C<content> method will reconstitute the
base code for it as a single string. This method is also the method used
for overloading stringification. When an Element is used in a double-quoted
string for example, this is the method that is called.

B<WARNING:>

You should be aware that because of the way that here-docs are handled, any
here-doc content is not included in C<content>, and as such you should
B<not> eval or execute the result if it contains any L<PPI::Token::HereDoc>.

The L<PPI::Document> method C<serialize> should be used to stringify a PDOM
document into something that can be executed as expected.

Returns the basic code as a string (excluding here-doc content).

=cut

### XS -> PPI/XS.xs:_PPI_Element__content 0.900+
sub content { '' }





#####################################################################
# Naigation Methods

=pod

=head2 parent

Elements themselves are not intended to contain other Elements, that is
left to the L<PPI::Node> abstract class, a subclass of C<PPI::Element>.
However, all Elements can be contained B<within> a parent Node.

If an Element is within a parent Node, the C<parent> method returns the
Node.

=cut

sub parent { $_PARENT{refaddr $_[0]} }

=pod

=head2 descendant_of $element

Answers whether a C<PPI::Element> is contained within another one.

C<PPI::Element>s are considered to be descendants of themselves.

=begin testing descendant_of 9

my $Document = PPI::Document->new( \'( [ thingy ] ); $blarg = 1' );
isa_ok( $Document, 'PPI::Document' );
ok(
	$Document->descendant_of($Document),
	'Document is a descendant of itself.',
);

my $words = $Document->find('Token::Word');
is(scalar @{$words}, 1, 'Document contains 1 Word.');
my $word = $words->[0];
ok(
	$word->descendant_of($word),
	'Word is a descendant of itself.',
);
ok(
	$word->descendant_of($Document),
	'Word is a descendant of the Document.',
);
ok(
	! $Document->descendant_of($word),
	'Document is not a descendant of the Word.',
);

my $symbols = $Document->find('Token::Symbol');
is(scalar @{$symbols}, 1, 'Document contains 1 Symbol.');
my $symbol = $symbols->[0];
ok(
	! $word->descendant_of($symbol),
	'Word is not a descendant the Symbol.',
);
ok(
	! $symbol->descendant_of($word),
	'Symbol is not a descendant the Word.',
);

=end testing

=cut

sub descendant_of {
	my $cursor = shift;
	my $parent = shift or return undef;
	while ( refaddr $cursor != refaddr $parent ) {
		$cursor = $_PARENT{refaddr $cursor} or return '';
	}
	return 1;
}

=pod

=head2 ancestor_of $element

Answers whether a C<PPI::Element> is contains another one.

C<PPI::Element>s are considered to be ancestors of themselves.

=begin testing ancestor_of 9

my $Document = PPI::Document->new( \'( [ thingy ] ); $blarg = 1' );
isa_ok( $Document, 'PPI::Document' );
ok(
	$Document->ancestor_of($Document),
	'Document is an ancestor of itself.',
);

my $words = $Document->find('Token::Word');
is(scalar @{$words}, 1, 'Document contains 1 Word.');
my $word = $words->[0];
ok(
	$word->ancestor_of($word),
	'Word is an ancestor of itself.',
);
ok(
	! $word->ancestor_of($Document),
	'Word is not an ancestor of the Document.',
);
ok(
	$Document->ancestor_of($word),
	'Document is an ancestor of the Word.',
);

my $symbols = $Document->find('Token::Symbol');
is(scalar @{$symbols}, 1, 'Document contains 1 Symbol.');
my $symbol = $symbols->[0];
ok(
	! $word->ancestor_of($symbol),
	'Word is not an ancestor the Symbol.',
);
ok(
	! $symbol->ancestor_of($word),
	'Symbol is not an ancestor the Word.',
);

=end testing

=cut

sub ancestor_of {
	my $self   = shift;
	my $cursor = shift or return undef;
	while ( refaddr $cursor != refaddr $self ) {
		$cursor = $_PARENT{refaddr $cursor} or return '';
	}
	return 1;
}

=pod

=head2 statement

For a C<PPI::Element> that is contained (at some depth) within a
L<PPI::Statment>, the C<statement> method will return the first parent
Statement object lexically 'above' the Element.

Returns a L<PPI::Statement> object, which may be the same Element if the
Element is itself a L<PPI::Statement> object.

Returns false if the Element is not within a Statement and is not itself
a Statement.

=cut

sub statement {
	my $cursor = shift;
	while ( ! _INSTANCE($cursor, 'PPI::Statement') ) {
		$cursor = $_PARENT{refaddr $cursor} or return '';
	}
	$cursor;
}

=pod

=head2 top

For a C<PPI::Element> that is contained within a PDOM tree, the C<top> method
will return the top-level Node in the tree. Most of the time this should be
a L<PPI::Document> object, however this will not always be so. For example,
if a subroutine has been removed from its Document, to be moved to another
Document.

Returns the top-most PDOM object, which may be the same Element, if it is
not within any parent PDOM object.

=cut

sub top {
	my $cursor = shift;
	while ( my $parent = $_PARENT{refaddr $cursor} ) {
		$cursor = $parent;
	}
	$cursor;
}

=pod

=head2 document

For an Element that is contained within a L<PPI::Document> object,
the C<document> method will return the top-level Document for the Element.

Returns the L<PPI::Document> for this Element, or false if the Element is not
contained within a Document.

=cut

sub document {
	my $top = shift->top;
	_INSTANCE($top, 'PPI::Document') and $top;
}

=pod

=head2 next_sibling

All L<PPI::Node> objects (specifically, our parent Node) contain a number of
C<PPI::Element> objects. The C<next_sibling> method returns the C<PPI::Element>
immediately after the current one, or false if there is no next sibling.

=cut

sub next_sibling {
	my $self     = shift;
	my $parent   = $_PARENT{refaddr $self} or return '';
	my $key      = refaddr $self;
	my $elements = $parent->{children};
	my $position = List::MoreUtils::firstidx {
		refaddr $_ == $key
		} @$elements;
	$elements->[$position + 1] || '';
}

=pod

=head2 snext_sibling

As per the other 's' methods, the C<snext_sibling> method returns the next
B<significant> sibling of the C<PPI::Element> object.

Returns a C<PPI::Element> object, or false if there is no 'next' significant
sibling.

=cut

sub snext_sibling {
	my $self     = shift;
	my $parent   = $_PARENT{refaddr $self} or return '';
	my $key      = refaddr $self;
	my $elements = $parent->{children};
	my $position = List::MoreUtils::firstidx {
		refaddr $_ == $key
		} @$elements;
	while ( defined(my $it = $elements->[++$position]) ) {
		return $it if $it->significant;
	}
	'';
}

=pod

=head2 previous_sibling

All L<PPI::Node> objects (specifically, our parent Node) contain a number of
C<PPI::Element> objects. The C<previous_sibling> method returns the Element
immediately before the current one, or false if there is no 'previous'
C<PPI::Element> object.

=cut

sub previous_sibling {
	my $self     = shift;
	my $parent   = $_PARENT{refaddr $self} or return '';
	my $key      = refaddr $self;
	my $elements = $parent->{children};
	my $position = List::MoreUtils::firstidx {
		refaddr $_ == $key
		} @$elements;
	$position and $elements->[$position - 1] or '';
}

=pod

=head2 sprevious_sibling

As per the other 's' methods, the C<sprevious_sibling> method returns
the previous B<significant> sibling of the C<PPI::Element> object.

Returns a C<PPI::Element> object, or false if there is no 'previous' significant
sibling.

=cut

sub sprevious_sibling {
	my $self     = shift;
	my $parent   = $_PARENT{refaddr $self} or return '';
	my $key      = refaddr $self;
	my $elements = $parent->{children};
	my $position = List::MoreUtils::firstidx {
		refaddr $_ == $key
		} @$elements;
	while ( $position-- and defined(my $it = $elements->[$position]) ) {
		return $it if $it->significant;
	}
	'';
}

=pod

=head2 first_token

As a support method for higher-order algorithms that deal specifically with
tokens and actual Perl content, the C<first_token> method finds the first
PPI::Token object within or equal to this one.

That is, if called on a L<PPI::Node> subclass, it will descend until it
finds a L<PPI::Token>. If called on a L<PPI::Token> object, it will return
the same object.

Returns a L<PPI::Token> object, or dies on error (which should be extremely
rare and only occur if an illegal empty L<PPI::Statement> exists below the
current Element somewhere.

=cut

sub first_token {
	my $cursor = shift;
	while ( $cursor->isa('PPI::Node') ) {
		$cursor = $cursor->first_element
		or die "Found empty PPI::Node while getting first token";
	}
	$cursor;
}


=pod

=head2 last_token

As a support method for higher-order algorithms that deal specifically with
tokens and actual Perl content, the C<last_token> method finds the last
PPI::Token object within or equal to this one.

That is, if called on a L<PPI::Node> subclass, it will descend until it
finds a L<PPI::Token>. If called on a L<PPI::Token> object, it will return
the itself.

Returns a L<PPI::Token> object, or dies on error (which should be extremely
rare and only occur if an illegal empty L<PPI::Statement> exists below the
current Element somewhere.

=cut

sub last_token {
	my $cursor = shift;
	while ( $cursor->isa('PPI::Node') ) {
		$cursor = $cursor->last_element
		or die "Found empty PPI::Node while getting first token";
	}
	$cursor;
}

=pod

=head2 next_token

As a support method for higher-order algorithms that deal specifically with
tokens and actual Perl content, the C<next_token> method finds the
L<PPI::Token> object that is immediately after the current Element, even if
it is not within the same parent L<PPI::Node> as the one for which the
method is being called.

Note that this is B<not> defined as a L<PPI::Token>-specific method,
because it can be useful to find the next token that is after, say, a
L<PPI::Statement>, although obviously it would be useless to want the
next token after a L<PPI::Document>.

Returns a L<PPI::Token> object, or false if there are no more tokens after
the Element.

=cut

sub next_token {
	my $cursor = shift;

	# Find the next element, going upwards as needed
	while ( 1 ) {
		my $element = $cursor->next_sibling;
		if ( $element ) {
			return $element if $element->isa('PPI::Token');
			return $element->first_token;
		}
		$cursor = $cursor->parent or return '';
		if ( $cursor->isa('PPI::Structure') and $cursor->finish ) {
			return $cursor->finish;
		}
	}
}

=pod

=head2 previous_token

As a support method for higher-order algorithms that deal specifically with
tokens and actual Perl content, the C<previous_token> method finds the
L<PPI::Token> object that is immediately before the current Element, even
if it is not within the same parent L<PPI::Node> as this one.

Note that this is not defined as a L<PPI::Token>-only method, because it can
be useful to find the token is before, say, a L<PPI::Statement>, although
obviously it would be useless to want the next token before a
L<PPI::Document>.

Returns a L<PPI::Token> object, or false if there are no more tokens before
the C<Element>.

=cut

sub previous_token {
	my $cursor = shift;

	# Find the previous element, going upwards as needed
	while ( 1 ) {
		my $element = $cursor->previous_sibling;
		if ( $element ) {
			return $element if $element->isa('PPI::Token');
			return $element->last_token;
		}
		$cursor = $cursor->parent or return '';
		if ( $cursor->isa('PPI::Structure') and $cursor->start ) {
			return $cursor->start;
		}
	}
}





#####################################################################
# Manipulation

=pod

=head2 clone

As per the L<Clone> module, the C<clone> method makes a perfect copy of
an Element object. In the generic case, the implementation is done using
the L<Clone> module's mechanism itself. In higher-order cases, such as for
Nodes, there is more work involved to keep the parent-child links intact.

=cut

sub clone {
	Clone::clone(shift);
}

=pod

=head2 insert_before @Elements

The C<insert_before> method allows you to insert lexical perl content, in
the form of C<PPI::Element> objects, before the calling C<Element>. You
need to be very careful when modifying perl code, as it's easy to break
things.

In its initial incarnation, this method allows you to insert a single
Element, and will perform some basic checking to prevent you inserting
something that would be structurally wrong (in PDOM terms).

In future, this method may be enhanced to allow the insertion of multiple
Elements, inline-parsed code strings or L<PPI::Document::Fragment> objects.

Returns true if the Element was inserted, false if it can not be inserted,
or C<undef> if you do not provide a L<PPI::Element> object as a parameter.

=begin testing __insert_before 6

my $Document = PPI::Document->new( \"print 'Hello World';" );
isa_ok( $Document, 'PPI::Document' );
my $semi = $Document->find_first('Token::Structure');
isa_ok( $semi, 'PPI::Token::Structure' );
is( $semi->content, ';', 'Got expected token' );
my $foo = PPI::Token::Word->new('foo');
isa_ok( $foo, 'PPI::Token::Word' );
is( $foo->content, 'foo', 'Created Word token' );
$semi->__insert_before( $foo );
is( $Document->serialize, "print 'Hello World'foo;",
	'__insert_before actually inserts' );

=end testing

=begin testing insert_before after __insert_before 6

my $Document = PPI::Document->new( \"print 'Hello World';" );
isa_ok( $Document, 'PPI::Document' );
my $semi = $Document->find_first('Token::Structure');
isa_ok( $semi, 'PPI::Token::Structure' );
is( $semi->content, ';', 'Got expected token' );
my $foo = PPI::Token::Word->new('foo');
isa_ok( $foo, 'PPI::Token::Word' );
is( $foo->content, 'foo', 'Created Word token' );
$semi->insert_before( $foo );
is( $Document->serialize, "print 'Hello World'foo;",
	'insert_before actually inserts' );

=end testing

=cut

sub __insert_before {
	my $self = shift;
	$self->parent->__insert_before_child( $self, @_ );
}

=pod

=head2 insert_after @Elements

The C<insert_after> method allows you to insert lexical perl content, in
the form of C<PPI::Element> objects, after the calling C<Element>. You need
to be very careful when modifying perl code, as it's easy to break things.

In its initial incarnation, this method allows you to insert a single
Element, and will perform some basic checking to prevent you inserting
something that would be structurally wrong (in PDOM terms).

In future, this method may be enhanced to allow the insertion of multiple
Elements, inline-parsed code strings or L<PPI::Document::Fragment> objects.

Returns true if the Element was inserted, false if it can not be inserted,
or C<undef> if you do not provide a L<PPI::Element> object as a parameter.

=begin testing __insert_after 6

my $Document = PPI::Document->new( \"print 'Hello World';" );
isa_ok( $Document, 'PPI::Document' );
my $string = $Document->find_first('Token::Quote');
isa_ok( $string, 'PPI::Token::Quote' );
is( $string->content, "'Hello World'", 'Got expected token' );
my $foo = PPI::Token::Word->new('foo');
isa_ok( $foo, 'PPI::Token::Word' );
is( $foo->content, 'foo', 'Created Word token' );
$string->__insert_after( $foo );
is( $Document->serialize, "print 'Hello World'foo;",
	'__insert_after actually inserts' );

=end testing

=begin testing insert_after after __insert_after 6

my $Document = PPI::Document->new( \"print 'Hello World';" );
isa_ok( $Document, 'PPI::Document' );
my $string = $Document->find_first('Token::Quote');
isa_ok( $string, 'PPI::Token::Quote' );
is( $string->content, "'Hello World'", 'Got expected token' );
my $foo = PPI::Token::Word->new('foo');
isa_ok( $foo, 'PPI::Token::Word' );
is( $foo->content, 'foo', 'Created Word token' );
$string->insert_after( $foo );
is( $Document->serialize, "print 'Hello World'foo;",
	'insert_after actually inserts' );

=end testing

=cut

sub __insert_after {
	my $self = shift;
	$self->parent->__insert_after_child( $self, @_ );
}

=pod

=head2 remove

For a given C<PPI::Element>, the C<remove> method will remove it from its
parent B<intact>, along with all of its children.

Returns the C<Element> itself as a convenience, or C<undef> if an error
occurs while trying to remove the C<Element>.

=cut

sub remove {
	my $self   = shift;
	my $parent = $self->parent or return $self;
	$parent->remove_child( $self );
}

=pod

=head2 delete

For a given C<PPI::Element>, the C<delete> method will remove it from its
parent, immediately deleting the C<Element> and all of its children (if it
has any).

Returns true if the C<Element> was successfully deleted, or C<undef> if
an error occurs while trying to remove the C<Element>.

=cut

sub delete {
	$_[0]->remove or return undef;
	$_[0]->DESTROY;
	1;
}

=pod

=head2 replace $Element

Although some higher level class support more exotic forms of replace,
at the basic level the C<replace> method takes a single C<Element> as
an argument and replaces the current C<Element> with it.

To prevent accidental damage to code, in this initial implementation the
replacement element B<must> be of the same class (or a subclass) as the
one being replaced.

=cut

sub replace {
	my $self    = ref $_[0] ? shift : return undef;
	my $Element = _INSTANCE(shift, ref $self) or return undef;
	die "The ->replace method has not yet been implemented";
}

=pod

=head2 location

If the Element exists within a L<PPI::Document> that has
indexed the Element locations using C<PPI::Document::index_locations>, the
C<location> method will return the location of the first character of the
Element within the Document.

Returns the location as a reference to a five-element array in the form C<[
$line, $rowchar, $col, $logical_line, $logical_file_name ]>. The values are in
a human format, with the first character of the file located at C<[ 1, 1, 1, ?,
'something' ]>.

The second and third numbers are similar, except that the second is the
literal horizontal character, and the third is the visual column, taking
into account tabbing (see L<PPI::Document/"tab_width [ $width ]">).

The fourth number is the line number, taking into account any C<#line>
directives.  The fifth element is the name of the file that the element was
found in, if available, taking into account any C<#line> directives.

Returns C<undef> on error, or if the L<PPI::Document> object has not been
indexed.

=cut

sub location {
	my $self = shift;

	$self->_ensure_location_present or return undef;

	# Return a copy, not the original
	return [ @{$self->{_location}} ];
}

=pod

=head2 line_number

If the Element exists within a L<PPI::Document> that has indexed the Element
locations using C<PPI::Document::index_locations>, the C<line_number> method
will return the line number of the first character of the Element within the
Document.

Returns C<undef> on error, or if the L<PPI::Document> object has not been
indexed.

=begin testing line_number 3

my $document = PPI::Document->new(\<<'END_PERL');


   foo
END_PERL

isa_ok( $document, 'PPI::Document' );
my $words = $document->find('PPI::Token::Word');
is( scalar @{$words}, 1, 'Found expected word token.' );
is( $words->[0]->line_number, 3, 'Got correct line number.' );

=end testing

=cut

sub line_number {
	my $self = shift;

	my $location = $self->location() or return undef;
	return $location->[0];
}

=pod

=head2 column_number

If the Element exists within a L<PPI::Document> that has indexed the Element
locations using C<PPI::Document::index_locations>, the C<column_number> method
will return the column number of the first character of the Element within the
Document.

Returns C<undef> on error, or if the L<PPI::Document> object has not been
indexed.

=begin testing column_number 3

my $document = PPI::Document->new(\<<'END_PERL');


   foo
END_PERL

isa_ok( $document, 'PPI::Document' );
my $words = $document->find('PPI::Token::Word');
is( scalar @{$words}, 1, 'Found expected word token.' );
is( $words->[0]->column_number, 4, 'Got correct column number.' );

=end testing

=cut

sub column_number {
	my $self = shift;

	my $location = $self->location() or return undef;
	return $location->[1];
}

=pod

=head2 visual_column_number

If the Element exists within a L<PPI::Document> that has indexed the Element
locations using C<PPI::Document::index_locations>, the C<visual_column_number>
method will return the visual column number of the first character of the
Element within the Document, according to the value of
L<PPI::Document/"tab_width [ $width ]">.

Returns C<undef> on error, or if the L<PPI::Document> object has not been
indexed.

=begin testing visual_column_number 3

my $document = PPI::Document->new(\<<"END_PERL");


\t foo
END_PERL

isa_ok( $document, 'PPI::Document' );
my $tab_width = 5;
$document->tab_width($tab_width);  # don't use a "usual" value.
my $words = $document->find('PPI::Token::Word');
is( scalar @{$words}, 1, 'Found expected word token.' );
is(
	$words->[0]->visual_column_number,
	$tab_width + 2,
	'Got correct visual column number.',
);

=end testing

=cut

sub visual_column_number {
	my $self = shift;

	my $location = $self->location() or return undef;
	return $location->[2];
}

=pod

=head2 logical_line_number

If the Element exists within a L<PPI::Document> that has indexed the Element
locations using C<PPI::Document::index_locations>, the C<logical_line_number>
method will return the line number of the first character of the Element within
the Document, taking into account any C<#line> directives.

Returns C<undef> on error, or if the L<PPI::Document> object has not been
indexed.

=begin testing logical_line_number 3

# Double quoted so that we don't really have a "#line" at the beginning and
# errors in this file itself aren't affected by this.
my $document = PPI::Document->new(\<<"END_PERL");


\#line 1 test-file
   foo
END_PERL

isa_ok( $document, 'PPI::Document' );
my $words = $document->find('PPI::Token::Word');
is( scalar @{$words}, 1, 'Found expected word token.' );
is( $words->[0]->logical_line_number, 1, 'Got correct logical line number.' );

=end testing

=cut

sub logical_line_number {
	my $self = shift;

	return $self->location()->[3];
}

=pod

=head2 logical_filename

If the Element exists within a L<PPI::Document> that has indexed the Element
locations using C<PPI::Document::index_locations>, the C<logical_filename>
method will return the logical file name containing the first character of the
Element within the Document, taking into account any C<#line> directives.

Returns C<undef> on error, or if the L<PPI::Document> object has not been
indexed.

=begin testing logical_filename 3

# Double quoted so that we don't really have a "#line" at the beginning and
# errors in this file itself aren't affected by this.
my $document = PPI::Document->new(\<<"END_PERL");


\#line 1 test-file
   foo
END_PERL

isa_ok( $document, 'PPI::Document' );
my $words = $document->find('PPI::Token::Word');
is( scalar @{$words}, 1, 'Found expected word token.' );
is(
	$words->[0]->logical_filename,
	'test-file',
	'Got correct logical line number.',
);

=end testing

=cut

sub logical_filename {
	my $self = shift;

	my $location = $self->location() or return undef;
	return $location->[4];
}

sub _ensure_location_present {
	my $self = shift;

	unless ( exists $self->{_location} ) {
		# Are we inside a normal document?
		my $Document = $self->document or return undef;
		if ( $Document->isa('PPI::Document::Fragment') ) {
			# Because they can't be serialized, document fragments
			# do not support the concept of location.
			return undef;
		}

		# Generate the locations. If they need one location, then
		# the chances are they'll want more, and it's better that
		# everything is already pre-generated.
		$Document->index_locations or return undef;
		unless ( exists $self->{_location} ) {
			# erm... something went very wrong here
			return undef;
		}
	}

	return 1;
}

# Although flush_locations is only publically a Document-level method,
# we are able to implement it at an Element level, allowing us to
# selectively flush only the part of the document that occurs after the
# element for which the flush is called.
sub _flush_locations {
	my $self  = shift;
	unless ( $self == $self->top ) {
		return $self->top->_flush_locations( $self );
	}

	# Get the full list of all Tokens
	my @Tokens = $self->tokens;

	# Optionally allow starting from an arbitrary element (or rather,
	# the first Token equal-to-or-within an arbitrary element)
	if ( _INSTANCE($_[0], 'PPI::Element') ) {
		my $start = shift->first_token;
		while ( my $Token = shift @Tokens ) {
			return 1 unless $Token->{_location};
			next unless refaddr($Token) == refaddr($start);

			# Found the start. Flush it's location
			delete $$Token->{_location};
			last;
		}
	}

	# Iterate over any remaining Tokens and flush their location
	foreach my $Token ( @Tokens ) {
		delete $Token->{_location};
	}

	1;
}





#####################################################################
# XML Compatibility Methods

sub _xml_name {
	my $class = ref $_[0] || $_[0];
	my $name  = lc join( '_', split /::/, $class );
	substr($name, 4);
}

sub _xml_attr {
	return {};
}

sub _xml_content {
	defined $_[0]->{content} ? $_[0]->{content} : '';
}





#####################################################################
# Internals

# Set the error string
sub _error {
	$errstr = $_[1];
	undef;
}

# Clear the error string
sub _clear {
	$errstr = '';
	$_[0];
}

# Being DESTROYed in this manner, rather than by an explicit
# ->delete means our reference count has probably fallen to zero.
# Therefore we don't need to remove ourselves from our parent,
# just the index ( just in case ).
### XS -> PPI/XS.xs:_PPI_Element__DESTROY 0.900+
sub DESTROY { delete $_PARENT{refaddr $_[0]} }

# Operator overloads
sub __equals  { ref $_[1] and refaddr($_[0]) == refaddr($_[1]) }
sub __nequals { !__equals(@_) }
sub __eq {
	my $self  = _INSTANCE($_[0], 'PPI::Element') ? $_[0]->content : $_[0];
	my $other = _INSTANCE($_[1], 'PPI::Element') ? $_[1]->content : $_[1];
	$self eq $other;
}
sub __ne { !__eq(@_) }

1;

=pod

=head1 TO DO

It would be nice if C<location> could be used in an ad-hoc manner. That is,
if called on an Element within a Document that has not been indexed, it will
do a one-off calculation to find the location. It might be very painful if
someone started using it a lot, without remembering to index the document,
but it would be handy for things that are only likely to use it once, such
as error handlers.

=head1 SUPPORT

See the L<support section|PPI/SUPPORT> in the main module.

=head1 AUTHOR

Adam Kennedy E<lt>adamk@cpan.orgE<gt>

=head1 COPYRIGHT

Copyright 2001 - 2011 Adam Kennedy.

This program is free software; you can redistribute
it and/or modify it under the same terms as Perl itself.

The full text of the license can be found in the
LICENSE file included with this module.

=cut
