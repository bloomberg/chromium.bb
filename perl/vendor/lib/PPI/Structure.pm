package PPI::Structure;

=pod

=head1 NAME

PPI::Structure - The base class for Perl braced structures

=head1 INHERITANCE

  PPI::Structure
  isa PPI::Node
      isa PPI::Element

=head1 DESCRIPTION

PPI::Structure is the root class for all Perl bracing structures. This
covers all forms of C< [ ... ] >, C< { ... } >, and C< ( ... ) > brace
types, and includes cases where only one half of the pair exist.

The class PPI::Structure itself is full abstract and no objects of that
type should actually exist in the tree.

=head2 Elements vs Children

A B<PPI::Structure> has an unusual existance. Unlike a L<PPI::Document>
or L<PPI::Statement>, which both simply contain other elements, a
structure B<both> contains and consists of content.

That is, the brace tokens are B<not> considered to be "children" of the
structure, but are part of it.

In practice, this will mean that while the -E<gt>elements and -E<gt>tokens
methods (and related) B<will> return a list with the brace tokens at either
end, the -E<gt>children method explicitly will B<not> return the brace.

=head1 STRUCTURE CLASSES

Excluding the transient L<PPI::Structure::Unknown> that exists briefly
inside the parser, there are eight types of structure.

=head2 L<PPI::Structure::List>

This covers all round braces used for function arguments, in C<foreach>
loops, literal lists, and braces used for precedence-ordering purposes.

=head2 L<PPI::Structure::For>

Although B<not> used for the C<foreach> loop list, this B<is> used for
the special case of the round-brace three-part semicolon-seperated C<for>
loop expression (the traditional C style for loop).

=head2 L<PPI::Structure::Given>

This is for the expression being matched in switch statements.

=head2 L<PPI::Structure::When>

This is for the matching expression in "when" statements.

=head2 L<PPI::Structure::Condition>

This round-brace structure covers boolean conditional braces, such as
for C<if> and C<while> blocks.

=head2 L<PPI::Structure::Block>

This curly-brace and common structure is used for all form of code
blocks. This includes those for C<if>, C<do> and similar, as well
as C<grep>, C<map>, C<sort>, C<sub> and (labelled or anonymous) 
scoping blocks.

=head2 L<PPI::Structure::Constructor>

This class covers brace structures used for the construction of
anonymous C<ARRAY> and C<HASH> references.

=head2 L<PPI::Structure::Subscript>

This class covers square-braces and curly-braces used after a
-E<gt> pointer to access the subscript of an C<ARRAY> or C<HASH>.

=head1 METHODS

C<PPI::Structure> itself has very few methods. Most of the time, you will be
working with the more generic L<PPI::Element> or L<PPI::Node> methods, or one
of the methods that are subclass-specific.

=cut

use strict;
use Scalar::Util   ();
use Params::Util   qw{_INSTANCE};
use PPI::Node      ();
use PPI::Exception ();

use vars qw{$VERSION @ISA *_PARENT};
BEGIN {
	$VERSION = '1.215';
	@ISA     = 'PPI::Node';
	*_PARENT = *PPI::Element::_PARENT;
}

use PPI::Structure::Block       ();
use PPI::Structure::Condition   ();
use PPI::Structure::Constructor ();
use PPI::Structure::For         ();
use PPI::Structure::Given       ();
use PPI::Structure::List        ();
use PPI::Structure::Subscript   ();
use PPI::Structure::Unknown     ();
use PPI::Structure::When        ();





#####################################################################
# Constructor

sub new {
	my $class = shift;
	my $Token = PPI::Token::__LEXER__opens($_[0]) ? shift : return undef;

	# Create the object
	my $self = bless {
		children => [],
		start    => $Token,
		}, $class;

	# Set the start braces parent link
	Scalar::Util::weaken(
		$_PARENT{Scalar::Util::refaddr $Token} = $self
	);

	$self;
}





#####################################################################
# PPI::Structure API methods

=pod

=head2 start

For lack of better terminology (like "open" and "close") that has not
already in use for some other more important purpose, the two individual
braces for the structure are known within PPI as the "start" and "finish"
braces (at least for method purposes).

The C<start> method returns the start brace for the structure (i.e. the
opening brace).

Returns the brace as a L<PPI::Token::Structure> or C<undef> if the
structure does not have a starting brace.

Under normal parsing circumstances this should never occur, but may happen
due to manipulation of the PDOM tree.

=cut

sub start  { $_[0]->{start}  }

=pod

=head2 finish

The C<finish> method returns the finish brace for the structure (i.e. the
closing brace).

Returns the brace as a L<PPI::Token::Structure> or C<undef> if the
structure does not have a finishing brace. This can be quite common if
the document is not complete (for example, from an editor where the user
may be halfway through typeing a subroutine).

=cut

sub finish { $_[0]->{finish} }

=pod

=head2 braces

The C<braces> method is a utility method which returns the brace type,
regardless of whether has both braces defined, or just the starting
brace, or just the ending brace.

Returns on of the three strings C<'[]'>, C<'{}'>, or C<'()'>, or C<undef>
on error (primarily not having a start brace, as mentioned above).

=cut

sub braces {
	my $self = $_[0]->{start} ? shift : return undef;
	return {
		'[' => '[]',
		'(' => '()',
		'{' => '{}',
	}->{ $self->{start}->{content} };
}

=pod

=head1 complete

The C<complete> method is a convenience method that returns true if
the both braces are defined for the structure, or false if only one
brace is defined.

Unlike the top level C<complete> method which checks for completeness
in depth, the structure complete method ONLY confirms completeness
for the braces, and does not recurse downwards.

=cut

sub complete {
	!! ($_[0]->{start} and $_[0]->{finish});
}





#####################################################################
# PPI::Node overloaded methods

# For us, the "elements" concept includes the brace tokens
sub elements {
	my $self = shift;

	if ( wantarray ) {
		# Return a list in array context
		return ( $self->{start} || (), @{$self->{children}}, $self->{finish} || () );
	} else {
		# Return the number of elements in scalar context.
		# This is memory-cheaper than creating another big array
		return scalar(@{$self->{children}})
			+ ($self->{start}  ? 1 : 0)
			+ ($self->{finish} ? 1 : 0);
	}
}

# For us, the first element is probably the opening brace
sub first_element {
	# Technically, if we have no children and no opening brace,
	# then the first element is the closing brace.
	$_[0]->{start} or $_[0]->{children}->[0] or $_[0]->{finish};
}

# For us, the last element is probably the closing brace
sub last_element {
	# Technically, if we have no children and no closing brace,
	# then the last element is the opening brace
	$_[0]->{finish} or $_[0]->{children}->[-1] or $_[0]->{start};
}

# Location is same as the start token, if any
sub location {
	my $self  = shift;
	my $first = $self->first_element or return undef;
	$first->location;
}





#####################################################################
# PPI::Element overloaded methods

# Get the full set of tokens, including start and finish
sub tokens {
	my $self = shift;
	my @tokens = (
		$self->{start}  || (),
		$self->SUPER::tokens(@_),
		$self->{finish} || (),
		);
	@tokens;
}

# Like the token method ->content, get our merged contents.
# This will recurse downwards through everything
### Reimplement this using List::Utils stuff
sub content {
	my $self = shift;
	my $content = $self->{start} ? $self->{start}->content : '';
	foreach my $child ( @{$self->{children}} ) {
		$content .= $child->content;
	}
	$content .= $self->{finish}->content if $self->{finish};
	$content;
}

# Is the structure completed
sub _complete {
	!! ( defined $_[0]->{finish} );
}

# You can insert either another structure, or a token
sub insert_before {
	my $self    = shift;
	my $Element = _INSTANCE(shift, 'PPI::Element') or return undef;
	if ( $Element->isa('PPI::Structure') ) {
		return $self->__insert_before($Element);
	} elsif ( $Element->isa('PPI::Token') ) {
		return $self->__insert_before($Element);
	}
	'';
}

# As above, you can insert either another structure, or a token
sub insert_after {
	my $self    = shift;
	my $Element = _INSTANCE(shift, 'PPI::Element') or return undef;
	if ( $Element->isa('PPI::Structure') ) {
		return $self->__insert_after($Element);
	} elsif ( $Element->isa('PPI::Token') ) {
		return $self->__insert_after($Element);
	}
	'';
}

1;

=pod

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
