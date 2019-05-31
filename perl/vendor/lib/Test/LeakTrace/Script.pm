package Test::LeakTrace::Script;

use strict;

use Test::LeakTrace ();

my $Mode = $ENV{TEST_LEAKTRACE};

sub import{
	shift;

	$Mode = shift if @_;
}

no warnings 'void';

INIT{
	Test::LeakTrace::_start(1);
}

END{
	$Mode = -simple unless defined $Mode;
	Test::LeakTrace::_finish($Mode);
	return;
}

1;
__END__

=head1 NAME

Test::LeakTrace::Script - A LeakTrace interface for whole scripts

=head1 SYNOPSIS

	#!perl -w
	use Test::LeakTrace::Script sub{
		my($svref, $file, $line) = @_;

		warn "leaked $svref from $file line $line.\n";
	};

=head1 DESCRIPTION

This is a interface to C<Test::LeakTrace> for whole scripts.

=head1 INTERFACE

=head2 Command line interface

	$ perl -MTest::LeakTrace::Script script.pl

	$ perl -MTest::LeakTrace::Script=-verbose script.pl

	$ TEST_LEAKTRACE=-lines script.pl

=head1 ENVIRONMENT VARIABLES

=head2 TEST_LEAKTRACE=mode

=head3 -simple (DEFAULT)

=head3 -sv_dump

=head3 -lines

=head3 -verbose

=head1 SEE ALSO

L<Test::LeakTrace>.

=cut

