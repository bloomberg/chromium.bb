package PPI::Document::File;

=pod

=head1 NAME

PPI::Document::File - A Perl Document located in a specific file

=head1 DESCRIPTION

B<WARNING: This class is experimental, and may change without notice>

B<PPI::Document::File> provides a L<PPI::Document> subclass that represents
a Perl document stored in a specific named file.

=head1 METHODS

=cut

use strict;
use Carp          ();
use Params::Util  qw{_STRING _INSTANCE};
use PPI::Document ();

use vars qw{$VERSION @ISA};
BEGIN {
	$VERSION = '1.215';
	@ISA     = 'PPI::Document';
}





#####################################################################
# Constructor and Accessors

=pod

=head2 new

  my $file = PPI::Document::File->new( 'Module.pm' );

The C<new> constructor works the same as for the regular one, except
that the only params allowed is a file name. You cannot create an
"anonymous" PPI::Document::File object, not can you create an empty one.

Returns a new PPI::Document::File object, or C<undef> on error.

=cut

sub new {
	my $class    = shift;
	my $filename = _STRING(shift);
	unless ( defined $filename ) {
		# Perl::Critic got a complaint about not handling a file
		# named "0".
		return $class->_error("Did not provide a file name to load");
	}

	# Load the Document
	my $self = $class->SUPER::new( $filename, @_ ) or return undef;

	# Unlike a normal inheritance situation, due to our need to stay
	# compatible with caching magic, this actually returns a regular
	# anonymous document. We need to rebless if
	if ( _INSTANCE($self, 'PPI::Document') ) {
		bless $self, 'PPI::Document::File';
	} else {
		die "PPI::Document::File SUPER call returned an object of the wrong type";
	}

	# Save the filename
	$self->{filename} = $filename;

	$self;
}

=head2 filename

The C<filename> accessor returns the name of the file in which the document
is stored.

=cut

sub filename {
	$_[0]->{filename};
}

=pod

=head2 save

  # Save to the file we were loaded from
  $file->save;
  
  # Save a copy to somewhere else
  $file->save( 'Module2.pm' );

The C<save> method works similarly to the one in the parent L<PPI::Document>
class, saving a copy of the document to a file.

The difference with this subclass is that if C<save> is not passed any
filename, it will save it back to the file it was loaded from.

Note: When saving to a different file, it is considered to be saving a
B<copy> and so the value returned by the C<filename> accessor will stay
the same, and not change to the new filename.

=cut

sub save {
	my $self = shift;

	# Save to where?
	my $filename = shift;
	unless ( defined $filename ) {
		$filename = $self->filename;
	}

	# Hand off to main save method
	$self->SUPER::save( $filename, @_ );
}

1;

=pod

=head1 TO DO

- May need to overload some methods to forcefully prevent Document
objects becoming children of another Node.

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
