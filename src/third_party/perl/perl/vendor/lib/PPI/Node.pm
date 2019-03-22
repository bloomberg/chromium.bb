package PPI::Node;

=pod

=head1 NAME

PPI::Node - Abstract PPI Node class, an Element that can contain other Elements

=head1 INHERITANCE

  PPI::Node
  isa PPI::Element

=head1 SYNOPSIS

  # Create a typical node (a Document in this case)
  my $Node = PPI::Document->new;
  
  # Add an element to the node( in this case, a token )
  my $Token = PPI::Token::Word->new('my');
  $Node->add_element( $Token );
  
  # Get the elements for the Node
  my @elements = $Node->children;
  
  # Find all the barewords within a Node
  my $barewords = $Node->find( 'PPI::Token::Word' );
  
  # Find by more complex criteria
  my $my_tokens = $Node->find( sub { $_[1]->content eq 'my' } );
  
  # Remove all the whitespace
  $Node->prune( 'PPI::Token::Whitespace' );
  
  # Remove by more complex criteria
  $Node->prune( sub { $_[1]->content eq 'my' } );

=head1 DESCRIPTION

The C<PPI::Node> class provides an abstract base class for the Element
classes that are able to contain other elements L<PPI::Document>,
L<PPI::Statement>, and L<PPI::Structure>.

As well as those listed below, all of the methods that apply to
L<PPI::Element> objects also apply to C<PPI::Node> objects.

=head1 METHODS

=cut

use strict;
use Carp            ();
use Scalar::Util    qw{refaddr};
use List::MoreUtils ();
use Params::Util    qw{_INSTANCE _CLASS _CODELIKE};
use PPI::Element    ();

use vars qw{$VERSION @ISA *_PARENT};
BEGIN {
	$VERSION = '1.215';
	@ISA     = 'PPI::Element';
	*_PARENT = *PPI::Element::_PARENT;
}





#####################################################################
# The basic constructor

sub new {
	my $class = ref $_[0] || $_[0];
	bless { children => [] }, $class;
}





#####################################################################
# PDOM Methods

=pod

=head2 scope

The C<scope> method returns true if the node represents a lexical scope
boundary, or false if it does not.

=cut

### XS -> PPI/XS.xs:_PPI_Node__scope 0.903+
sub scope { '' }

=pod

=head2 add_element $Element

The C<add_element> method adds a L<PPI::Element> object to the end of a
C<PPI::Node>. Because Elements maintain links to their parent, an
Element can only be added to a single Node.

Returns true if the L<PPI::Element> was added. Returns C<undef> if the
Element was already within another Node, or the method is not passed 
a L<PPI::Element> object.

=cut

sub add_element {
	my $self = shift;

	# Check the element
	my $Element = _INSTANCE(shift, 'PPI::Element') or return undef;
	$_PARENT{refaddr $Element} and return undef;

	# Add the argument to the elements
	push @{$self->{children}}, $Element;
	Scalar::Util::weaken(
		$_PARENT{refaddr $Element} = $self
	);

	1;
}

# In a typical run profile, add_element is the number 1 resource drain.
# This is a highly optimised unsafe version, for internal use only.
sub __add_element {
	Scalar::Util::weaken(
		$_PARENT{refaddr $_[1]} = $_[0]
	);
	push @{$_[0]->{children}}, $_[1];
}

=pod

=head2 elements

The C<elements> method accesses all child elements B<structurally> within
the C<PPI::Node> object. Note that in the base of the L<PPI::Structure>
classes, this C<DOES> include the brace tokens at either end of the
structure.

Returns a list of zero or more L<PPI::Element> objects.

Alternatively, if called in the scalar context, the C<elements> method
returns a count of the number of elements.

=cut

sub elements {
	if ( wantarray ) {
		return @{$_[0]->{children}};
	} else {
		return scalar @{$_[0]->{children}};
	}
}

=pod

=head2 first_element

The C<first_element> method accesses the first element structurally within
the C<PPI::Node> object. As for the C<elements> method, this does include
the brace tokens for L<PPI::Structure> objects.

Returns a L<PPI::Element> object, or C<undef> if for some reason the
C<PPI::Node> object does not contain any elements.

=cut

# Normally the first element is also the first child
sub first_element {
	$_[0]->{children}->[0];
}

=pod

=head2 last_element

The C<last_element> method accesses the last element structurally within
the C<PPI::Node> object. As for the C<elements> method, this does include
the brace tokens for L<PPI::Structure> objects.

Returns a L<PPI::Element> object, or C<undef> if for some reason the
C<PPI::Node> object does not contain any elements.

=cut

# Normally the last element is also the last child
sub last_element {
	$_[0]->{children}->[-1];
}

=pod

=head2 children

The C<children> method accesses all child elements lexically within the
C<PPI::Node> object. Note that in the case of the L<PPI::Structure>
classes, this does B<NOT> include the brace tokens at either end of the
structure.

Returns a list of zero of more L<PPI::Element> objects.

Alternatively, if called in the scalar context, the C<children> method
returns a count of the number of lexical children.

=cut

# In the default case, this is the same as for the elements method
sub children {
	wantarray ? @{$_[0]->{children}} : scalar @{$_[0]->{children}};
}

=pod

=head2 schildren

The C<schildren> method is really just a convenience, the significant-only
variation of the normal C<children> method.

In list context, returns a list of significant children. In scalar context,
returns the number of significant children.

=cut

sub schildren {
	return grep { $_->significant } @{$_[0]->{children}} if wantarray;
	my $count = 0;
	foreach ( @{$_[0]->{children}} ) {
		$count++ if $_->significant;
	}
	return $count;
}

=pod

=head2 child $index

The C<child> method accesses a child L<PPI::Element> object by its
position within the Node.

Returns a L<PPI::Element> object, or C<undef> if there is no child
element at that node.

=cut

sub child {
	$_[0]->{children}->[$_[1]];
}

=pod

=head2 schild $index

The lexical structure of the Perl language ignores 'insignificant' items,
such as whitespace and comments, while L<PPI> treats these items as valid
tokens so that it can reassemble the file at any time. Because of this,
in many situations there is a need to find an Element within a Node by
index, only counting lexically significant Elements.

The C<schild> method returns a child Element by index, ignoring
insignificant Elements. The index of a child Element is specified in the
same way as for a normal array, with the first Element at index 0, and
negative indexes used to identify a "from the end" position.

=cut

sub schild {
	my $self = shift;
	my $idx  = 0 + shift;
	my $el   = $self->{children};
	if ( $idx < 0 ) {
		my $cursor = 0;
		while ( exists $el->[--$cursor] ) {
			return $el->[$cursor] if $el->[$cursor]->significant and ++$idx >= 0;
		}
	} else {
		my $cursor = -1;
		while ( exists $el->[++$cursor] ) {
			return $el->[$cursor] if $el->[$cursor]->significant and --$idx < 0;
		}
	}
	undef;
}

=pod

=head2 contains $Element

The C<contains> method is used to determine if another L<PPI::Element>
object is logically "within" a C<PPI::Node>. For the special case of the
brace tokens at either side of a L<PPI::Structure> object, they are
generally considered "within" a L<PPI::Structure> object, even if they are
not actually in the elements for the L<PPI::Structure>.

Returns true if the L<PPI::Element> is within us, false if not, or C<undef>
on error.

=cut

sub contains {
	my $self    = shift;
	my $Element = _INSTANCE(shift, 'PPI::Element') or return undef;

	# Iterate up the Element's parent chain until we either run out
	# of parents, or get to ourself.
	while ( $Element = $Element->parent ) {
		return 1 if refaddr($self) == refaddr($Element);
	}

	'';
}

=pod

=head2 find $class | \&wanted

The C<find> method is used to search within a code tree for
L<PPI::Element> objects that meet a particular condition.

To specify the condition, the method can be provided with either a simple
class name (full or shortened), or a C<CODE>/function reference.

  # Find all single quotes in a Document (which is a Node)
  $Document->find('PPI::Quote::Single');
  
  # The same thing with a shortened class name
  $Document->find('Quote::Single');
  
  # Anything more elaborate, we so with the sub
  $Document->find( sub {
  	# At the top level of the file...
  	$_[1]->parent == $_[0]
  	and (
  		# ...find all comments and POD
  		$_[1]->isa('PPI::Token::Pod')
  		or
  		$_[1]->isa('PPI::Token::Comment')
  	)
  } );

The function will be passed two arguments, the top-level C<PPI::Node>
you are searching in and the current L<PPI::Element> that the condition
is testing.

The anonymous function should return one of three values. Returning true
indicates a condition match, defined-false (C<0> or C<''>) indicates
no-match, and C<undef> indicates no-match and no-descend.

In the last case, the tree walker will skip over anything below the
C<undef>-returning element and move on to the next element at the same
level.

To halt the entire search and return C<undef> immediately, a condition
function should throw an exception (i.e. C<die>).

Note that this same wanted logic is used for all methods documented to
have a C<\&wanted> parameter, as this one does.

The C<find> method returns a reference to an array of L<PPI::Element>
objects that match the condition, false (but defined) if no Elements match
the condition, or C<undef> if you provide a bad condition, or an error
occurs during the search process.

In the case of a bad condition, a warning will be emitted as well.

=cut

sub find {
	my $self   = shift;
	my $wanted = $self->_wanted(shift) or return undef;

	# Use a queue based search, rather than a recursive one
	my @found = ();
	my @queue = @{$self->{children}};
	eval {
		while ( @queue ) {
			my $Element = shift @queue;
			my $rv      = &$wanted( $self, $Element );
			push @found, $Element if $rv;

			# Support "don't descend on undef return"
			next unless defined $rv;

			# Skip if the Element doesn't have any children
			next unless $Element->isa('PPI::Node');

			# Depth-first keeps the queue size down and provides a
			# better logical order.
			if ( $Element->isa('PPI::Structure') ) {
				unshift @queue, $Element->finish if $Element->finish;
				unshift @queue, @{$Element->{children}};
				unshift @queue, $Element->start if $Element->start;
			} else {
				unshift @queue, @{$Element->{children}};
			}
		}
	};
	if ( $@ ) {
		# Caught exception thrown from the wanted function
		return undef;
	}

	@found ? \@found : '';
}

=pod

=head2 find_first $class | \&wanted

If the normal C<find> method is like a grep, then C<find_first> is
equivalent to the L<Scalar::Util> C<first> function.

Given an element class or a wanted function, it will search depth-first
through a tree until it finds something that matches the condition,
returning the first Element that it encounters.

See the C<find> method for details on the format of the search condition.

Returns the first L<PPI::Element> object that matches the condition, false
if nothing matches the condition, or C<undef> if given an invalid condition,
or an error occurs.

=cut

sub find_first {
	my $self   = shift;
	my $wanted = $self->_wanted(shift) or return undef;

	# Use the same queue-based search as for ->find
	my @queue = @{$self->{children}};
	my $rv    = eval {
		# The defined() here prevents a ton of calls to PPI::Util::TRUE
		while ( @queue ) {
			my $Element = shift @queue;
			my $rv      = &$wanted( $self, $Element );
			return $Element if $rv;

			# Support "don't descend on undef return"
			next unless defined $rv;

			# Skip if the Element doesn't have any children
			next unless $Element->isa('PPI::Node');

			# Depth-first keeps the queue size down and provides a
			# better logical order.
			if ( $Element->isa('PPI::Structure') ) {
				unshift @queue, $Element->finish if defined($Element->finish);
				unshift @queue, @{$Element->{children}};
				unshift @queue, $Element->start  if defined($Element->start);
			} else {
				unshift @queue, @{$Element->{children}};
			}
		}
	};
	if ( $@ ) {
		# Caught exception thrown from the wanted function
		return undef;
	}

	$rv or '';
}

=pod

=head2 find_any $class | \&wanted

The C<find_any> method is a short-circuiting true/false method that behaves
like the normal C<find> method, but returns true as soon as it finds any
Elements that match the search condition.

See the C<find> method for details on the format of the search condition.

Returns true if any Elements that match the condition can be found, false if
not, or C<undef> if given an invalid condition, or an error occurs.

=cut

sub find_any {
	my $self = shift;
	my $rv   = $self->find_first(@_);
	$rv ? 1 : $rv; # false or undef
}

=pod

=head2 remove_child $Element

If passed a L<PPI::Element> object that is a direct child of the Node,
the C<remove_element> method will remove the C<Element> intact, along
with any of its children. As such, this method acts essentially as a
'cut' function.

If successful, returns the removed element.  Otherwise, returns C<undef>.

=cut

sub remove_child {
	my $self  = shift;
	my $child = _INSTANCE(shift, 'PPI::Element') or return undef;

	# Find the position of the child
	my $key = refaddr $child;
	my $p   = List::MoreUtils::firstidx {
		refaddr $_ == $key
	} @{$self->{children}};
	return undef unless defined $p;

	# Splice it out, and remove the child's parent entry
	splice( @{$self->{children}}, $p, 1 );
	delete $_PARENT{refaddr $child};

	$child;
}

=pod

=head2 prune $class | \&wanted

The C<prune> method is used to strip L<PPI::Element> objects out of a code
tree. The argument is the same as for the C<find> method, either a class
name, or an anonymous subroutine which returns true/false. Any Element
that matches the class|wanted will be deleted from the code tree, along
with any of its children.

The C<prune> method returns the number of C<Element> objects that matched
and were removed, B<non-recursively>. This might also be zero, so avoid a
simple true/false test on the return false of the C<prune> method. It
returns C<undef> on error, which you probably B<should> test for.

=begin testing prune 2

# Avoids a bug in old Perls relating to the detection of scripts
# Known to occur in ActivePerl 5.6.1 and at least one 5.6.2 install.
my $hashbang = reverse 'lrep/nib/rsu/!#'; 
my $document = PPI::Document->new( \<<"END_PERL" );
$hashbang

use strict;

sub one { 1 }
sub two { 2 }
sub three { 3 }

print one;
print "\n";
print three;
print "\n";

exit;
END_PERL

isa_ok( $document, 'PPI::Document' );
ok( defined($document->prune ('PPI::Statement::Sub')),
	'Pruned multiple subs ok' );

=end testing

=cut

sub prune {
	my $self   = shift;
	my $wanted = $self->_wanted(shift) or return undef;

	# Use a depth-first queue search
	my $pruned = 0;
	my @queue  = $self->children;
	eval {
		while ( my $element = shift @queue ) {
			my $rv = &$wanted( $self, $element );
			if ( $rv ) {
				# Delete the child
				$element->delete or return undef;
				$pruned++;
				next;
			}

			# Support the undef == "don't descend"
			next unless defined $rv;

			if ( _INSTANCE($element, 'PPI::Node') ) {
				# Depth-first keeps the queue size down
				unshift @queue, $element->children;
			}
		}
	};
	if ( $@ ) {
		# Caught exception thrown from the wanted function
		return undef;		
	}

	$pruned;
}

# This method is likely to be very heavily used, to take
# it slowly and carefuly.
### NOTE: Renaming this function or changing either to self will probably
###       break File::Find::Rule::PPI
sub _wanted {
	my $either = shift;
	my $it     = defined($_[0]) ? shift : do {
		Carp::carp('Undefined value passed as search condition') if $^W;
		return undef;
	};

	# Has the caller provided a wanted function directly
	return $it if _CODELIKE($it);
	if ( ref $it ) {
		# No other ref types are supported
		Carp::carp('Illegal non-CODE reference passed as search condition') if $^W;
		return undef;
	}

	# The first argument should be an Element class, possibly in shorthand
	$it = "PPI::$it" unless substr($it, 0, 5) eq 'PPI::';
	unless ( _CLASS($it) and $it->isa('PPI::Element') ) {
		# We got something, but it isn't an element
		Carp::carp("Cannot create search condition for '$it': Not a PPI::Element") if $^W;
		return undef;
	}

	# Create the class part of the wanted function
	my $wanted_class = "\n\treturn '' unless \$_[1]->isa('$it');";

	# Have we been given a second argument to check the content
	my $wanted_content = '';
	if ( defined $_[0] ) {
		my $content = shift;
		if ( ref $content eq 'Regexp' ) {
			$content = "$content";
		} elsif ( ref $content ) {
			# No other ref types are supported
			Carp::carp("Cannot create search condition for '$it': Not a PPI::Element") if $^W;
			return undef;
		} else {
			$content = quotemeta $content;
		}

		# Complete the content part of the wanted function
		$wanted_content .= "\n\treturn '' unless defined \$_[1]->{content};";
		$wanted_content .= "\n\treturn '' unless \$_[1]->{content} =~ /$content/;";
	}

	# Create the complete wanted function
	my $code = "sub {"
		. $wanted_class
		. $wanted_content
		. "\n\t1;"
		. "\n}";

	# Compile the wanted function
	$code = eval $code;
	(ref $code eq 'CODE') ? $code : undef;
}





####################################################################
# PPI::Element overloaded methods

sub tokens {
	map { $_->tokens } @{$_[0]->{children}};
}

### XS -> PPI/XS.xs:_PPI_Element__content 0.900+
sub content {
	join '', map { $_->content } @{$_[0]->{children}};
}

# Clone as normal, but then go down and relink all the _PARENT entries
sub clone {
	my $self  = shift;
	my $clone = $self->SUPER::clone;
	$clone->__link_children;
	$clone;
}

sub location {
	my $self  = shift;
	my $first = $self->{children}->[0] or return undef;
	$first->location;
}





#####################################################################
# Internal Methods

sub DESTROY {
	local $_;
	if ( $_[0]->{children} ) {
		my @queue = $_[0];
		while ( defined($_ = shift @queue) ) {
			unshift @queue, @{delete $_->{children}} if $_->{children};

			# Remove all internal/private weird crosslinking so that
			# the cascading DESTROY calls will get called properly.
			%$_ = ();
		}
	}

	# Remove us from our parent node as normal
	delete $_PARENT{refaddr $_[0]};
}

# Find the position of a child
sub __position {
	my $key = refaddr $_[1];
	List::MoreUtils::firstidx { refaddr $_ == $key } @{$_[0]->{children}};
}

# Insert one or more elements before a child
sub __insert_before_child {
	my $self = shift;
	my $key  = refaddr shift;
	my $p    = List::MoreUtils::firstidx {
	         refaddr $_ == $key
	         } @{$self->{children}};
	foreach ( @_ ) {
		Scalar::Util::weaken(
			$_PARENT{refaddr $_} = $self
			);
	}
	splice( @{$self->{children}}, $p, 0, @_ );
	1;
}

# Insert one or more elements after a child
sub __insert_after_child {
	my $self = shift;
	my $key  = refaddr shift;
	my $p    = List::MoreUtils::firstidx {
	         refaddr $_ == $key
	         } @{$self->{children}};
	foreach ( @_ ) {
		Scalar::Util::weaken(
			$_PARENT{refaddr $_} = $self
			);
	}
	splice( @{$self->{children}}, $p + 1, 0, @_ );
	1;
}

# Replace a child
sub __replace_child {
	my $self = shift;
	my $key  = refaddr shift;
	my $p    = List::MoreUtils::firstidx {
	         refaddr $_ == $key
	         } @{$self->{children}};
	foreach ( @_ ) {
		Scalar::Util::weaken(
			$_PARENT{refaddr $_} = $self
			);
	}
	splice( @{$self->{children}}, $p, 1, @_ );
	1;
}

# Create PARENT links for an entire tree.
# Used when cloning or thawing.
sub __link_children {
	my $self = shift;

	# Relink all our children ( depth first )
	my @queue = ( $self );
	while ( my $Node = shift @queue ) {
		# Link our immediate children
		foreach my $Element ( @{$Node->{children}} ) {
			Scalar::Util::weaken(
				$_PARENT{refaddr($Element)} = $Node
				);
			unshift @queue, $Element if $Element->isa('PPI::Node');
		}

		# If it's a structure, relink the open/close braces
		next unless $Node->isa('PPI::Structure');
		Scalar::Util::weaken(
			$_PARENT{refaddr($Node->start)}  = $Node
			) if $Node->start;
		Scalar::Util::weaken(
			$_PARENT{refaddr($Node->finish)} = $Node
			) if $Node->finish;
	}

	1;
}

1;

=pod

=head1 TO DO

- Move as much as possible to L<PPI::XS>

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
