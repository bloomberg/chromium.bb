package Win32::File::Object;

=pod

=head1 NAME

Win32::File::Object - Simplified object abstraction over Win32::File

=head1 SYNOPSIS

  # Get a handle for the file.
  my $object = Win32::File::Object->new( $filename, $autowrite );
  
  # Read a property flag for the file.
  my $readonly = $object->readonly;
  
  # Set a propertly flag for the file.
  $object->readonly(1);
  
  # If autowrite is false, write the changes to the file.
  $object->write;

=head1 DESCRIPTION

L<Win32::File> is an interface to the Win32 API for file
attributes.

Unfortunately it is a B<direct> interface to the underlying Win32 API,
with a completely non-Perlish interface involving CamelCase function
names, bit-field flags and return-by-param.

B<Win32::File::Object> is a straight-forward object-oriented Perlish
wrapper around the raw underlying API wrapper.

=head1 METHODS

=cut

use 5.006;
use strict;
use Carp        ();
use Win32::File ();

use vars qw{$VERSION};
BEGIN {
	$VERSION = '0.02';
}





#####################################################################
# Constructor

=pod

=head2 new

  my $file = Win32::File::Object->new( $path, $autowrite );

The C<new> constructor creates a new handle to the Win32 filesystem
attributes of an existing file or directory.

The compulsory C<$filename> parameter is the name of the file or
directory to create the handle on.

The optional C<$autowrite> parameter, if true, indicates that the
object should write the filesystem attributes to the file every
time the method is called to set the property.

If the C<$autowrite> param is false or not provided, you will
need to call an explicit C<write> method in order to apply the
changes to the file.

=cut

sub new {
	my $class     = shift;
	my $path      = shift;
	my $autowrite = !! shift;
	unless ( $path ) {
		Carp::croak("Did not provide a file name");
	}
	unless ( -f $path ) {
		Carp::croak("File '$path' does not exist");
	}

	# Create the object
	my $self = bless {
		path      => $path,
		autowrite => $autowrite,
		rollback  => ! 1,
	}, $class;

	# Get the attributes
	$self->read;

	return $self;
}

=pod

=head2 path

The C<path> accessor returns the original file path as provided to
the constructor as a string.

=cut

sub path {
	$_[0]->{path};
}

=pod

=head2 autowrite

The C<autowrite> accessor returns true if the object will
automatically write changes to the filesystem, or false if
not.

=cut

sub autowrite {
	$_[0]->{autowrite};
}





#####################################################################
# Main Methods

=pod

=head2 read

the C<read> method reads (updates) the filesystem attributes, in case
they have been updated since the object was originally created.

Returns true on success or throws an exception (dies) on error.

=cut

sub read {
	my $self = shift;

	# Read the bitfield
	my $bits;
	my $path = $self->path;
	unless ( Win32::File::GetAttributes( $self->path => $bits ) ) {
		Carp::croak("GetAttributes failed for '$path'");
	}

	# Read the flags
	$self->{archive}    = ( $bits & Win32::File::ARCHIVE()    ) ? 1 : 0;
	$self->{compressed} = ( $bits & Win32::File::COMPRESSED() ) ? 1 : 0;
	$self->{directory}  = ( $bits & Win32::File::DIRECTORY()  ) ? 1 : 0;
	$self->{hidden}     = ( $bits & Win32::File::HIDDEN()     ) ? 1 : 0;
	$self->{normal}     = ( $bits & Win32::File::NORMAL()     ) ? 1 : 0;
	$self->{offline}    = ( $bits & Win32::File::OFFLINE()    ) ? 1 : 0;
	$self->{readonly}   = ( $bits & Win32::File::READONLY()   ) ? 1 : 0;
	$self->{system}     = ( $bits & Win32::File::SYSTEM()     ) ? 1 : 0;
	$self->{temporary}  = ( $bits & Win32::File::TEMPORARY()  ) ? 1 : 0;

	return 1;
}

=pod

=head2 write

the C<write> method writes the object attributes back to the filesystem.

Returns true on success or throws an exception (dies) on error.

=cut

sub write {
	my $self = shift;

	# Generate the bitfield from the attributes
	my $bits = 0;
	if ( $self->archive ) {
		$bits += Win32::File::ARCHIVE();
	}
	if ( $self->compressed ) {
		$bits += Win32::File::COMPRESSED();
	}
	if ( $self->directory ) {
		$bits += Win32::File::DIRECTORY();
	}
	if ( $self->hidden ) {
		$bits += Win32::File::HIDDEN();
	}
	if ( $self->normal ) {
		$bits += Win32::File::NORMAL();
	}
	if ( $self->offline ) {
		$bits += Win32::File::OFFLINE();
	}
	if ( $self->readonly ) {
		$bits += Win32::File::READONLY();
	}
	if ( $self->system ) {
		$bits += Win32::File::SYSTEM();
	}
	if ( $self->temporary ) {
		$bits += Win32::File::TEMPORARY();
	}

	# Apply the attributes to the file
	my $path = $self->path;
	unless ( Win32::File::SetAttributes( $path, $bits ) ) {
		Carp::croak("SetAttributes failed for '$path'");
	}

	return 1;
}





#####################################################################
# Attribute Methods

=pod

=head2 archive

  # Get the value
  my $archive = $file->archive;
  
  # Set the value
  $file->archive(1);

The C<archive> accessor gets or set the Win32 "archive" status for
the file.

=cut

sub archive {
	shift->_attr( archive => @_ );
}

=pod

=head2 compressed

  # Get the value
  my $compressed = $file->compressed;
  
  # Set the value
  $file->compressed(1);

The C<compressed> accessor gets or set the Win32 "compressed" status
for the file.

=cut

sub compressed {
	shift->_attr( compressed => @_ );
}

=pod

=head2 directory

  # Get the value
  my $directory = $file->directory;
  
  # Set the value
  $file->directory(1);

The C<directory> accessor gets or set the Win32 "directory" status for
the file.

=cut

sub directory {
	shift->_attr( directory => @_ );
}

=pod

=head2 hidden

  # Get the value
  my $hidden = $file->hidden;
  
  # Set the value
  $file->hidden(1);

The C<hidden> accessor gets or set the Win32 "hidden" status for
the file.

=cut

sub hidden {
	shift->_attr( hidden => @_ );
}

=pod

=head2 normal

  # Get the value
  my $normal = $file->normal;
  
  # Set the value
  $file->normal(1);

The C<normal> accessor gets or set the Win32 "normal" status for
the file.

=cut

sub normal {
	shift->_attr( normal => @_ );
}

=pod

=head2 offline

  # Get the value
  my $offline = $file->offline;
  
  # Set the value
  $file->offline(1);

The C<offline> accessor gets or set the Win32 "offline" status for
the file.

=cut

sub offline {
	shift->_attr( offline => @_ );
}

=pod

=head2 readonly

  # Get the value
  my $readonly = $file->readonly;
  
  # Set the value
  $file->readonly(1);

The C<readonly> accessor gets or set the Win32 "readonly" status for
the file.

=cut

sub readonly {
	shift->_attr( readonly => @_ );
}

=pod

=head2 system

  # Get the value
  my $system = $file->system;
  
  # Set the value
  $file->system(1);

The C<system> accessor gets or set the Win32 "system" status for
the file.

=cut

sub system {
	shift->_attr( system => @_ );
}

=pod

=head2 temporary

  # Get the value
  my $temporary = $file->temporary;
  
  # Set the value
  $file->temporary(1);

The C<temporary> accessor gets or set the Win32 "temporary" status for
the file.

=cut

sub temporary {
	shift->_attr( temporary => @_ );
}

sub _attr {
	my $self = shift;
	my $name = shift;
	my $new  = $_[0] ? 1 : 0;
	return $self->{$name} unless @_;
	return $self->{$name} if $new == $self->{$name};

	# Set the rollback if needed
	if ( $self->{rollback} and ! exists $self->{rollback}->{$name} ) {
		$self->{rollback}->{$name} = $new;
	}

	# Set the new value
	$self->{$name} = $new;
	$self->write if $self->autowrite;

	return $self->{$name};
}

1;

=pod

=head1 SUPPORT

Bugs should be reported via the CPAN bug tracker at

L<http://rt.cpan.org/NoAuth/ReportBug.html?Queue=Win32-File-Object>

For other issues, or commercial enhancement or support, contact the author.

=head1 AUTHOR

Adam Kennedy E<lt>adamk@cpan.orgE<gt>

=head1 SEE ALSO

L<Win32::File>

=head1 COPYRIGHT

Copyright 2008 - 2009 Adam Kennedy.

This program is free software; you can redistribute
it and/or modify it under the same terms as Perl itself.

The full text of the license can be found in the
LICENSE file included with this module.

=cut
