=head1 NAME

Test::Warn - Perl extension to test methods for warnings

=head1 SYNOPSIS

  use Test::Warn;

  warning_is    {foo(-dri => "/")} "Unknown Parameter 'dri'", "dri != dir gives warning";
  warnings_are  {bar(1,1)} ["Width very small", "Height very small"];

  warning_is    {add(2,2)} undef, "No warnings for calc 2+2"; # or
  warnings_are  {add(2,2)} [],    "No warnings for calc 2+2"; # whichever reads better :-)

  warning_like  {foo(-dri => "/")} qr/unknown param/i, "an unknown parameter test";
  warnings_like {bar(1,1)} [qr/width.*small/i, qr/height.*small/i];

  warning_is    {foo()} {carped => "didn't find the right parameters"};
  warnings_like {foo()} [qr/undefined/,qr/undefined/,{carped => qr/no result/i}];

  warning_like {foo(undef)}                 'uninitialized';
  warning_like {bar(file => '/etc/passwd')} 'io';

  warning_like {eval q/"$x"; $x;/}
               [qw/void uninitialized/],
               "some warnings at compile time";

  warnings_exist {...} [qr/expected warning/], "Expected warning is thrown";

=head1 DESCRIPTION

A good style of Perl programming calls for a lot of diverse regression tests.

This module provides a few convenience methods for testing warning based-code.

If you are not already familiar with the L<Test::More> manpage
now would be the time to go take a look.

=head2 FUNCTIONS

=over 4

=item B<warning_is> I<BLOCK STRING, TEST_NAME>

Tests that BLOCK gives the specified warning exactly once.

The test fails if the BLOCK warns more than once or does not warn at all.
If the string is undef, then the test succeeds if the BLOCK doesn't
give any warning.

Another way to say that there are no warnings in the block
is:

  warnings_are {foo()} [], "no warnings"

If you want to test for a warning given by Carp
you have to write something like:

  warning_is {carp "msg"} {carped => 'msg'}, "Test for a carped warning";

The test will fail if a "normal" warning is found instead of a "carped" one.

Note: C<warn "foo"> would print something like C<foo at -e line 1>.
This method ignores everything after the "at". Thus to match this warning
you would have to call C<< warning_is {warn "foo"} "foo", "Foo succeeded" >>.
If you need to test for a warning at an exact line,
try something like:

  warning_like {warn "foo"} qr/at XYZ.dat line 5/

Warn messages with a trailing newline (like C<warn "foo\n">) don't produce the C<at -e line 1> message by Perl.
Up to Test::Warn 0.30 such warning weren't supported by C<< warning_is {warn "foo\n"} "foo\n" >>.
Starting with version 0.31 they are supported, but also marked as experimental.

L<C<warning_is()>|/warning_is-BLOCK-STRING-TEST_NAME> and L<C<warnings_are()>|/warnings_are-BLOCK-ARRAYREF-TEST_NAME>
are only aliases to the same method.  So you also could write
C<< warning_is {foo()} [], "no warning" >> or something similar.

I decided to give two methods the same name to improve readability.

A true value is returned if the test succeeds, false otherwise.

The test name is optional, but recommended.

=item B<warnings_are> I<BLOCK ARRAYREF, TEST_NAME>

Tests to see that BLOCK gives exactly the specified warnings.
The test fails if the warnings from BLOCK are not exactly the ones in ARRAYREF.
If the ARRAYREF is equal to C<< [] >>,
then the test succeeds if the BLOCK doesn't give any warning.

Please read also the notes to
L<C<warning_is()>|/warning_is-BLOCK-STRING-TEST_NAME>
as these methods are only aliases.

If you want more than one test for carped warnings, try this:

  warnings_are {carp "c1"; carp "c2"} {carped => ['c1','c2'];

or

  warnings_are {foo()} ["Warning 1", {carped => ["Carp 1", "Carp 2"]}, "Warning 2"];

Note that C<< {carped => ...} >> must always be a hash ref.

=item B<warning_like> I<BLOCK REGEXP, TEST_NAME>

Tests that BLOCK gives exactly one warning and it can be matched by
the given regexp.

If the string is undef, then the tests succeeds if the BLOCK doesn't
give any warning.

The REGEXP is matched against the whole warning line,
which in general has the form C<< "WARNING at __FILE__ line __LINE__" >>.
So you can check for a warning in the file C<Foo.pm> on line 5 with:

  warning_like {bar()} qr/at Foo.pm line 5/, "Testname"

I don't know whether it makes sense to do such a test :-(

However, you should be prepared as a matching with C<'at'>, C<'file'>, C<'\d'>
or similar will always pass.

Consider C<< qr/^foo/ >> if you want to test for warning C<"foo something"> in file F<foo.pl>.

You can also write the regexp in a string as C<"/.../">
instead of using the C<< qr/.../ >> syntax.

Note that the slashes are important in the string,
as strings without slashes are reserved for warning categories
(to match warning categories as can be seen in the perllexwarn man page).

Similar to
L<< C<warning_is()>|/warning_is-BLOCK-STRING-TEST_NAME >> and
L<< C<warnings_are()>|/warnings_are-BLOCK-ARRAYREF-TEST_NAME >>
you can test for warnings via C<carp> with:

  warning_like {bar()} {carped => qr/bar called too early/i};

Similar to
L<< C<warning_is()>|/warning_is-BLOCK-STRING-TEST_NAME >> and
L<< C<warnings_are()>|/warnings_are-BLOCK-ARRAYREF-TEST_NAME >>,

L<< C<warning_like()>|/warning_like-BLOCK-REGEXP-TEST_NAME >> and
L<< C<warnings_like()>|/warnings_like-BLOCK-ARRAYREF-TEST_NAME >>
are only aliases to the same methods.

A true value is returned if the test succeeds, false otherwise.

The test name is optional, but recommended.

=item B<warning_like> I<BLOCK STRING, TEST_NAME>

Tests whether a BLOCK gives exactly one warning of the passed category.

The categories are grouped in a tree,
like it is expressed in L<perllexwarn>.
Also see L</BUGS AND LIMITATIONS>.


Thanks to the grouping in a tree,
it's possible to test simply for an 'io' warning,
instead of testing for a 'closed|exec|layer|newline|pipe|unopened' warning.

Note, that warnings occurring at compile time
can only be caught in an eval block. So

  warning_like {eval q/"$x"; $x;/}
                [qw/void uninitialized/],
                "some warnings at compile time";

will work, while it wouldn't work without the eval.

Note, that it isn't possible yet,
to test for own categories,
created with L<warnings::register>.

=item B<warnings_like> I<BLOCK ARRAYREF, TEST_NAME>

Tests to see that BLOCK gives exactly the number of the specified
warnings, in the defined order.

Please read also the notes to
L<< C<warning_like()>|/warning_like-BLOCK-REGEXP-TEST_NAME >>
as these methods are only aliases.

Similar to
L<< C<warnings_are()>|/warnings_are-BLOCK-ARRAYREF-TEST_NAME >>,
you can test for multiple warnings via C<carp>
and for warning categories, too:

  warnings_like {foo()}
                [qr/bar warning/,
                 qr/bar warning/,
                 {carped => qr/bar warning/i},
                 'io'
                ],
                "I hope you'll never have to write a test for so many warnings :-)";

=item B<warnings_exist> I<BLOCK STRING|ARRAYREF, TEST_NAME>

Same as warning_like, but will C<< warn() >> all warnings that do not match the supplied regex/category,
instead of registering an error. Use this test when you just want to make sure that specific
warnings were generated, and couldn't care less if other warnings happened in the same block
of code.

  warnings_exist {...} [qr/expected warning/], "Expected warning is thrown";

  warnings_exist {...} ['uninitialized'], "Expected warning is thrown";

=back

=head2 EXPORT

C<warning_is>,
C<warnings_are>,
C<warning_like>,
C<warnings_like>,
C<warnings_exist> by default.

=head1 BUGS AND LIMITATIONS

Category check is done as C<< qr/category_name/ >>. In some case this works, like for
category C<'uninitialized'>. For C<'utf8'> it does not work. Perl does not have a list
of warnings, so it is not possible to generate one for C<Test::Warn>.

If you want to add a warning to a category, send a pull request. Modifications
should be done to C<< %warnings_in_category >>. You should look into perl source to check
how warning is looking exactly.

Please note that warnings with newlines inside are very awkward.
The only sensible way to handle them is to use the C<warning_like> or
C<warnings_like> methods. The background is that there is no
really safe way to distinguish between warnings with newlines and a
stacktrace.

If a method has its own warn handler,
overwriting C<$SIG{__WARN__}>,
my test warning methods won't get these warnings.

The C<warning_like BLOCK CATEGORY, TEST_NAME> method isn't fully tested.
Please take note if you use this this calling style,
and report any bugs you find.

=head2 XS warnings

As described in https://rt.cpan.org/Ticket/Display.html?id=42070&results=3c71d1b101a730e185691657f3b02f21 or https://github.com/hanfried/test-warn/issues/1 XS warnings might not be caught.

=head1 SEE ALSO

Have a look to the similar L<Test::Exception> module. L<Test::Trap>

=head1 THANKS

Many thanks to Adrian Howard, chromatic and Michael G. Schwern,
who have given me a lot of ideas.

=head1 AUTHOR

Janek Schleicher, E<lt>bigj AT kamelfreund.deE<gt>

=head1 COPYRIGHT AND LICENSE

Copyright 2002 by Janek Schleicher

Copyright 2007-2014 by Alexandr Ciornii, L<http://chorny.net/>

Copyright 2015-2018 by Janek Schleicher

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself.

=cut


package Test::Warn;

use 5.006;
use strict;
use warnings;

use Sub::Uplevel 0.12;

our $VERSION = '0.36';

require Exporter;

our @ISA = qw(Exporter);

our %EXPORT_TAGS = ( 'all' => [ qw(
    @EXPORT	
) ] );

our @EXPORT_OK = ( @{ $EXPORT_TAGS{'all'} } );

our @EXPORT = qw(
    warning_is   warnings_are
    warning_like warnings_like
    warnings_exist
);

use Test::Builder;
my $Tester = Test::Builder->new;

{
no warnings 'once';
*warning_is = *warnings_are;
*warning_like = *warnings_like;
}

sub warnings_are (&$;$) {
    my $block       = shift;
    my @exp_warning = map {_canonical_exp_warning($_)}
                          _to_array_if_necessary( shift() || [] );
    my $testname    = shift;
    my @got_warning = ();
    local $SIG{__WARN__} = sub {
        my ($called_from) = caller(0);  # to find out Carping methods
        push @got_warning, _canonical_got_warning($called_from, shift());
    };
    uplevel 1,$block;
    my $ok = _cmp_is( \@got_warning, \@exp_warning );
    $Tester->ok( $ok, $testname );
    $ok or _diag_found_warning(@got_warning),
           _diag_exp_warning(@exp_warning);
    return $ok;
}


sub warnings_like (&$;$) {
    my $block       = shift;
    my @exp_warning = map {_canonical_exp_warning($_)}
                          _to_array_if_necessary( shift() || [] );
    my $testname    = shift;
    my @got_warning = ();
    local $SIG{__WARN__} = sub {
        my ($called_from) = caller(0);  # to find out Carping methods
        push @got_warning, _canonical_got_warning($called_from, shift());
    };
    uplevel 1,$block;
    my $ok = _cmp_like( \@got_warning, \@exp_warning );
    $Tester->ok( $ok, $testname );
    $ok or _diag_found_warning(@got_warning),
           _diag_exp_warning(@exp_warning);
    return $ok;
}

sub warnings_exist (&$;$) {
    my $block       = shift;
    my @exp_warning = map {_canonical_exp_warning($_)}
                          _to_array_if_necessary( shift() || [] );
    my $testname    = shift;
    my @got_warning = ();
    local $SIG{__WARN__} = sub {
        my ($called_from) = caller(0);  # to find out Carping methods
        my $wrn_text=shift;
        my $wrn_rec=_canonical_got_warning($called_from, $wrn_text);
        foreach my $wrn (@exp_warning) {
          if (_cmp_got_to_exp_warning_like($wrn_rec,$wrn)) {
            push @got_warning, $wrn_rec;
            return;
          }
        }
        warn $wrn_text;
    };
    uplevel 1,$block;
    my $ok = _cmp_like( \@got_warning, \@exp_warning );
    $Tester->ok( $ok, $testname );
    $ok or _diag_found_warning(@got_warning),
           _diag_exp_warning(@exp_warning);
    return $ok;
}


sub _to_array_if_necessary {
    return (ref($_[0]) eq 'ARRAY') ? @{$_[0]} : ($_[0]);
}

sub _canonical_got_warning {
    my ($called_from, $msg) = @_;
    my $warn_kind = $called_from eq 'Carp' ? 'carped' : 'warn';
    my @warning_stack = split /\n/, $msg;     # some stuff of uplevel is included
    return {$warn_kind => $warning_stack[0]}; # return only the real message
}

sub _canonical_exp_warning {
    my ($exp) = @_;
    if (ref($exp) eq 'HASH') {             # could be {carped => ...}
        my $to_carp = $exp->{carped} or return; # undefined message are ignored
        return (ref($to_carp) eq 'ARRAY')  # is {carped => [ ..., ...] }
            ? map({ {carped => $_} } grep {defined $_} @$to_carp)
            : +{carped => $to_carp};
    }
    return {warn => $exp};
}

sub _cmp_got_to_exp_warning {
    my ($got_kind, $got_msg) = %{ shift() };
    my ($exp_kind, $exp_msg) = %{ shift() };
    return 0 if ($got_kind eq 'warn') && ($exp_kind eq 'carped');
    my $cmp;
    if ($exp_msg =~ /\n$/s) {
      $cmp = "$got_msg\n" eq $exp_msg;
    } else {
      $cmp = $got_msg =~ /^\Q$exp_msg\E at .+ line \d+\.?$/s;
    }
    return $cmp;
}

sub _cmp_got_to_exp_warning_like {
    my ($got_kind, $got_msg) = %{ shift() };
    my ($exp_kind, $exp_msg) = %{ shift() };
    return 0 if ($got_kind eq 'warn') && ($exp_kind eq 'carped');
    if (my $re = $Tester->maybe_regex($exp_msg)) { #qr// or '//'
        my $cmp = $got_msg =~ /$re/;
        return $cmp;
    } else {
        return Test::Warn::Categorization::warning_like_category($got_msg,$exp_msg);
    }
}


sub _cmp_is {
    my @got  = @{ shift() };
    my @exp  = @{ shift() };
    scalar @got == scalar @exp or return 0;
    my $cmp = 1;
    $cmp &&= _cmp_got_to_exp_warning($got[$_],$exp[$_]) for (0 .. $#got);
    return $cmp;
}

sub _cmp_like {
    my @got  = @{ shift() };
    my @exp  = @{ shift() };
    scalar @got == scalar @exp or return 0;
    my $cmp = 1;
    $cmp &&= _cmp_got_to_exp_warning_like($got[$_],$exp[$_]) for (0 .. $#got);
    return $cmp;
}

sub _diag_found_warning {
    foreach (@_) {
        if (ref($_) eq 'HASH') {
            ${$_}{carped} ? $Tester->diag("found carped warning: ${$_}{carped}")
                          : $Tester->diag("found warning: ${$_}{warn}");
        } else {
            $Tester->diag( "found warning: $_" );
        }
    }
    $Tester->diag( "didn't find a warning" ) unless @_;
}

sub _diag_exp_warning {
    foreach (@_) {
        if (ref($_) eq 'HASH') {
            ${$_}{carped} ? $Tester->diag("expected to find carped warning: ${$_}{carped}")
                          : $Tester->diag("expected to find warning: ${$_}{warn}");
        } else {
            $Tester->diag( "expected to find warning: $_" );
        }
    }
    $Tester->diag( "didn't expect to find a warning" ) unless @_;
}

package Test::Warn::Categorization;

use Carp;

my $bits = \%warnings::Bits;
my @warnings = sort grep {
  my $warn_bits = $bits->{$_};
  #!grep { $_ ne $warn_bits && ($_ & $warn_bits) eq $_ } values %$bits;
} keys %$bits;

# Create a warning name category (e.g. 'utf8') to map to a list of warnings.
# The warnings are strings that will be OR'ed together into a
# regular expression: qr/...|...|.../.
my %warnings_in_category = (
  'utf8' => ['Wide character in \w+\b',],
);

sub _warning_category_regexp {
    my $category = shift;
    my $category_bits = $bits->{$category} or return;
    my @category_warnings
      = grep { ($bits->{$_} & $category_bits) eq $bits->{$_} } @warnings;

    my @list =
      map { exists $warnings_in_category{$_}? (@{ $warnings_in_category{$_}}) : ($_) }
      @category_warnings;
    my $re = join "|", @list;
    return qr/$re/;
}

sub warning_like_category {
    my ($warning, $category) = @_;
    my $re = _warning_category_regexp($category) or
        carp("Unknown warning category '$category'"),return;
    my $ok = $warning =~ /$re/;
    return $ok;
}

1;
