package PPI::Document::Normalized;

=pod

=head1 NAME

PPI::Document::Normalized - A normalized Perl Document

=head1 DESCRIPTION

A C<Normalized Document> object is the result of the normalization process
contained in the L<PPI::Normal> class. See the documentation for
L<PPI::Normal> for more information.

The object contains a version stamp and function list for the version
of L<PPI::Normal> used to create it, and a processed and delinked
L<PPI::Document> object.

Typically, the Document object will have been mangled by the normalization
process in a way that would make it fatal to try to actually DO anything
with it.

Put simply, B<never> use the Document object after normalization.
B<YOU HAVE BEEN WARNED!>

The object is designed the way it is to provide a bias towards false
negatives. A comparison between two ::Normalized object will only return
true if they were produced by the same version of PPI::Normal, with the
same set of normalization functions (in the same order).

You may get false negatives if you are caching objects across an upgrade.

Please note that this is done for security purposes, as there are many
cases in which low layer normalization is likely to be done as part of
a code security process, and false positives could be highly dangerous.

=head1 METHODS

=cut

# For convenience (and since this isn't really a public class), import
# the methods we will need from Scalar::Util.
use strict;
use Scalar::Util qw{refaddr reftype blessed};
use Params::Util qw{_INSTANCE _ARRAY};
use PPI::Util    ();

use vars qw{$VERSION};
BEGIN {
	$VERSION = '1.215';
}

use overload 'bool' => \&PPI::Util::TRUE;
use overload '=='   => 'equal';






#####################################################################
# Constructor and Accessors

=pod

=head2 new

The C<new> method is intended for use only by the L<PPI::Normal> class,
and to get ::Normalized objects, you are highly recommended to use
either that module, or the C<normalized> method of the L<PPI::Document>
object itself.

=cut

sub new {
	my $class = shift;
	my %args  = @_;

	# Check the required params
	my $Document  = _INSTANCE($args{Document}, 'PPI::Document') or return undef;
	my $version   = $args{version} or return undef;
	my $functions = _ARRAY($args{functions}) or return undef;

	# Create the object
	my $self = bless {
		Document  => $Document,
		version   => $version,
		functions => $functions,
		}, $class;

	$self;
}

sub _Document { $_[0]->{Document}  }

=pod

=head2 version

The C<version> accessor returns the L<PPI::Normal> version used to create
the object.

=cut

sub version   { $_[0]->{version}   }

=pod

=head2 functions

The C<functions> accessor returns a reference to an array of the
normalization functions (in order) that were called when creating
the object.

=cut

sub functions { $_[0]->{functions} }





#####################################################################
# Comparison Methods

=pod

=head2 equal $Normalized

The C<equal> method is the primary comparison method, taking another
PPI::Document::Normalized object, and checking for equivalence to it.

The C<==> operator is also overload to this method, so that you can
do something like the following:

  my $first  = PPI::Document->load('first.pl');
  my $second = PPI::Document->load('second.pl');
  
  if ( $first->normalized == $second->normalized ) {
  	print "The two documents are equivalent";
  }

Returns true if the normalized documents are equivalent, false if not,
or C<undef> if there is an error.

=cut

sub equal {
	my $self  = shift;
	my $other = _INSTANCE(shift, 'PPI::Document::Normalized') or return undef;

	# Prevent multiple concurrent runs
	return undef if $self->{processing};

	# Check the version and function list first
	return '' unless $self->version eq $other->version;
	$self->_equal_ARRAY( $self->functions, $other->functions ) or return '';

	# Do the main comparison run
	$self->{seen} = {};
	my $rv = $self->_equal_blessed( $self->_Document, $other->_Document );
	delete $self->{seen};

	$rv;
}

# Check that two objects are matched
sub _equal_blessed {
	my ($self, $this, $that) = @_;
	my ($bthis, $bthat) = (blessed $this, blessed $that);
	$bthis and $bthat and $bthis eq $bthat or return '';

	# Check the object as a reference
	$self->_equal_reference( $this, $that );
}

# Check that two references match their types
sub _equal_reference {
	my ($self, $this, $that) = @_;
	my ($rthis, $rthat) = (refaddr $this, refaddr $that);
	$rthis and $rthat or return undef;

	# If we have seen this before, are the pointing
	# is it the same one we saw in both sides
	my $seen = $self->{seen}->{$rthis};
	if ( $seen and $seen ne $rthat ) {
		return '';
	}

	# Check the reference types
	my ($tthis, $tthat) = (reftype $this, reftype $that);
	$tthis and $tthat and $tthis eq $tthat or return undef;

	# Check the children of the reference type
	$self->{seen}->{$rthis} = $rthat;
	my $method = "_equal_$tthat";
	my $rv = $self->$method( $this, $that );
	delete $self->{seen}->{$rthis};
	$rv;
}

# Compare the children of two SCALAR references
sub _equal_SCALAR {
	my ($self, $this, $that) = @_;
	my ($cthis, $cthat) = ($$this, $$that);
	return $self->_equal_blessed( $cthis, $cthat )   if blessed $cthis;
	return $self->_equal_reference( $cthis, $cthat ) if ref $cthis;
	return (defined $cthat and $cthis eq $cthat)     if defined $cthis;
	! defined $cthat;
}

# For completeness sake, lets just treat REF as a specialist SCALAR case
sub _equal_REF { shift->_equal_SCALAR(@_) }

# Compare the children of two ARRAY references
sub _equal_ARRAY {
	my ($self, $this, $that) = @_;

	# Compare the number of elements
	scalar(@$this) == scalar(@$that) or return '';

	# Check each element in the array.
	# Descend depth-first.
	foreach my $i ( 0 .. scalar(@$this) ) {
		my ($cthis, $cthat) = ($this->[$i], $that->[$i]);
		if ( blessed $cthis ) {
			return '' unless $self->_equal_blessed( $cthis, $cthat );
		} elsif ( ref $cthis ) {
			return '' unless $self->_equal_reference( $cthis, $cthat );
		} elsif ( defined $cthis ) {
			return '' unless (defined $cthat and $cthis eq $cthat);
		} else {
			return '' if defined $cthat;
		}
	}

	1;
}

# Compare the children of a HASH reference
sub _equal_HASH {
	my ($self, $this, $that) = @_;

	# Compare the number of keys
	return '' unless scalar(keys %$this) == scalar(keys %$that);

	# Compare each key, descending depth-first.
	foreach my $k ( keys %$this ) {
		return '' unless exists $that->{$k};
		my ($cthis, $cthat) = ($this->{$k}, $that->{$k});
		if ( blessed $cthis ) {
			return '' unless $self->_equal_blessed( $cthis, $cthat );
		} elsif ( ref $cthis ) {
			return '' unless $self->_equal_reference( $cthis, $cthat );
		} elsif ( defined $cthis ) {
			return '' unless (defined $cthat and $cthis eq $cthat);
		} else {
			return '' if defined $cthat;
		}
	}

	1;
}		

# We do not support GLOB comparisons
sub _equal_GLOB {
	my ($self, $this, $that) = @_;
	warn('GLOB comparisons are not supported');
	'';
}

# We do not support CODE comparisons
sub _equal_CODE {
	my ($self, $this, $that) = @_;
	refaddr $this == refaddr $that;
}

# We don't support IO comparisons
sub _equal_IO {
	my ($self, $this, $that) = @_;
	warn('IO comparisons are not supported');
	'';
}

sub DESTROY {
	# Take the screw up Document with us
	if ( $_[0]->{Document} ) {
		$_[0]->{Document}->DESTROY;
		delete $_[0]->{Document};
	}
}

1;

=pod

=head1 SUPPORT

See the L<support section|PPI/SUPPORT> in the main module.

=head1 AUTHOR

Adam Kennedy E<lt>adamk@cpan.orgE<gt>

=head1 COPYRIGHT

Copyright 2005 - 2011 Adam Kennedy.

This program is free software; you can redistribute
it and/or modify it under the same terms as Perl itself.

The full text of the license can be found in the
LICENSE file included with this module.

=cut
	
