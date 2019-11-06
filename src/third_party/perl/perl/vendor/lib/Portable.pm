package Portable;

=pod

=head1 NAME

Portable - Perl on a Stick

=head1 SYNOPSIS

Launch a script portably

  F:\anywhere\perl.exe -MPortable script.pl

Have a script specifically request to run portably

  #!/usr/bin/perl
  use Portable;

=head1 DESCRIPTION

"Portable" is a term used for applications that are installed onto a
portable storage device (most commonly a USB memory stick) rather than
onto a single host.

This technique has become very popular for Windows applications, as it
allows a user to make use of their own software on typical publically
accessible computers at libraries, hotels and internet cafes.

Converting a Windows application into portable form has a specific set
of challenges, as the application has no access to the Windows registry,
no access to "My Documents" type directories, and does not exist at a
reliable filesystem path (because the portable storage medium can be
mounted at an arbitrary volume or filesystem location).

B<Portable> provides a methodology and implementation to support
the creating of "Portable Perl" applications and distributions.

While this will initially be focused on a Windows implementation,
wherever possible the module will be built to be platform-agnostic
in the hope that future versions can support other operating systems,
or work across multiple operating systems.

This module is not ready for public use. For now, see the code for
more details on how it works...

=head1 METHODS

=cut

use 5.008;
use strict;
use warnings;
use Carp                   ();
use File::Spec        3.29 ();
use Parse::CPAN::Meta 1.39 ();

our $VERSION = '1.17';

# This variable is provided exclusively for the
# use of test scripts.
our $FAKE_PERL;

# Globally-accessible flag to see if Portable is enabled.
# Defaults to undef, because if Portable.pm is not loaded
# AT ALL, $Portable::ENABLED returns undef anyways.
our $ENABLED = undef;

# Param-checking
sub _STRING ($) {
	(defined $_[0] and ! ref $_[0] and length($_[0])) ? $_[0] : undef;
}
sub _HASH ($) {
	(ref $_[0] eq 'HASH' and scalar %{$_[0]}) ? $_[0] : undef;
}
sub _ARRAY ($) {
	(ref $_[0] eq 'ARRAY' and @{$_[0]}) ? $_[0] : undef;
}

# Package variables
my %applied;
my $cache;





#####################################################################
# Pragma/Import Interface

sub import {
	my $class   = shift;
	$class->apply( @_ ? @_ : qw{ Config CPAN } );
}

sub apply {
	# default %applied;
	my $class   = shift;
	my $self    = $class->default;
	my %apply   = map { $_ => 1 } @_;
	if ( $apply{Config} and ! $applied{Config} ) {
		$self->config->apply($self);
		$applied{Config}  = 1;
		$ENABLED          = 1;
	}
	if ( $apply{CPAN} and ! $applied{CPAN} and $self->cpan ) {
		$self->cpan->apply($self);
		$applied{CPAN}    = 1;
		$ENABLED          = 1;
	}
	if ( $apply{HomeDir} and ! $applied{HomeDir} and $self->homedir ) {
		$self->homedir->apply($self);
		$applied{HomeDir} = 1;
		$ENABLED          = 1;
	}

	# We don't need to do anything for CPAN::Mini.
	# It will load us instead (I think)

	return 1;
}

sub applied {
	$applied{$_[1]};
}





#####################################################################
# Constructors

sub new {
	my $class = shift;
	my $self  = bless { @_ }, $class;

	# Param checking
	unless ( exists $self->{dist_volume} ) {
		Carp::croak('Missing or invalid dist_volume param');
	}
	unless ( _STRING($self->dist_dirs) ) {
		Carp::croak('Missing or invalid dist_dirs param');
	}
	unless ( _STRING($self->dist_root) ) {
		Carp::croak('Missing or invalid dist_root param');
	}
	unless ( _HASH($self->{portable}) ) {
		Carp::croak('Missing or invalid portable param');
	}

	# Compulsory support for Config.pm
	require Portable::Config;
	$self->{Config} = Portable::Config->new( $self );

	# Optional support for CPAN::Config
	if ( $self->portable_cpan ) {
		require Portable::CPAN;
		$self->{CPAN} = Portable::CPAN->new( $self );
	}

	# Optional support for File::HomeDir
	if ( $self->portable_homedir ) {
		require Portable::HomeDir;
		$self->{HomeDir} = Portable::HomeDir->new( $self );
	}

	# Optional support for CPAN::Mini
	if ( $self->portable_minicpan ) {
		require Portable::minicpan;
		$self->{minicpan} = Portable::minicpan->new( $self );
	}

	return $self;
}

sub default {
	# state $cache;
	return $cache if $cache;

	# Get the perl executable location
	my $perlpath = ($ENV{HARNESS_ACTIVE} and $FAKE_PERL) ? $FAKE_PERL : $^X;

	# The path to Perl has a localized path.
	# G:\\strawberry\\perl\\bin\\perl.exe
	# Split it up, and search upwards to try and locate the
	# portable.perl file in the distribution root.
	my ($dist_volume, $d, $f) = File::Spec->splitpath($perlpath);
	my @d = File::Spec->splitdir($d);
	pop @d if $d[-1] eq '';
	my @tmp = grep {
			-f File::Spec->catpath( $dist_volume, $_, 'portable.perl' )
		}
		map {
			File::Spec->catdir(@d[0 .. $_])
		} reverse ( 0 .. $#d );
	my $dist_dirs = $tmp[0];
	unless ( defined $dist_dirs ) {
		Carp::croak("Failed to find the portable.perl file");
	}

	# Derive the main paths from the plain dirs
	my $dist_root = File::Spec->catpath($dist_volume, $dist_dirs, '' );
	my $conf      = File::Spec->catpath($dist_volume, $dist_dirs, 'portable.perl' );

	# Load the YAML file
	my $portable = Parse::CPAN::Meta::LoadFile( $conf );
	unless ( _HASH($portable) ) {
		Carp::croak("Missing or invalid portable.perl file");
	}

	# Hand off to the main constructor,
	# cache the result and return it
	$cache = __PACKAGE__->new(
		dist_volume => $dist_volume,
		dist_dirs   => $dist_dirs,
		dist_root   => $dist_root,
		conf        => $conf,
		perlpath    => $perlpath,
		portable    => $portable,
	);
}





#####################################################################
# Configuration Accessors

sub dist_volume {
	$_[0]->{dist_volume};
}

sub dist_dirs {
	$_[0]->{dist_dirs};
}

sub dist_root {
	$_[0]->{dist_root};
}

sub conf {
	$_[0]->{conf};
}

sub perlpath {
	$_[0]->{perlpath};
}

sub portable_cpan {
	$_[0]->{portable}->{CPAN};
}

sub portable_config {
	$_[0]->{portable}->{Config};
}

sub portable_homedir {
	$_[0]->{portable}->{HomeDir};
}

sub portable_minicpan {
	$_[0]->{portable}->{minicpan};
}

sub portable_env {
	$_[0]->{portable}->{Env};
}

sub config {
	$_[0]->{Config};
}

sub cpan {
	$_[0]->{CPAN};
}

sub homedir {
	$_[0]->{HomeDir};
}

sub minicpan {
	$_[0]->{minicpan};
}

sub env {
	$_[0]->{Env};
}

1;

=pod

=head1 SUPPORT

Bugs should be reported via the CPAN bug tracker.

L<http://rt.cpan.org/NoAuth/ReportBug.html?Queue=Portable>

For other issues, or commercial support, contact the author.

=head1 AUTHOR

Adam Kennedy E<lt>adamk@cpan.orgE<gt>

=head1 SEE ALSO

L<http://win32.perl.org/>

=head1 COPYRIGHT

Copyright 2008 - 2011 Adam Kennedy.

This program is free software; you can redistribute
it and/or modify it under the same terms as Perl itself.

The full text of the license can be found in the
LICENSE file included with this module.

=cut
