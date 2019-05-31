package PPI::Normal;

=pod

=head1 NAME

PPI::Normal - Normalize Perl Documents

=head2 DESCRIPTION

Perl Documents, as created by PPI, are typically filled with all sorts of
mess such as whitespace and comments and other things that don't effect
the actual meaning of the code.

In addition, because there is more than one way to do most things, and the
syntax of Perl itself is quite flexible, there are many ways in which the
"same" code can look quite different.

PPI::Normal attempts to resolve this by providing a variety of mechanisms
and algorithms to "normalize" Perl Documents, and determine a sort of base
form for them (although this base form will be a memory structure, and
not something that can be turned back into Perl source code).

The process itself is quite complex, and so for convenience and
extensibility it has been separated into a number of layers. At a later
point, it will be possible to write Plugin classes to insert additional
normalization steps into the various different layers.

In addition, you can choose to do the normalization only as deep as a
particular layer, depending on aggressively you want the normalization
process to be.

=head1 METHODS

=cut

use strict;
use Carp                      ();
use List::Util 1.33           ();
use PPI::Util                 '_Document';
use PPI::Document::Normalized ();
use PPI::Normal::Standard     ();
use PPI::Singletons           '%LAYER';

our $VERSION = '1.269'; # VERSION

# With the registration mechanism in place, load in the main set of
# normalization methods to initialize the store.
PPI::Normal::Standard->import;




#####################################################################
# Configuration

=pod

=head2 register $function => $layer, ...

The C<register> method is used by normalization method providers to
tell the normalization engines which functions need to be run, and
in which layer they apply.

Provide a set of key/value pairs, where the key is the full name of the
function (in string form), and the value is the layer (see description
of the layers above) in which it should be run.

Returns true if all functions are registered, or C<undef> on error.

=cut

sub register {
	my $class = shift;
	while ( @_ ) {
		# Check the function
		my $function = shift;
		SCOPE: {
			no strict 'refs';
			defined $function and defined &{"$function"}
				or Carp::croak("Bad function name provided to PPI::Normal");
		}

		# Has it already been added?
		if ( List::Util::any { $_ eq $function } map @{$_}, values %LAYER ) {
			return 1;
		}

		# Check the layer to add it to
		my $layer = shift;
		defined $layer and $layer =~ /^(?:1|2)$/
			or Carp::croak("Bad layer provided to PPI::Normal");

		# Add to the layer data store
		push @{ $LAYER{$layer} }, $function;
	}

	1;
}





#####################################################################
# Constructor and Accessors

=pod

=head2 new

  my $level_1 = PPI::Normal->new;
  my $level_2 = PPI::Normal->new(2);

Creates a new normalization object, to which Document objects
can be passed to be normalized.

Of course, what you probably REALLY want is just to call
L<PPI::Document>'s C<normalize> method.

Takes an optional single parameter of the normalisation layer
to use, which at this time can be either "1" or "2".

Returns a new C<PPI::Normal> object, or C<undef> on error.

=cut

sub new {
	my $class = shift;
	my $layer = @_ ?
		(defined $_[0] and ! ref $_[0] and $_[0] =~ /^[12]$/) ? shift : return undef
		: 1;

	# Create the object
	my $object = bless {
		layer => $layer,
		}, $class;

	$object;
}

=pod

=head1 layer

The C<layer> accessor returns the normalisation layer of the object.

=cut

sub layer { $_[0]->{layer} }





#####################################################################
# Main Methods

=pod

=head2 process

The C<process> method takes anything that can be converted to a
L<PPI::Document> (object, SCALAR ref, filename), loads it and
applies the normalisation process to the document.

Returns a L<PPI::Document::Normalized> object, or C<undef> on error.

=cut

sub process {
	my $self = ref $_[0] ? shift : shift->new;

	# PPI::Normal objects are reusable, but not re-entrant
	return undef if $self->{Document};

	# Get or create the document
	$self->{Document} = _Document(shift) or return undef;

	# Work out what functions we need to call
	my @functions = map { @{ $LAYER{$_} } } ( 1 .. $self->layer );

	# Execute each function
	foreach my $function ( @functions ) {
		no strict 'refs';
		&{"$function"}( $self->{Document} );
	}

	# Create the normalized Document object
	my $Normalized = PPI::Document::Normalized->new(
		Document  => $self->{Document},
		version   => __PACKAGE__->VERSION,
		functions => \@functions,
	) or return undef;

	# Done, clean up
	delete $self->{Document};
	return $Normalized;
}

1;

=pod

=head1 NOTES

The following normalisation layers are implemented. When writing
plugins, you should register each transformation function with the
appropriate layer.

=head2 Layer 1 - Insignificant Data Removal

The basic step common to all normalization, layer 1 scans through the
Document and removes all whitespace, comments, POD, and anything else
that returns false for its C<significant> method.

It also checks each Element and removes known-useless sub-element
metadata such as the Element's physical position in the file.

=head2 Layer 2 - Significant Element Removal

After the removal of the insignificant data, Layer 2 removed larger, more
complex, and superficially "significant" elements, that can be removed
for the purposes of normalisation.

Examples from this layer include pragmas, now-useless statement
separators (since the PDOM tree is holding statement elements), and
several other minor bits and pieces.

=head2 Layer 3 - TO BE COMPLETED

This version of the forward-port of the Perl::Compare functionality
to the 0.900+ API of PPI only implements Layer 1 and 2 at this time.

=head1 TO DO

- Write the other 4-5 layers :)

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
