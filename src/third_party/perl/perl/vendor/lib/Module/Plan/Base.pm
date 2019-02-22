package Module::Plan::Base;

=pod

=head1 NAME

Module::Plan::Base - Base class for Module::Plan classes

=head1 DESCRIPTION

B<Module::Plan::Base> provides the underlying basic functionality. That is,
taking a file, injecting it into CPAN, and the installing it via the L<CPAN>
module.

It also provides for a basic "phase" system, that allows steps to be taken
in the appropriate order. This is very simple for now, but may be upgraded
later into a dependency-based system.

This class is undocumented for the moment.

See L<pip> for the front-end console application for this module.

=cut

use 5.006;
use strict;
use Carp           'croak';
use File::Spec     ();
use File::Temp     ();
use File::Basename ();
use Params::Util   qw{ _STRING _CLASS _INSTANCE };
use URI            ();
use URI::file      ();
#use LWP::Simple    (); # Loaded on-demand with require
#use CPAN::Inject   (); # Loaded on-demand with require
#use PAR::Dist      (); # Loaded on-demand with require
BEGIN {
	# Versions of CPAN older than 1.88 strip off '.' from @INC,
	# breaking stuff. At 1.88 CPAN changed to converting them
	# to absolute paths via rel2abs instead.
	# This is an exact copy of the code that does this, which
	# will allow Module::Plan::Base to work with versions of CPAN.pm
	# older than 1.88 without being impacted by the bug.
	# This is mainly good, because forcing CPAN.pm to be upgraded
	# has problems of it's own, and so by using this hack we can
	# install correctly with the version of CPAN.pm bundled with
	# older versions of Perl.
	foreach my $inc ( @INC ) {
		$inc = File::Spec->rel2abs($inc) unless ref $inc;
	}
}
use CPAN;

use vars qw{$VERSION};
BEGIN {
	$VERSION = '1.19';
}





#####################################################################
# Constructor and Accessors

sub new {
	my $class = shift;
	my $self  = bless { @_ }, $class;

	# Create internal state variables
	$self->{names}     = [ ];
	$self->{uris}      = { };
	$self->{dists}     = { };
	$self->{cpan_path} = { };

	# Precalculate the various paths for the P5I file
	$self->{p5i_uri}   = $self->_p5i_uri( $self->p5i     );
	$self->{p5i_dir}   = $self->_p5i_dir( $self->p5i_uri );
	$self->{dir}       = File::Temp::tempdir( CLEANUP => 1 );

	# Check the no_inject option
	$self->{no_inject} = !! $self->{no_inject};

	# Create the CPAN injector
	unless ( $self->no_inject ) {
		require CPAN::Inject;
		$self->{inject} ||= CPAN::Inject->from_cpan_config;
		unless ( _INSTANCE($self->{inject}, 'CPAN::Inject') ) {
			croak("Did not provide a valid 'param' CPAN::Inject object");
		}
	}

	$self;
}

# Which params do we allow to read
my %READ_ALLOW = ( no_inject => 1 );

sub read {
	my $class  = shift;

	# Check the file
	my $p5i = shift or croak( 'You did not specify a file name' );
	croak( "File '$p5i' does not exist" )              unless -e $p5i;
	croak( "'$p5i' is a directory, not a file" )       unless -f _;
	croak( "Insufficient permissions to read '$p5i'" ) unless -r _;

	# Get a filtered set of params to pass through
	my %params = @_;
	   %params = map { $_ => $params{$_} }
	             grep { $READ_ALLOW{$_} }
	             sort keys %params;

	# Slurp in the file
	my $contents;
	SCOPE: {
		local $/ = undef;
		open CFG, $p5i or croak( "Failed to open file '$p5i': $!" );
		$contents = <CFG>;
		close CFG;
	}

	# Split and find the header line for the type
	my @lines  = split /(?:\015{1,2}\012|\015|\012)/, $contents;
	my $header = shift @lines;
	unless ( _CLASS($header) ) {
		croak("Invalid header '$header', not a class name");
	}

	# Load the class
	require join('/', split /::/, $header) . '.pm';
	unless ( $header->VERSION and $header->isa($class) ) {
		croak("Invalid header '$header', class is not a Module::Plan::Base subclass");
	}

	# MSWIN32: we want this because URI encodes backslashes
	# and encoded backslashes make File::Spec (and later LWP::Simple)
	# confuse afterwords.
	$p5i =~ s{\\}{/}g;

	# Class looks good, create our object and hand off
	return $header->new(
		p5i   => $p5i,
		lines => \@lines,
		%params,
	);
}

sub p5i {
	$_[0]->{p5i};
}

sub p5i_uri {
	$_[0]->{p5i_uri};
}

sub p5i_dir {
	$_[0]->{p5i_dir};
}

sub dir {
	$_[0]->{dir};
}

sub lines {
	@{ $_[0]->{lines} };
}

sub names {
	@{ $_[0]->{names} };
}

sub dists {
	%{ $_[0]->{dists} };
}

sub dists_hash {
	$_[0]->{dists};
}

sub uris {
	my $self = shift;
	my %copy = %{ $self->{uris} };
	foreach my $key ( keys %copy ) {
		$copy{$key} = $copy{$key}->clone;
	}
	%copy;
}

sub no_inject {
	$_[0]->{no_inject};
}

sub inject {
	$_[0]->{inject};
}

# Generate the plan file from the plan object
sub as_string {
	return join '',
		map { "$_\n" }
		$_[0]->can('ref')
			? $_[0]->ref
			: ref $_[0],
		"",
		$_[0]->lines;
}





#####################################################################
# Files and Installation

sub add_file {
	my $self = shift;
	my $file = _STRING(shift) or croak("Did not provide a file name");

	# Handle relative and absolute paths
	$file = File::Spec->rel2abs( $file, $self->dir );
	my (undef, undef, $name) = File::Spec->splitpath( $file );

	# Check for duplicates
	if ( scalar grep { $name eq $_ } @{$self->{names}} ) {
		croak("Duplicate file $name in plan");
	}

	# Add the name and the file name
	push @{ $self->{names} }, $name;
	$self->{dists}->{$name} = $file;

	return 1;
}

sub add_uri {
	my $self = shift;
	my $uri  = _INSTANCE(shift, 'URI') or croak("Did not provide a URI");
	unless ( $uri->can('path') ) {
		croak("URI is not have a ->path method");
	}

	# Split into segments to get the file
	my @segments = $uri->path_segments;
	my $name     = $segments[-1];

	# Check for duplicates
	if ( scalar grep { $name eq $_ } @{$self->{names}} ) {
		croak("Duplicate file $name in plan");
	}

	# Add the name and the file name
	push @{ $self->{names} }, $name;
	$self->{uris}->{$name} = $uri;

	return 1;
}

sub run {
	die ref($_[0]) . " does not implement 'run'";
}

sub _fetch_uri {
	my $self = shift;
	my $name = shift;
	my $uri  = $self->{uris}->{$name};
	unless ( $uri ) {
		die("Unknown uri for $name");
	}

	# Determine the dists file name
 	my $file = File::Spec->catfile( $self->{dir}, $name );
	if ( -f $file ) {
		die("File $file already exists");
	}
	$self->{dists}->{$name} = $file;

	# Download the URI to the destination
	require LWP::Simple;
	my $content = LWP::Simple::get( $uri );
	unless ( defined $content ) {
		croak("Failed to download $uri");
	}

	# Save the file
	unless ( open( DOWNLOAD, '>', $file ) ) {
		croak("Failed to open $file to write");
	}
	binmode( DOWNLOAD );
	unless ( print DOWNLOAD $content ) {
		croak("Failed to write to $file");
	}
	unless ( close( DOWNLOAD ) ) {
		croak("Failed to close $file");
	}

	return 1;
}

sub _cpan_inject {
	my $self = shift;
	my $name = shift;
	my $file = $self->{dists}->{$name};
	unless ( $file ) {
		die("Unknown file $name");
	}

	# Inject the file into the CPAN cache
	$self->{cpan_path}->{$name} = $self->inject->add( file => $file );

	1;
}

sub _cpan_install {
	my $self   = shift;
	my $name   = shift;
	my $distro = $self->{cpan_path}->{$name};
	unless ( $distro ) {
		die("Unknown file $name");
	}

	# Install via the CPAN::Shell
	CPAN::Shell->install($distro);
}

sub _par_install {
	my $self = shift;
	my $name = shift;
	my $uri  = $self->{uris}->{$name};
	unless ( $uri ) {
		die("Unknown uri for $name");
	}

	# Install entirely using PAR::Dist
	require PAR::Dist;
	PAR::Dist::install_par( $uri->as_string );
}

# Takes arbitrary param, returns URI to the P5I file
sub _p5i_uri {
	my $uri = _INSTANCE($_[1], 'URI') ? $_[1]
		: _STRING($_[1])          ? URI->new($_[1])
		: undef
		or croak("Not a valid P5I path");

	# Convert generics to file URIs
	unless ( $uri->scheme ) {
		# It's a raw filename
		$uri = URI::file->new($uri->as_string) or croak("Not a valid P5I path");
	}

	# Make any file paths absolute
	if ( $uri->isa('URI::file') ) {
		my $file = File::Spec->rel2abs( $uri->path );
		$uri = URI::file->new($file);
	}

	$uri;
}

sub _p5i_dir {
	my $uri = _INSTANCE($_[1], 'URI')
		or croak("Did not pass a URI to p5i_dir");

	# Use a naive method for the moment
	my $string = $uri->as_string;
	$string =~ s/\/[^\/]+$//;

	# Return the modified version
	URI->new( $string, $uri->scheme );
}

1;

=pod

=head1 SUPPORT

See the main L<pip> module for support information.

=head1 AUTHORS

Adam Kennedy E<lt>adamk@cpan.orgE<gt>

=head1 SEE ALSO

L<pip>, L<Module::Plan>, L<Module::Inspector>

=head1 COPYRIGHT

Copyright 2006 - 2010 Adam Kennedy.

This program is free software; you can redistribute
it and/or modify it under the same terms as Perl itself.

The full text of the license can be found in the
LICENSE file included with this module.

=cut
