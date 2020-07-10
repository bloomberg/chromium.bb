package Test::Fork;

use strict;
use warnings;

our $VERSION = '0.02';

use base 'Test::Builder::Module';
our @EXPORT = qw(fork_ok);

my $CLASS = __PACKAGE__;


sub note {
    my $msg = shift;
    my $fh = $CLASS->builder->output;

    print $fh "# $msg\n";
}


=head1 NAME

Test::Fork - test code which forks

=head1 SYNOPSIS

    use Test::More tests => 4;
    use Test::Fork;
    
    fork_ok(2, sub{
        pass("Test in the child process");
        pass("Another test in the child process");
    });
    
    pass("Test in the parent");

=head1 DESCRIPTION

B<THIS IS ALPHA CODE!>  The implementation is unreliable and the interface
is subject to change.

Because each test has a number associated with it, testing code which forks
is problematic.  Coordinating the test number amongst the parent and child
processes is complicated.  Test::Fork provides a function to smooth over
the complications.

=head2 Functions

Each function is exported by default.

=head3 B<fork_ok>

    my $child_pid = fork_ok( $num_tests, sub {
        ...child test code...
    });

Runs the given child test code in a forked process.  Returns the pid of the
forked child process, or false if the fork fails.

$num_tests is the number of tests in your child test code.
Consider it to be a sub-plan.

fork_ok() itself is a test, if the fork fails it will fail.  fork_ok()
test does not count towards your $num_tests.

    # This is three tests.
    fork_ok( 2, sub {
        is $foo, $bar;
        ok Something->method;
    });

The children are automatically reaped.

=cut

my %Reaped;
my %Running_Children;
my $Is_Child = 0;

sub fork_ok ($&) {
    my($num_tests, $child_sub) = @_;
    
    my $tb = $CLASS->builder;    
    my $pid = fork;

    # Failed fork
    if( !defined $pid ) {
        return $tb->ok(0, "fork() failed: $!");
    }
    # Parent
    elsif( $pid ) {
        # Avoid race condition where child has run and is reaped before
        # parent even runs.
        $Running_Children{$pid} = 1 unless $Reaped{$pid};

        $tb->use_numbers(0);
        $tb->current_test($tb->current_test + $num_tests);

        $tb->ok(1, "fork() succeeded, child pid $pid");
        return $pid;
    }

    # Child
    $Is_Child = 1;

    $tb->use_numbers(0);
    $tb->no_ending(1);
    
    note("Running child pid $$");
    $child_sub->();
    exit;
}

END {
    while( !$Is_Child and keys %Running_Children ) {
        note("reaper($$) waiting on @{[keys %Running_Children]}");
        _check_kids();
        _reaper();
    }
}

sub _check_kids {
    for my $child (keys %Running_Children) {
        delete $Running_Children{$child} if $Reaped{$child};
        delete $Running_Children{$child} unless kill 0, $child;
        note("Child $child already reaped");
    }
}

sub _reaper {
    local $?;  # wait sets $?

    my $child_pid = wait;
    $Reaped{$child_pid}++;
    delete $Running_Children{$child_pid};

    note("child $child_pid reaped");

    $CLASS->builder->use_numbers(1) unless keys %Running_Children;

    return $child_pid == -1 ? 0 : 1;
}

$SIG{CHLD} = \&_reaper;


=head1 CAVEATS

The failure of tests in a child process cannot be detected by the parent.
Therefore, the normal end-of-test reporting done by Test::Builder will
not notice failed child tests.

Test::Fork turns off test numbering in order to avoid test counter
coordination issues.  It turns it back on once the children are done
running.

Test::Fork will wait for all your child processes to complete at the end of
the parent process.

=head1 SEE ALSO

L<Test::MultiFork>


=head1 AUTHOR

Michael G Schwern E<lt>schwern@pobox.comE<gt>


=head1 BUGS and FEEDBACK

Please send all bugs and feature requests to 
I<bug-Test-Fork> at I<rt.cpan.org> or use the web interface via
L<http://rt.cpan.org>.

If you use it, please send feedback.  I like getting feedback.


=head1 COPYRIGHT and LICENSE

Copyright 2007-2008 by Michael G Schwern E<lt>schwern@pobox.comE<gt>.

This program is free software; you can redistribute it and/or
modify it under the same terms as Perl itself.

See F<http://www.perl.com/perl/misc/Artistic.html>

=cut

42;
