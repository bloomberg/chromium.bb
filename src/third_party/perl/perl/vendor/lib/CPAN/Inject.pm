package CPAN::Inject;

=pod

=head1 NAME

CPAN::Inject - Base class for injecting distributions into CPAN sources

=head1 SYNOPSIS

  # Create the injector
  my $cpan = CPAN::Inject->new(
      sources => '/root/.cpan/sources',  # Required field
      author  => 'LOCAL',                # The default
      );
  
  # Add a file to the user
  $cpan->add( file => 'some/random/Perl-Tarball-1.02.tar.gz' );
  
  # What would have have to use when installing
  # $path = 'LOCAL/Perl-Tarball-1.02.tar.gz';
  my $path = $cpan->install_path( 'some/random/Perl-Tarball-1.02.tar.gz' );

=head1 DESCRIPTION

Following the release of L<CPAN::Mini>, the L<CPAN::Mini::Inject> module
was created to add additional distributions into a minicpan mirror.

While it was created for use with a minicpan mirror, similar
functionality can be reused in other situations.

B<CPAN::Inject> replicates the basics of this functionality.

Specifically, it takes an arbitrary tarball and adds it to the CPAN
sources directory for a particular author, and then add the new file
to the F<CHECKSUMS> file.

It does not reimplement the logic to add files to the indexes.

The initial use this module was created for was to inject tarballs into
the CPAN sources directory for the reserved LOCAL user, so that the can be
installed via the CPAN shell, with automated recursion to CPAN dependencies.

But although the number of functions is limited (current only C<add> exists,
with the others to be added as needed) the implementation is very generic
and sub-classable, so that it can be reused in other situations.

=head1 METHODS

=cut

use 5.006;
use strict;
use Params::Util    ();
use File::stat      ();
use File::chmod     ();
use File::Spec      ();
use File::Path      ();
use File::Copy      ();
use File::Basename  ();
use CPAN::Checksums ();

use vars qw{$VERSION $CHECK_OWNER};

BEGIN {
	$VERSION = '1.13';

	# Attempt to determine whether or not we are capable
	# of finding the owner of a directory.
	# Unless someone set it to a hard-coded value before we
	# started to load this module.
	unless ( defined $CHECK_OWNER ) {
		# Take a directory we know should exist...
		my $root = File::Spec->rootdir();
		unless ( -d $root ) {
			die "Cannot determine if CPAN::Inject can operate on this platform";
		}

		# ... find the owner for it...
		my $owner = File::stat::stat($root)->uid;

		# ... and if it works, check again in the future.
		# Unless someone set it already, in which case
		$CHECK_OWNER = defined $owner ? 1 : '';
	}

	# And boolify the value, just to be a little safer
	$CHECK_OWNER = !! $CHECK_OWNER;
}





#####################################################################
# Constructor and Accessors

=pod

=head2 new

  # Create the injector for the default LOCAL author
  $cpan = CPAN::Inject->new(
      sources => '/root/.cpan/sources',
      );
  
  # Create the injector for a specific author
  $cpan = CPAN::Inject->new(
      sources => '/root/.cpan/sources',
      author  => 'ADAMK',
      );

The C<new> constructor takes a set of named params and create a cpan
injection object.

* B<sources> - The compulsory C<sources> param should be the path to a
directory that is the root of a mirror (or a partial mirror such as a
L<CPAN::Cache> or a L<CPAN::Mini>).

To retain the permissions and ownership integrity of the sources tree,
you must be the owner of the C<sources> directory in order to inject the
distribution tarballs.

* B<author> - The optional C<author> param should be the CPAN id of an
author. By default, the reserved local CPAN id "LOCAL" will be used.

The author provided will be used as a default in all further actions.

Returns a C<CPAN::Inject> object, or throws an exception on error.

=cut

sub new {
	my $class = shift;
	my $self  = bless {@_}, $class;

	# Check where we are going to write to
	my $sources = $self->sources;
	unless ( Params::Util::_STRING($sources) ) {
		Carp::croak("Did not probide a sources param, or not a string");
	}
	unless ( -d $sources ) {
		# The sources directory may actually exist, but we cannot
		# see it because we do not have execute permissions to the
		# parent directory tree.
		# For example, if it is at /root/.cpan/source and we do not
		# have -x permissions to /root
		my ($v, $d) = File::Spec->splitpath( $sources, 'nofile' );
		my @dirs    = File::Spec->splitdir( $d );

		# Ignore the last directory, since that is what we -d tested
		pop @dirs;

		# Check for the existance and rx status of each parent
		foreach my $i ( 0 .. $#dirs ) {
			my $parent = File::Spec->catpath(
				$v,
				File::Spec->catdir( @dirs[0..$i] ),
				'', # No file (returns just the dir)
				);
			unless ( -d $parent ) {
				Carp::croak("The directory '$sources' does not exist");
			}
			unless ( -r $parent and -x $parent ) {
				# Assume that it does exist, but that we can't see it
				Carp::croak("The sources directory is not owned by the current user");
			}
		}
		Carp::croak("The directory '$sources' does not exist");
	}
	unless ( $< == File::stat::stat($sources)->uid ) {
		Carp::croak("The sources directory is not owned by the current user");
	}

	# Check for a default author name
	$self->{author} = 'LOCAL' unless $self->author;
	unless ( _AUTHOR( $self->author ) ) {
		Carp::croak( "The author name '"
			. $self->author
			. "' is not a valid author string"
		);
	}

	$self;
}

=pod

=head2 from_cpan_config

The C<from_cpan_config> constructor loads the CPAN.pm configuration file, and
uses the data contained within to specific the sources path for the
object.

This constructor is otherwise the same.

Returns a B<CPAN::Inject> object on success, or throws an exception on
error.

=cut

sub from_cpan_config {
	my $class = shift;

	# Load the CPAN module
	require CPAN;

	# Support for different mechanisms depending on the version
	# of CPAN that is in use.
	if ( defined $CPAN::HandleConfig::VERSION ) {
		CPAN::HandleConfig->load;
	} else {
		CPAN::Config->load;
	}

	# Get the sources directory
	my $sources = undef;
	if ( defined $CPAN::Config->{keep_source_where} ) {
		$sources = $CPAN::Config->{keep_source_where};
	} elsif ( defined $CPAN::Config->{cpan_home} ) {
		$sources = File::Spec->catdir( $CPAN::Config->{cpan_home}, 'sources' );
	} else {
		Carp::croak("Failed to find sources directory in CPAN::Config");
	}

	# Hand off to the main constructor
	return $class->new(
		sources => $sources,
		@_,
		);
}

=pod

=head2 sources

The C<sources> accessor returns the path to the root of the directory tree.

=cut

sub sources {
	$_[0]->{sources};
}

=pod

=head2 author

The C<author> accessor returns the CPAN id for the default author which
will be "LOCAL" if you did not provide an alternative param to the the
C<new> constructor.

=cut

sub author {
	$_[0]->{author};
}





#####################################################################
# Main methods

=pod

=head2 add

  # Add a file to the constructor/default author
  $cpan->add( file => 'any/arbitrary/Perl-Tarball-1.01.tar.gz' );

The C<add> method takes a Perl distribution tarball from an arbitrary
path, and adds it to the sources path.

The specific location the tarball is copied to will be in the root
directory for the author provided to the constructor.

Returns the install_path value as a convenience, or throws an exception
on error.

=cut

sub add {
	my $self   = shift;
	my %params = @_;

	# Check the file source path
	my $from_file = $params{file};
	unless ( $from_file and -f $from_file and -r $from_file ) {
		Carp::croak("Did not provide a file name, or does not exist");
	}

	# Get the file name
	my $name = File::Basename::fileparse($from_file)
		or die "Failed to get filename";

	# Find the location to copy it to
	my $to_file = $self->file_path($name);
	my $to_dir  = File::Basename::dirname($to_file);

	# Make the path for the file
	SCOPE: {
		local $@;
		eval {
			File::Path::mkpath($to_dir);
		};
		if ( my $e = $@ ) {
			Carp::croak("Failed to create $to_dir: $e");
		}
	}

	# Copy the file to the directory, and ensure writable
	File::Copy::copy( $from_file => $to_file )
		or Carp::croak("Failed to copy $from_file to $to_file");
	chmod( 0644, $to_file )
		or Carp::croak("Failed to correct permissions for $to_file");

	# Update the checksums file, and ensure writable
	SCOPE: {
		local $@;
		eval {
			CPAN::Checksums::updatedir($to_dir);
		};
		if ( my $e = $@ ) {
			Carp::croak("Failed to update CHECKSUMS after insertion: $e");
		}
	}
	chmod( 0644, File::Spec->catfile( $to_dir, 'CHECKSUMS' ) )
		or Carp::croak("Failed to correct permissions for CHECKSUMS");

	# Return the install_path as a convenience
	$self->install_path($name);
}

=pod

=head2 remove

  # Remove a distribution from the repository
  $cpan->remove( dist => 'LOCAL/Perl-Tarball-1.01.tar.gz' );

The C<remove> method takes a distribution path and removes it from the
sources path. The file is also removed.

Does not return anything useful and throws an exception on error.

=cut

sub remove {
	my $self   = shift;
	my %params = @_;

	# Get the file name
	my $name = File::Basename::fileparse($params{dist})
		or die "Failed to get filename";

	my $file_path = $self->file_path($name);

	# Remove the file from CPAN.
	unlink $file_path while -e $file_path;

	# Update the checksums file
	my $to_file = $self->file_path($name);
	my $to_dir  = File::Basename::dirname($to_file);
	SCOPE: {
		local $@;
		eval {
			CPAN::Checksums::updatedir($to_dir);
		};
		if ( my $e = $@ ) {
			Carp::croak("Failed to update CHECKSUMS after removal: $e");
		}
	}

	return 1;
}

=pod

=head2 author_subpath

  # $path = 'authors/id/L/LO/LOCAL'
  $path = $cpan->author_subpath;

The C<author_subpath> method takes a CPAN author id (or uses the CPAN
author id originally provided to the constructor) and returns the
relative subpath for the AUTHOR within the sources tree.

Returns the subpath as a string.

=cut

sub author_subpath {
	my $author = $_[0]->author;
	File::Spec->catdir(
		'authors', 'id',
		substr( $author, 0, 1 ),
		substr( $author, 0, 2 ), $author,
	);
}

=pod

=head2 author_path

  # $path = '/root/.cpan/sources/authors/id/L/LO/LOCAL'
  $path = $cpan->author_subpath;

The C<author_path> method finds the full path for the root directory for
the named author.

Returns the path as a string.

=cut

sub author_path {
	File::Spec->catdir( $_[0]->sources, $_[0]->author_subpath, );
}

=pod

=head2 file_path

  # $path = '/root/.cpan/sources/authors/id/L/LO/LOCAL/Perl-Tarball-1.02.tar.gz'
  $path = $cpan->file_path( 'Perl-Tarball-1.02.tar.gz' );
  $path = $cpan->file_path( '/some/random/place/Perl-Tarball-1.02.tar.gz' );

The C<file_path> method takes the name of a tarball (either just the name
or a full path) and calculates the location that the file will end up at.

When files are copied into the sources directory, they are always copied
to the top level of the author root.

Returns the path as a string.

=cut

sub file_path {
	File::Spec->catfile( $_[0]->sources, $_[0]->author_subpath, $_[1], );
}

=pod

=head2 install_path

  # $path = 'LOCAL/Perl-Tarball-1.01.tar.gz';
  $path = $cpan->install_path( 'Perl-Tarball-1.01.tar.gz' );
  $path = $cpan->install_path( '/some/random/place/Perl-Tarball-1.02.tar.gz' );

The C<install_path> method returns the path for the distribution as the
CPAN shell understands it.

Using this path, the CPAN shell can expand it to locate the
distribution, and then can install it.

Returns the path as a string.

=cut

sub install_path {
	my $self = shift;
	my $file = File::Basename::fileparse(shift)
		or Carp::croak("Failed to get filename");
	join( '/', $self->author, $file );
}





#####################################################################
# Support Functions

sub _AUTHOR {
	( Params::Util::_STRING( $_[0] ) and $_[0] =~ /^[A-Z]{3,}$/ ) ? $_[0] : undef;
}

1;

=pod

=head1 SUPPORT

This module is stored in an Open Repository at the following address.

L<http://svn.ali.as/cpan/trunk/CPAN-Inject>

Write access to the repository is made available automatically to any
published CPAN author, and to most other volunteers on request.

If you are able to submit your bug report in the form of new (failing)
unit tests, or can apply your fix directly instead of submitting a patch,
you are B<strongly> encouraged to do so as the author currently maintains
over 100 modules and it can take some time to deal with non-Critcal bug
reports or patches.

This will guarentee that your issue will be addressed in the next
release of the module.

If you cannot provide a direct test or fix, or don't have time to do so,
then regular bug reports are still accepted and appreciated via the CPAN
bug tracker.

L<http://rt.cpan.org/NoAuth/ReportBug.html?Queue=CPAN-Inject>

For other issues, for commercial enhancement or support, or to have your
write access enabled for the repository, contact the author at the email
address above.

=head1 AUTHOR

Adam Kennedy E<lt>adamk@cpan.orgE<gt>

=head1 SEE ALSO

L<CPAN::Mini::Inject>

=head1 COPYRIGHT

Copyright 2006 - 2011 Adam Kennedy.

This program is free software; you can redistribute
it and/or modify it under the same terms as Perl itself.

The full text of the license can be found in the
LICENSE file included with this module.

=cut
