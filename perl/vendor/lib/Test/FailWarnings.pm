use 5.008001;
use strict;
use warnings;

package Test::FailWarnings;
# ABSTRACT: Add test failures if warnings are caught
our $VERSION = '0.008'; # VERSION

use Test::More 0.86;
use Cwd qw/getcwd/;
use File::Spec;
use Carp;

our $ALLOW_DEPS = 0;
our @ALLOW_FROM = ();

my $ORIG_DIR = getcwd(); # cache in case handler runs after a chdir

sub import {
    my ( $class, @args ) = @_;
    croak("import arguments must be key/value pairs")
      unless @args % 2 == 0;
    my %opts = @args;
    $ALLOW_DEPS = $opts{'-allow_deps'};
    if ( $opts{'-allow_from'} ) {
        @ALLOW_FROM =
          ref $opts{'-allow_from'} ? @{ $opts{'-allow_from'} } : $opts{'-allow_from'};
    }
    $SIG{__WARN__} = \&handler;
}

sub handler {
    my $msg = shift;
    $msg = '' unless defined $msg;
    chomp $msg;
    my ( $package, $filename, $line ) = _find_source();

    # shortcut if ignoring dependencies and warning did not
    # come from something local
    if ($ALLOW_DEPS) {
        $filename = File::Spec->abs2rel( $filename, $ORIG_DIR )
          if File::Spec->file_name_is_absolute($filename);
        return if $filename !~ /^(?:t|xt|lib|blib)/;
    }

    return if grep { $package eq $_ } @ALLOW_FROM;

    if ( $msg !~ m/at .*? line \d/ ) {
        chomp $msg;
        $msg = "'$msg' at $filename line $line.";
    }
    else {
        $msg = "'$msg'";
    }
    my $builder = Test::More->builder;
    $builder->ok( 0, "Test::FailWarnings should catch no warnings" )
      or $builder->diag("Warning was $msg");
}

sub _find_source {
    my $i = 1;
    while (1) {
        my ( $pkg, $filename, $line ) = caller( $i++ );
        return caller( $i - 2 ) unless defined $pkg;
        next if $pkg =~ /^(?:Carp|warnings)/;
        return ( $pkg, $filename, $line );
    }
}

1;


# vim: ts=4 sts=4 sw=4 et:

__END__

=pod

=encoding utf-8

=head1 NAME

Test::FailWarnings - Add test failures if warnings are caught

=head1 VERSION

version 0.008

=head1 SYNOPSIS

Test file:

    use strict;
    use warnings;
    use Test::More;
    use Test::FailWarnings;

    ok( 1, "first test" );
    ok( 1 + "lkadjaks", "add non-numeric" );

    done_testing;

Output:

    ok 1 - first test
    not ok 2 - Test::FailWarnings should catch no warnings
    #   Failed test 'Test::FailWarnings should catch no warnings'
    #   at t/bin/main-warn.pl line 7.
    # Warning was 'Argument "lkadjaks" isn't numeric in addition (+) at t/bin/main-warn.pl line 7.'
    ok 3 - add non-numeric
    1..3
    # Looks like you failed 1 test of 3.

=head1 DESCRIPTION

This module hooks C<$SIG{__WARN__}> and converts warnings to L<Test::More>
C<fail()> calls.  It is designed to be used with C<done_testing>, when you
don't need to know the test count in advance.

Just as with L<Test::NoWarnings>, this does not catch warnings if other things
localize C<$SIG{__WARN__}>, as this is designed to catch I<unhandled> warnings.

=for Pod::Coverage handler

=head1 USAGE

=head2 Overriding C<$SIG{__WARN__}>

On C<import>, C<$SIG{__WARN__}> is replaced with
C<Test::FailWarnings::handler>.

    use Test::FailWarnings;  # global

If you don't want global replacement, require the module instead and localize
in whatever scope you want.

    require Test::FailWarnings;

    {
        local $SIG{__WARN__} = \&Test::FailWarnings::handler;
        # ... warnings will issue fail() here
    }

When the handler reports on the source of the warning, it will look past
any calling packages starting with C<Carp> or C<warnings> to try to detect
the real origin of the warning.

=head2 Allowing warnings from dependencies

If you want to ignore failures from outside your own code, you can set
C<$Test::FailWarnings::ALLOW_DEPS> to a true value.  You can
do that on the C<use> line with C<< -allow_deps >>.

    use Test::FailWarnings -allow_deps => 1;

When true, warnings will only be thrown if they appear to originate from a filename
matching C<< qr/^(?:t|xt|lib|blib)/ >>

=head2 Allowing warnings from specific modules

If you want to white-list specific modules only, you can add their package
names to C<@Test::NoWarnings::ALLOW_FROM>.  You can do that on the C<use> line
with C<< -allow_from >>.

    use Test::FailWarnings -allow_from => [ qw/Annoying::Module/ ];

=head1 SEE ALSO

=over 4

=item *

L<Test::NoWarnings> -- catches warnings and reports in an C<END> block.  Not (yet) friendly with C<done_testing>.

=item *

L<Test::Warnings> -- a replacement for Test::NoWarnings that works with done_testing

=item *

L<Test::Warn> -- test for warnings without triggering failures from this modules

=back

=for :stopwords cpan testmatrix url annocpan anno bugtracker rt cpants kwalitee diff irc mailto metadata placeholders metacpan

=head1 SUPPORT

=head2 Bugs / Feature Requests

Please report any bugs or feature requests through the issue tracker
at L<https://github.com/dagolden/Test-FailWarnings/issues>.
You will be notified automatically of any progress on your issue.

=head2 Source Code

This is open source software.  The code repository is available for
public review and contribution under the terms of the license.

L<https://github.com/dagolden/Test-FailWarnings>

  git clone https://github.com/dagolden/Test-FailWarnings.git

=head1 AUTHOR

David Golden <dagolden@cpan.org>

=head1 COPYRIGHT AND LICENSE

This software is Copyright (c) 2013 by David Golden.

This is free software, licensed under:

  The Apache License, Version 2.0, January 2004

=cut
