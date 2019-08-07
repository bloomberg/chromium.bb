package Test::Manifest;
use strict;

use warnings;
no warnings;

use base qw(Exporter);
use vars qw(@EXPORT_OK @EXPORT $VERSION);

use Carp qw(carp);
use File::Spec::Functions qw(catfile);

@EXPORT    = qw(run_t_manifest);
@EXPORT_OK = qw(get_t_files make_test_manifest manifest_name);

$VERSION = '1.23';

my $Manifest = catfile( "t", "test_manifest" );
my %SeenInclude = ();
my %SeenTest = ();

require 5.006;

sub MY::test_via_harness
	{
	my($self, $perl, $tests) = @_;

	return qq|\t$perl "-MTest::Manifest" | .
		   qq|"-e" "run_t_manifest(\$(TEST_VERBOSE), '\$(INST_LIB)', | .
		   qq|'\$(INST_ARCHLIB)', \$(TEST_LEVEL) )"\n|;
	};

=head1 NAME

Test::Manifest - interact with a t/test_manifest file

=head1 SYNOPSIS

	# in Makefile.PL
	eval "use Test::Manifest";

	# in the file t/test_manifest, list the tests you want 
	# to run
	
=head1 DESCRIPTION

C<Test::Harness> assumes that you want to run all of the F<.t> files in the
F<t/> directory in ascii-betical order during C<make test> unless you say
otherwise.  This leads to some interesting naming schemes for test
files to get them in the desired order. This interesting names ossify
when they get into source control, and get even more interesting as
more tests show up.

C<Test::Manifest> overrides the default behaviour by replacing the
test_via_harness target in the Makefile.  Instead of running at the
F<t/*.t> files in ascii-betical order, it looks in the F<t/test_manifest>
file to find out which tests you want to run and the order in which
you want to run them.  It constructs the right value for MakeMaker to
do the right thing.

In F<t/test_manifest>, simply list the tests that you want to run.  Their
order in the file is the order in which they run.  You can comment
lines with a C<#>, just like in Perl, and C<Test::Manifest> will strip
leading and trailing whitespace from each line.  It also checks that
the specified file is actually in the F<t/> directory.  If the file does
not exist, it does not put its name in the list of test files to run and 
it will issue a warning.

Optionally, you can add a number after the test name in test_manifest
to define sets of tests. See C<get_t_files> for more information.

=head2 Functions

=over 4

=item run_t_manifest( TEST_VERBOSE, INST_LIB, INST_ARCHLIB, TEST_LEVEL )

Run all of the files in t/test_manifest through Test::Harness:runtests
in the order they appear in the file.

	eval "use Test::Manifest";

=cut

sub run_t_manifest
	{
	require Test::Harness;
	require File::Spec;

	$Test::Harness::verbose = shift;

	local @INC = @INC;
	unshift @INC, map { File::Spec->rel2abs($_) } @_[0,1];

	my( $level ) = $_[2] || 0;
	
	print STDERR "Test::Manifest $VERSION\n"
		if $Test::Harness::verbose;
		
	print STDERR "Level is $level\n" 
		if $Test::Harness::verbose;
	
	my @files = get_t_files( $level );
	print STDERR "Test::Manifest::test_harness found [@files]\n" 
		if $Test::Harness::verbose;

	Test::Harness::runtests( @files );
	}

=item get_t_files( [LEVEL] )

In scalar context it returns a single string that you can use directly
in WriteMakefile(). In list context it returns a list of the files it
found in t/test_manifest.

If a t/test_manifest file does not exist, get_t_files() returns
nothing.

get_t_files() warns you if it can't find t/test_manifest, or if
entries start with "t/". It skips blank lines, and strips Perl
style comments from the file.

Each line in t/test_manifest can have three parts: the test name,
the test level (a floating point number), and a comment. By default,
the test level is 1.

	test_name.t 2  #Run this only for level 2 testing
	
Without an argument, get_t_files() returns all the test files it
finds. With an argument that is true (so you can't use 0 as a level)
and is a number, it skips tests with a level greater than that
argument. You can then define sets of tests and choose a set to
run. For instance, you might create a set for end users, but also
add on a set for deeper testing for developers.

Experimentally, you can include a command to grab test names from 
another file. The command starts with a C<;> to distinguish it
from a true filename. The filename (currently) is relative to the
current working directory, unlike the filenames, which are relative
to C<t/>. The filenames in the included are still relative to C<t/>.

	;include t/file_with_other_test_names.txt

Also experimentally, you can stop Test::Manifest from reading filenames
with the C<;skip> directive. Test::Harness will skip the filenames up to
the C<;unskip> directive (or end of file)

	run_this1
	;skip
	skip_this
	;unskip
	run_this2

To select sets of tests, specify the level in the variable TEST_LEVEL
during `make test`. 

	make test # run all tests no matter the level
	make test TEST_LEVEL=2  # run all tests level 2 and below

=cut

sub get_t_files
	{
	my $upper_bound = shift;
	print STDERR "# Test level is $upper_bound\n"
		if $Test::Harness::verbose;
	
	%SeenInclude = ();
	%SeenTest    = ();

	carp( "$Manifest does not exist!" ) unless -e $Manifest;
	my $result = _load_test_manifest($Manifest, $upper_bound);
	return unless defined $result;
	my @tests = @{$result};
	
	return wantarray ? @tests : join " ", @tests;
	}

# Wrapper for loading test manifest files to support including other files
sub _load_test_manifest
	{
	my $manifest = shift;
	return unless open my( $fh ), $manifest;

	my $upper_bound = shift || 0;
	my @tests = ();

	LINE: while( <$fh> )
		{
		s/#.*//; s/^\s+//; s/\s+$//;

		next unless $_;

		my( $command, $arg ) = split/\s+/, $_, 2;
		if( ';' eq substr( $command, 0, 1 ) )
			{
			if( $command eq ';include' ) 
				{
				my $result = _include_file( $arg, $., $upper_bound );
				push @tests, @$result if defined $result;
				next;
				}
			elsif( $command eq ';skip' ) 
				{
				while( <$fh> ) { last if m/^;unskip/ }
				next LINE;
				}
			else
				{
				croak( "Unknown directive: $command" );
				}
			}
			
		my( $test, $level ) = ( $command, $arg );
		$level = 1 unless defined $level;
		
		next if( $upper_bound and $level > $upper_bound );
		
		carp( "Bad value for test [$test] level [$level]\n".
			"Level should be a floating-point number\n" )
			unless $level =~ m/^\d+(?:.\d+)?$/;
		carp( "test file begins with t/ [$test]" ) if m|^t/|;
		
		$test = catfile( "t", $test ) if -e catfile( "t", $test );
		
		unless( -e $test )
			{
			carp( "test file [$test] does not exist! Skipping!" );
			next;
			}
			
		# Make sure we don't include a test we've already seen
		next if exists $SeenTest{$test};

		$SeenTest{$test} = 1;
		push @tests, $test;
		}

	close $fh;
	return \@tests;
	}

sub _include_file
	{
	my( $file, $line, $upper_bound ) = @_;
	print STDERR "# Including file $file at line $line\n" 
		if $Test::Harness::verbose;
	
	unless( -e $file )
		{
		carp( "$file does not exist" ) ;
		return;
		}
	
	if( exists $SeenInclude{$file} )
		{
		carp( "$file already loaded - skipping" ) ;
		return;
		}

	$SeenInclude{$file} = $line;

	my $result = _load_test_manifest( $file, $upper_bound );
	return unless defined $result;
	
	$result;
	}
	
	
=item make_test_manifest()

Creates the test_manifest file in the t directory by reading
the contents of the t directory.

TO DO: specify tests in argument lists.

TO DO: specify files to skip.

=cut

sub make_test_manifest()
	{
	carp( "t/ directory does not exist!" ) unless -d "t";
	return unless open my( $fh ), "> $Manifest";

	my $count = 0;
	while( my $file = glob("t/*.t") )
		{
		$file =~ s|^t/||;
		print $fh "$file\n";
		$count++;
		}
	close $fh;

	return $count;
	}

=item manifest_name()

Returns the name of the test manifest file, relative to t/

=cut

sub manifest_name
	{
	return $Manifest;
	}

=back

=head1 SOURCE AVAILABILITY

This source is in Github:

	http://github.com/briandfoy/Test-Manifest/tree/master

=head1 CREDITS

Matt Vanderpol suggested and supplied a patch for the ;include
feature.

=head1 AUTHOR

brian d foy, C<< <bdfoy@cpan.org> >>

=head1 COPYRIGHT AND LICENSE

Copyright (c) 2002-2009 brian d foy.  All rights reserved.

This program is free software; you can redistribute it and/or modify
it under the same terms as Perl itself.

=cut


1;
