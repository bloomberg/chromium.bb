package pler;

# See 'sub main' for main functionality

use 5.00503;
use strict;
use Config;
use Carp                       ();
use Cwd                   3.00 ();
use File::Which           0.05 ();
use File::Spec            0.80 ();
use File::Spec::Functions      ':ALL';
use File::Find::Rule      0.20 ();
use Getopt::Long             0 ();
use Probe::Perl           0.01 ();

use vars qw{$VERSION};
BEGIN {
        $VERSION = '1.06';
}

# Does exec work on this platform
use constant EXEC_OK => ($^O ne 'MSWin32' and $^O ne 'cygwin');

# Can you overwrite an open file on this platform
use constant OVERWRITE_OK => !! ( $^O ne 'MSWin32' );






#####################################################################
# Resource Locations

sub MakefilePL () {
	catfile( curdir(), 'Makefile.PL' );
}

sub BuildPL () {
	catfile( curdir(), 'Build.PL' );
}

sub Makefile () {
	catfile( curdir(), 'Makefile' );
}

sub Build () {
	catfile( curdir(), 'Build' );
}

sub perl () {
	Probe::Perl->find_perl_interpreter;
}

# Look for make in $Config
sub make () {
	my $make  = $Config::Config{make};
	my $found = File::Which::which( $make );
	unless ( $found ) {
		Carp::croak("Failed to find '$make' (as specified by \$Config{make})");
	}
	return $found;
}

sub blib () {
	catdir( curdir(), 'blib' );
}

sub inc () {
	catdir( curdir(), 'inc' );
}

sub lib () {
	catdir( curdir(), 'lib' );
}

sub t () {
	catdir( curdir(), 't' );
}

sub xt () {
	catdir( curdir(), 'xt' );
}





#####################################################################
# Convenience Logic

sub has_makefilepl () {
	!! -f MakefilePL;
}

sub has_buildpl () {
	!! -f BuildPL;
}

sub has_makefile () {
	!! -f Makefile;
}

sub has_build () {
	!! -f Build;
}

sub has_blib () {
	!! -d blib;
}

sub blibpm () {
	eval {
		require blib;
	};
	return ! $@;
}

sub has_inc () {
	!! -f inc;
}

sub has_lib () {
	!! -d lib;
}

sub has_t () {
	!! -d t;
}

sub has_xt () {
	!! -d xt;
}

sub in_distroot () {
	!! (
		has_makefilepl or (has_lib and has_t)
	);
}

sub in_subdir () {
	!! (
		-f catfile( updir(), 'Makefile.PL' )
		or
		-d catdir( updir(), 't' )
	);
}

sub needs_makefile () {
	has_makefilepl and ! has_makefile;
}

sub needs_build () {
	has_buildpl and ! has_build;
}

sub mtime ($) {
	(stat($_[0]))[9];
}

sub old_makefile () {
	has_makefile
	and
	has_makefilepl
	and
	mtime(Makefile) < mtime(MakefilePL);
}

sub old_build () {
	has_build
	and
	has_buildpl
	and
	mtime(Build) < mtime(BuildPL);
}





#####################################################################
# Utility Functions

# Support verbosity
use vars qw{$VERBOSE};
BEGIN {
	$VERBOSE ||= 0;
}

sub is_verbose {
	$VERBOSE;
}

sub verbose ($) {
	message( $_[0] ) if $VERBOSE;
}

sub message ($) {
        print $_[0];
}

sub error (@) {
	print ' ' . join '', map { "$_\n" } ('', @_, '');
	exit(255);
}

sub run ($) {
	my $cmd = shift;
	verbose( "> $cmd" );
	system( $cmd );
}

sub handoff (@) {
	my $cmd = join ' ', @_;
	verbose( "> $cmd" );
	$ENV{HARNESS_ACTIVE}  = 1;
	$ENV{RELEASE_TESTING} = 1;
	if ( EXEC_OK ) {
		exec( @_ ) or Carp::croak("Failed to exec '$cmd'");
	} else {
		system( @_ );
		exit(0);
	}
}





#####################################################################
# Main Script

my @SWITCHES = ();

sub main {
	Getopt::Long::Configure('no_ignore_case');
	Getopt::Long::GetOptions(
		'help' => \&help,
		'V'    => sub { print "pler $VERSION\n"; exit(0) }, 
		'w'    => sub { push @SWITCHES, '-w' },
	);

	# Get the script name
	my $script = shift @ARGV;
	unless ( defined $script ) {
		print "# No file name pattern provided, using 't'...\n";
		$script = 't';
	}

	# Abuse the highly mature logic in Cwd to define an $ENV{PWD} value
	# by chdir'ing to the current directory.
	# This lets us get the current directory without losing symlinks.
	Cwd::chdir(curdir());
	my $orig = $ENV{PWD} or die "Failed to get original directory";

        # Can we locate the distribution root
	my ($v,$d,$f) = splitpath($ENV{PWD}, 'nofile');
	my @dirs      = splitdir($d);
	while ( @dirs ) {
		my $buildpl = catpath(
			$v, catdir(@dirs), BuildPL,
		);
		my $makefilepl = catpath(
			$v, catdir(@dirs), MakefilePL,
		);
		unless ( -f $buildpl or -f $makefilepl ) {
			pop @dirs;
			next;
		}

		# This is a distroot
		my $distroot = catpath( $v, catdir(@dirs), undef );
		Cwd::chdir($distroot);
		last;
	}
        unless ( in_distroot ) {
                error "Failed to locate the distribution root";
        }

	# Makefile.PL? Or Build.PL?
	my $BUILD_SYSTEM = has_buildpl ? 'build' : has_makefilepl ? 'make' : '';
	if ( $BUILD_SYSTEM eq 'build' ) {
		# Because Module::Build always runs with warnings on,
		# pler will as well when you use a Build.PL
		unless ( grep { $_ eq '-w' } @SWITCHES ) {
			push @SWITCHES, '-w';
		}
	}

	# If needed, regenerate the Makefile or Build file
	# Currently we do not remember Makefile.PL or Build.PL params
	if ( $BUILD_SYSTEM eq 'make' ) {
		if ( needs_makefile or (old_makefile and ! OVERWRITE_OK) ) {
			run( join ' ', perl, MakefilePL );
		}
	} elsif ( $BUILD_SYSTEM eq 'build' ) {
		if ( needs_build or old_build ) {
			run( join ' ', perl, BuildPL );
		}
	}

	# Locate the test script to run
	if ( $script =~ /\.t$/ ) {
		# EITHER
		# 1. They tab-completed the script relative to the original directory (most likely)
		# OR
		# 2. They typed the entire name of the test script
		my $tab_completed = File::Spec->catfile( $orig, $script );
		if ( -f $tab_completed ) {
			if ( $orig eq $ENV{PWD} ) {
				$script = $script; # Included for clarity
			} else {
				$script = File::Spec->abs2rel( $tab_completed, $ENV{PWD} );
			}
		}

        } else {
		# Get the list of possible tests
		my @directory = ( 't', has_xt ? 'xt' : () );
		my @possible  = File::Find::Rule->name('*.t')->file->in(@directory);

		# Filter by the search terms to find matching tests
		my $matches = filter(
			[ $script, @ARGV ],
			[ @possible ],
		);
		unless ( @$matches ) {
			error "No tests match '$script'";
		}
		if ( @$matches > 1 ) {
			error(
			        "More than one possible test",
		        	map { "  $_" } sort @$matches,
			);
		}
		$script = $matches->[0];

		# Localize the path
		$script = File::Spec->catfile( split /\//, $script );
	}
	unless ( -f $script ) {
		error "Test script '$script' does not exist";
	}

        # Rerun make or Build if needed
	if ( $BUILD_SYSTEM eq 'make' ) {
		# Do NOT run make if there is no Makefile.PL, because it likely means
		# there is a hand-written Makefile and NOT one derived from Makefile.PL,
		# and we have no idea what functionality we might trigger.
        	if ( in_distroot and has_makefile and has_makefilepl ) {
	                run( make );
	        }
	} elsif ( $BUILD_SYSTEM eq 'build' ) {
		if ( in_distroot and has_build and has_buildpl ) {
			run( Build );
		}
	}

	# Passing includes via -I params is not good enough
	# because you can't subshell them, and it's also not
	# how MakeMaker does it anyway.
	# We need to hack/extend PERL5LIB instead.
	my $path_sep = $Config{path_sep};
	my @PERL5LIB = ();

	# Build the command to execute
	my @flags = @SWITCHES;
	if ( has_blib ) {
		if ( has_inc ) {
			push @PERL5LIB, inc;
		}
		push @PERL5LIB, File::Spec->catdir(
			blib, 'lib',
		);
		push @PERL5LIB, File::Spec->catdir(
			blib, 'arch',
		);
	} elsif ( has_lib ) {
		push @PERL5LIB, lib;
	}

	# Absolutify the PERL5LIB elements so they will survive
	# the test script changing it's CWD. This was added to
	# deal with the path-shifting of the Padre tests.
	@PERL5LIB = map {
		File::Spec->rel2abs($_)
	} @PERL5LIB;

	# Hand off to the perl debugger
	unless ( pler->is_verbose ) {
		message( "# Debugging $script...\n" );
	}
	my @cmd = ( perl, @flags, '-d', $script );
	local $ENV{PERL5LIB} = defined($ENV{PERL5LIB})
		? join( $path_sep, @PERL5LIB, $ENV{PERL5LIB} )
		: join( $path_sep, @PERL5LIB );
	handoff( @cmd );
}

# Encapsulates the smart filtering as a function
sub filter {
	my $terms    = shift;
	my $possible = shift;
	my @matches  = @$possible;

	while ( @$terms ) {
		my $term = shift @$terms;

		if ( ref $term eq 'Regexp' ) {
			# If the term is a regexp apply it directly
			@matches = grep { $_ =~ $term } @matches;
		} elsif ( $term =~ /^[1-9]\d*$/ ) {
			# If the search is a pure integer (without leading
			# zeros) attempt a specialised numeric filter.
			@matches = grep { /\b0*${term}[^0-9]/ } @matches;
		} else {
			# Otherwise treat it as a naive string match
			$term = quotemeta $term;
			@matches = grep { /$term/i } @matches;
		}
	}

	return \@matches;
}

sub help { print <<'END_HELP'; exit(0); }
Usage:
    pler [options] [file/pattern]

Options:
        -V              Print the pler version
        -h, --help      Display this help
        -w              Run test with the -w warnings flag
END_HELP

1;

=pod

=head1 NAME

pler - The DWIM Perl Debugger

=head1 DESCRIPTION

B<pler> is a small script which provides a sanity layer for debugging
test scripts in Perl distributions.

While L<prove> has proven itself to be a highly useful program for
manually running one or more groups of scripts in a distribution,
what we also need is something that provides a similar level of
intelligence in a debugging context.

B<pler> checks that the environment is sound, runs some cleanup tasks
if needed, makes sure you are in the right directory, and then hands off
to the perl debugger as normal.

=head1 TO DO

- Tweak some small terminal related issues on Win32

=head1 SUPPORT

All bugs should be filed via the bug tracker at

L<http://rt.cpan.org/NoAuth/ReportBug.html?Queue=pler>

For other issues, or commercial enhancement and support, contact the author

=head1 AUTHOR

Adam Kennedy E<lt>adamk@cpan.orgE<gt>

=head1 SEE ALSO

L<prove>, L<http://ali.as/>

=head1 COPYRIGHT

Copyright 2006 - 2010 Adam Kennedy.

This program is free software; you can redistribute
it and/or modify it under the same terms as Perl itself.

The full text of the license can be found in the
LICENSE file included with this module.

=cut
