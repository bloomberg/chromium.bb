package Test::SubCalls; # git description: d4e5915

=pod

=head1 NAME

Test::SubCalls - Track the number of times subs are called

=head1 VERSION

version 1.10

=head1 SYNOPSIS

  use Test::SubCalls;
  
  # Start tracking calls to a named sub
  sub_track( 'Foo::foo' );
  
  # Run some test code
  ...
  
  # Test that some sub deep in the codebase was called
  # a specific number of times.
  sub_calls( 'Foo::foo', 5 );
  sub_calls( 'Foo::foo', 5, 'Use a custom test message' );
  
  # Reset the counts for one or all subs
  sub_reset( 'Foo::foo' );
  sub_reset_all();

=head1 DESCRIPTION

There are a number of different situations (like testing caching code)
where you want to want to do a number of tests, and then verify that
some underlying subroutine deep within the code was called a specific
number of times.

This module provides a number of functions for doing testing in this way
in association with your normal L<Test::More> (or similar) test scripts.

=head1 FUNCTIONS

In the nature of test modules, all functions are exported by default.

=cut

use 5.006;
use strict;
use File::Spec    0.80 ();
use Test::More    0.42 ();
use Hook::LexWrap 0.20 ();
use Exporter           ();
use Test::Builder      ();

our $VERSION = '1.10';
use vars qw{@ISA @EXPORT};
BEGIN {
	@ISA     = 'Exporter';
	@EXPORT  = qw{sub_track sub_calls sub_reset sub_reset_all};
}

my $Test = Test::Builder->new;

my %CALLS = ();





#####################################################################
# Test::SubCalls Functions

=pod

=head2 sub_track $subname

The C<sub_track> function creates a new call tracker for a named function.

The sub to track must be provided by name, references to the function
itself are insufficient.

Returns true if added, or dies on error.

=cut

sub sub_track {
	# Check the sub name is valid
	my $subname = shift;
	SCOPE: {
		no strict 'refs';
		unless ( defined *{"$subname"}{CODE} ) {
			die "Test::SubCalls::sub_track : The sub '$subname' does not exist";
		}
		if ( defined $CALLS{$subname} ) {
			die "Test::SubCalls::sub_track : Cannot add duplicate tracker for '$subname'";
		}
	}

	# Initialise the count
	$CALLS{$subname} = 0;

	# Lexwrap the subroutine
	Hook::LexWrap::wrap(
		$subname,
		pre => sub { $CALLS{$subname}++ },
	);

	1;
}

=pod

=head2 sub_calls $subname, $expected_calls [, $message ]

The C<sub_calls> function is the primary (and only) testing function
provided by C<Test::SubCalls>. A single call will represent one test in
your plan.

It takes the subroutine name as originally provided to C<sub_track>,
the expected number of times the subroutine should have been called,
and an optional test message.

If no message is provided, a default message will be provided for you.

Test is ok if the number of times the sub has been called matches the
expected number, or not ok if not.

=cut

sub sub_calls {
	# Check the sub name is valid
	my $subname = shift;
	unless ( defined $CALLS{$subname} ) {
		die "Test::SubCalls::sub_calls : Cannot test untracked sub '$subname'";
	}

	# Check the count
	my $count = shift;
	unless ( $count =~ /^(?:0|[1-9]\d*)\z/s ) {
		die "Test::SubCalls::sub_calls : Expected count '$count' is not an integer";
	}

	# Get the message, applying default if needed
	my $message = shift || "$subname was called $count times";
	$Test->is_num( $CALLS{$subname}, $count, $message );
}

=pod

=head2 sub_reset $subname

To prevent repeat users from having to take before and after counts when
they start testing from after zero, the C<sub_reset> function has been
provided to reset a sub call counter to zero.

Returns true or dies if the sub name is invalid or not currently tracked.

=cut

sub sub_reset {
	# Check the sub name is valid
	my $subname = shift;
	unless ( defined $CALLS{$subname} ) {
		die "Test::SubCalls::sub_reset : Cannot reset untracked sub '$subname'";
	}

	$CALLS{$subname} = 0;

	1;
}

=pod

=head2 sub_reset_all

Provided mainly as a convenience, the C<sub_reset_all> function will reset
all the counters currently defined.

Returns true.

=cut

sub sub_reset_all {
	foreach my $subname ( keys %CALLS ) {
		$CALLS{$subname} = 0;
	}
	1;
}

1;

=pod

=head1 SUPPORT

Bugs should be submitted via the CPAN bug tracker, located at

L<http://rt.cpan.org/NoAuth/ReportBug.html?Queue=Test-SubCalls>

For other issues, or commercial enhancement or support, contact the author.

=head1 AUTHOR

Adam Kennedy E<lt>adamk@cpan.orgE<gt>

=head1 SEE ALSO

L<http://ali.as/>, L<Test::Builder>, L<Test::More>, L<Hook::LexWrap>

=head1 COPYRIGHT

Copyright 2005 - 2009 Adam Kennedy.

This program is free software; you can redistribute
it and/or modify it under the same terms as Perl itself.

The full text of the license can be found in the
LICENSE file included with this module.

=cut
