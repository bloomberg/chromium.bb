package PPI::Find;

=pod

=head1 NAME

PPI::Find - Object version of the Element->find method

=head1 SYNOPSIS

  # Create the Find object
  my $Find = PPI::Find->new( \&wanted );
  
  # Return all matching Elements as a list
  my @found = $Find->in( $Document );
  
  # Can we find any matching Elements
  if ( $Find->any_matches($Document) ) {
  	print "Found at least one matching Element";
  }
  
  # Use the object as an iterator
  $Find->start($Document) or die "Failed to execute search";
  while ( my $token = $Find->match ) {
  	...
  }

=head1 DESCRIPTION

PPI::Find is the primary PDOM searching class in the core PPI package.

=head2 History

It became quite obvious during the development of PPI that many of the
modules that would be built on top of it were going to need large numbers
of saved, storable or easily creatable search objects that could be
reused a number of times.

Although the internal ->find method provides a basic ability to search,
it is by no means thorough. PPI::Find attempts to resolve this problem.

=head2 Structure and Style

PPI::Find provides a similar API to the popular L<File::Find::Rule>
module for file searching, but without the ability to assemble queries.

The implementation of a separate PPI::Find::Rule sub-class that does
provide this ability is left as an exercise for the reader.

=head2 The &wanted function

At the core of each PPI::Find object is a "wanted" function that is
passed a number of arguments and returns a value which controls the
flow of the search.

As the search executes, each Element will be passed to the wanted function
in depth-first order.

It will be provided with two arguments. The current Element to test as $_[0],
and the top-level Element of the search as $_[1].

The &wanted function is expected to return 1 (positive) if the Element
matches the condition, 0 (false) if it does not, and undef (undefined) if
the condition does not match, and the Find search should not descend to
any of the current Element's children.

Errors should be reported from the &wanted function via die, which will be
caught by the Find object and returned as an error.

=head1 METHODS

=cut

use strict;
use Params::Util qw{_INSTANCE};

use vars qw{$VERSION};
BEGIN {
	$VERSION = '1.215';
}





#####################################################################
# Constructor

=pod

=head2 new &wanted

The C<new> constructor takes a single argument of the &wanted function,
as described above and creates a new search.

Returns a new PPI::Find object, or C<undef> if not passed a CODE reference.

=cut

sub new {
	my $class  = ref $_[0] ? ref shift : shift;
	my $wanted = ref $_[0] eq 'CODE' ? shift : return undef;

	# Create the object
	my $self = bless {
		wanted => $wanted,
	}, $class;

	$self;
}

=pod

=head2 clone

The C<clone> method creates another instance of the same Find object.

The cloning is done safely, so if your existing Find object is in the
middle of an iteration, the cloned Find object will not also be in the
iteration and can be safely used independently.

Returns a duplicate PPI::Find object.

=cut

sub clone {
	my $self = ref $_[0] ? shift
		: die "->clone can only be called as an object method";
	my $class = ref $self;

	# Create the object
	my $clone = bless {
		wanted => $self->{wanted},
	}, $class;

	$clone;
}





####################################################################
# Search Execution Methods

=pod

=head2 in $Document [, array_ref => 1 ]

The C<in> method starts and completes a full run of the search.

It takes as argument a single L<PPI::Element> object which will
serve as the top of the search process.

Returns a list of PPI::Element objects that match the condition
described by the &wanted function, or the null list on error.

You should check the ->errstr method for any errors if you are
returned the null list, which may also mean simply that no Elements
were found that matched the condition.

Because of this need to explicitly check for errors, an alternative
return value mechanism is provide. If you pass the C<array_ref => 1>
parameter to the method, it will return the list of matched Elements
as a reference to an ARRAY. The method will return false if no elements
were matched, or C<undef> on error.

The ->errstr method can still be used to get the error message as normal.

=cut

sub in {
	my $self    = shift;
	my $Element = shift;
	my %params  = @_;
	delete $self->{errstr};
 
	# Are we already acting as an iterator
	if ( $self->{in} ) {
		return $self->_error('->in called while another search is in progress', %params);
	}

	# Get the root element for the search
	unless ( _INSTANCE($Element, 'PPI::Element') ) {
		return $self->_error('->in was not passed a PPI::Element object', %params);
	}

	# Prepare the search
	$self->{in}      = $Element;
	$self->{matches} = [];

	# Execute the search
	eval {
		$self->_execute;
	};
	if ( $@ ) {
		my $errstr = $@;
		$errstr =~ s/\s+at\s+line\s+.+$//;
		return $self->_error("Error while searching: $errstr", %params);
	}

	# Clean up and return
	delete $self->{in};
	if ( $params{array_ref} ) {
		if ( @{$self->{matches}} ) {
			return delete $self->{matches};
		}
		delete $self->{matches};
		return '';
	}

	# Return as a list
	my $matches = delete $self->{matches};
	@$matches;
}

=pod

=head2 start $Element

The C<start> method lets the Find object act as an iterator. The method
is passed the parent PPI::Element object as for the C<in> method, but does
not accept any parameters.

To simplify error handling, the entire search is done at once, with the
results cached and provided as-requested.

Returns true if the search completes, and false on error.

=cut

sub start {
	my $self    = shift;
	my $Element = shift;
	delete $self->{errstr};

	# Are we already acting as an iterator
	if ( $self->{in} ) {
		return $self->_error('->in called while another search is in progress');
	}

	# Get the root element for the search
	unless ( _INSTANCE($Element, 'PPI::Element') ) {
		return $self->_error('->in was not passed a PPI::Element object');
	}

	# Prepare the search
	$self->{in}      = $Element;
	$self->{matches} = [];

	# Execute the search
	eval {
		$self->_execute;
	};
	if ( $@ ) {
		my $errstr = $@;
		$errstr =~ s/\s+at\s+line\s+.+$//;
		$self->_error("Error while searching: $errstr");
		return undef;
	}

	1;
}

=pod

=head2 match

The C<match> method returns the next matching Element in the iteration.

Returns a PPI::Element object, or C<undef> if there are no remaining
Elements to be returned.

=cut

sub match {
	my $self = shift;
	return undef unless $self->{matches};

	# Fetch and return the next match
	my $match = shift @{$self->{matches}};
	return $match if $match;

	$self->finish;
	undef;
}

=pod

=head2 finish

The C<finish> method provides a mechanism to end iteration if you wish to
stop the iteration prematurely. It resets the Find object and allows it to
be safely reused.

A Find object will be automatically finished when C<match> returns false.
This means you should only need to call C<finnish> when you stop
iterating early.

You may safely call this method even when not iterating and it will return
without failure.

Always returns true

=cut

sub finish {
	my $self = shift;
	delete $self->{in};
	delete $self->{matches};
	delete $self->{errstr};
	1;
}





#####################################################################
# Support Methods and Error Handling

sub _execute {
	my $self   = shift;
	my $wanted = $self->{wanted};
	my @queue  = ( $self->{in} );

	# Pull entries off the queue and hand them off to the wanted function
	while ( my $Element = shift @queue ) {
		my $rv = &$wanted( $Element, $self->{in} );

		# Add to the matches if returns true
		push @{$self->{matches}}, $Element if $rv;

		# Continue and don't descend if it returned undef
		# or if it doesn't have children
		next unless defined $rv;
		next unless $Element->isa('PPI::Node');

		# Add the children to the head of the queue
		if ( $Element->isa('PPI::Structure') ) {
			unshift @queue, $Element->finish if $Element->finish;
			unshift @queue, $Element->children;
			unshift @queue, $Element->start if $Element->start;
		} else {
			unshift @queue, $Element->children;
		}
	}

	1;
}

=pod

=head2 errstr

The C<errstr> method returns the error messages when a given PPI::Find
object fails any action.

Returns a string, or C<undef> if there is no error.

=cut

sub errstr {
	shift->{errstr};
}

sub _error {
	my $self = shift;
	$self->{errstr} = shift;
	my %params = @_;
	$params{array_ref} ? undef : ();
}

1;

=pod

=head1 TO DO

- Implement the L<PPI::Find::Rule> class

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
