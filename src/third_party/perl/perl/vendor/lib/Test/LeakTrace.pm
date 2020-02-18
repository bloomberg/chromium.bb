package Test::LeakTrace;

use 5.008_001;
use strict;
use warnings;

our $VERSION = '0.16';

use XSLoader;
XSLoader::load(__PACKAGE__, $VERSION);

use Test::Builder::Module;
our @ISA = qw(Test::Builder::Module);

use Exporter qw(import); # use Exporter::import for backward compatibility
our @EXPORT = qw(
    leaktrace leaked_refs leaked_info leaked_count
    no_leaks_ok leaks_cmp_ok
    count_sv
);

our %EXPORT_TAGS = (
    all  => \@EXPORT,
    test => [qw(no_leaks_ok leaks_cmp_ok)],
    util => [qw(leaktrace leaked_refs leaked_info leaked_count count_sv)],
);


sub _do_leaktrace{
    my($block, $name, $need_stateinfo, $mode) = @_;

    if(!defined($mode) && !defined wantarray){
        warnings::warnif void => "Useless use of $name() in void context";
    }

    if($name eq 'leaked_count') {
        my $start;
        $start = count_sv();
        $block->();
        return count_sv() - $start;
    }

    local $SIG{__DIE__} = 'DEFAULT';

    _start($need_stateinfo);
    eval{
        $block->();
    };
    if($@){
        _finish(-silent);
        die $@;
    }

    return _finish($mode);
}

sub leaked_refs(&){
    my($block) = @_;
    return _do_leaktrace($block, 'leaked_refs', 0);
}

sub leaked_info(&){
    my($block) = @_;
    return _do_leaktrace($block, 'leaked_refs', 1);
}

sub leaked_count(&){
    my($block) = @_;
    return scalar _do_leaktrace($block, 'leaked_count', 0);
}

sub leaktrace(&;$){
    my($block, $mode) = @_;
    _do_leaktrace($block, 'leaktrace', 1, defined($mode) ? $mode : -simple);
    return;
}


sub leaks_cmp_ok(&$$;$){
    my($block, $cmp_op, $expected, $description) = @_;

    my $Test = __PACKAGE__->builder;

    if(!_runops_installed()){
        my $mod = exists $INC{'Devel/Cover.pm'} ? 'Devel::Cover' : 'strange runops routines';
        return $Test->ok(1, "skipped (under $mod)");
    }

    # calls to prepare cache in $block
    $block->();

    my $got = _do_leaktrace($block, 'leaked_count', 0);

    my $desc = sprintf 'leaks %s %-2s %s', $got, $cmp_op, $expected;
    if(defined $description){
        $description .= " ($desc)";
    }
    else{
        $description = $desc;
    }

    my $result = $Test->cmp_ok($got, $cmp_op, $expected, $description);

    if(!$result){
        open local(*STDERR), '>', \(my $content = '');
        $block->(); # calls it again because opening *STDERR changes the run-time environment

        _do_leaktrace($block, 'leaktrace', 1, -verbose);
        $Test->diag($content);
    }

    return $result;
}

sub no_leaks_ok(&;$){
    # ($block, $description)
    splice @_, 1, 0, ('<=', 0); # ($block, '<=', 0, $description);
    goto &leaks_cmp_ok;
}


1;
__END__

=for stopwords sv gfx

=head1 NAME

Test::LeakTrace - Traces memory leaks

=head1 VERSION

This document describes Test::LeakTrace version 0.15.

=head1 SYNOPSIS

    use Test::LeakTrace;

    # simple report
    leaktrace{
        # ...
    };

    # verbose output
    leaktrace{
        # ...
    } -verbose;

    # with callback
    leaktrace{
        # ...
    } sub {
        my($ref, $file, $line) = @_;
        warn "leaked $ref from $file line\n";
    };

    my @refs = leaked_refs{
        # ...
    };
    my @info = leaked_info{
        # ...
    };

    my $count = leaked_count{
        # ...
    };

    # standard test interface
    use Test::LeakTrace;

    no_leaks_ok{
        # ...
    } 'no memory leaks';

    leaks_cmp_ok{
        # ...
    } '<', 10;

=head1 DESCRIPTION

C<Test::LeakTrace> provides several functions that trace memory leaks.
This module scans arenas, the memory allocation system,
so it can detect any leaked SVs in given blocks.

B<Leaked SVs> are SVs which are not released after the end of the scope
they have been created. These SVs include global variables and internal caches.
For example, if you call a method in a tracing block, perl might prepare a cache
for the method. Thus, to trace true leaks, C<no_leaks_ok()> and C<leaks_cmp_ok()>
executes a block more than once.

=head1 INTERFACE

=head2 Exported functions

=head3 C<< leaked_info { BLOCK } >>

Executes I<BLOCK> and returns a list of leaked SVs and places where the SVs
come from, i.e. C<< [$ref, $file, $line] >>.

=head3 C<< leaked_refs { BLOCK } >>

Executes I<BLOCK> and returns a list of leaked SVs.

=head3 C<< leaked_count { BLOCK } >>

Executes I<BLOCK> and returns the number of leaked SVs.

=head3 C<< leaktrace { BLOCK } ?($mode | \&callback) >>

Executes I<BLOCK> and reports leaked SVs to C<*STDERR>.

Defined I<$mode>s are:

=over 4

=item -simple

Default. Reports the leaked SV identity (type and address), file name and line number.

=item -sv_dump

In addition to B<-simple>, dumps the sv content using C<sv_dump()>,
which also implements C<Devel::Peek::Dump()>.

=item -lines

In addition to B<-simple>, prints suspicious source lines.

=item -verbose

Both B<-sv_dump> and B<-lines>.

=back

=head3 C<< no_leaks_ok { BLOCK } ?$description >>

Tests that I<BLOCK> does not leaks SVs. This is a test function
using C<Test::Builder>.

Note that I<BLOCK> is called more than once. This is because
I<BLOCK> might prepare caches which are not memory leaks.

=head3 C<< leaks_cmp_ok { BLOCK } $cmp_op, $number, ?$description >>

Tests that I<BLOCK> leaks a specific number of SVs. This is a test
function using C<Test::Builder>.

Note that I<BLOCK> is called more than once. This is because
I<BLOCK> might prepare caches which are not memory leaks.

=head3 C<< count_sv() >>

Counts all the SVs in the arena.

=head2 Script interface

Like C<Devel::LeakTrace> C<Test::LeakTrace::Script> is provided for whole scripts.

The arguments of C<use Test::LeakTrace::Script> directive is the same as C<leaktrace()>.

    $ TEST_LEAKTRACE=-sv_dump perl -MTest::LeakTrace::Script script.pl
    $ perl -MTest::LeakTrace::Script=-verbose script.pl

    #!perl
    # ...

    use Test::LeakTrace::Script sub{
        my($ref, $file, $line) = @_;
        # ...
    };

    # ...

=head1 EXAMPLES

=head2 Testing modules

Here is a test script template that checks memory leaks.

    #!perl -w
    use strict;
    use constant HAS_LEAKTRACE => eval{ require Test::LeakTrace };
    use Test::More HAS_LEAKTRACE ? (tests => 1) : (skip_all => 'require Test::LeakTrace');
    use Test::LeakTrace;

    use Some::Module;

    leaks_cmp_ok{
        my $o = Some::Module->new();
        $o->something();
        $o->something_else();
    } '<', 1;

=head1 DEPENDENCIES

Perl 5.8.1 or later, and a C compiler.

=head1 CAVEATS

C<Test::LeakTrace> does not work with C<Devel::Cover> and modules which install
their own C<runops> routines, or the perl executor. So if the test functions of
this module detect strange C<runops> routines, they do nothing and report okay.

=head1 BUGS

No bugs have been reported.

Please report any bugs or feature requests to the author.

=head1 SEE ALSO

L<Devel::LeakTrace>.

L<Devel::LeakTrace::Fast>.

L<Test::TraceObject>.

L<Test::Weak>.

For guts:

L<perlguts>.

L<perlhack>.

F<sv.c>.

=head1 AUTHOR

Goro Fuji(gfx) E<lt>gfuji(at)cpan.orgE<gt>.

=head1 LICENSE AND COPYRIGHT

Copyright (c) 2009-2010, Goro Fuji(gfx). All rights reserved.

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself.

=cut
