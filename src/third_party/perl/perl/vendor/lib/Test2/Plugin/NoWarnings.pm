package Test2::Plugin::NoWarnings;

use strict;
use warnings;

our $VERSION = '0.07';

use Test2 1.302096;
use Test2::API qw( context_do );
use Test2::Event::Warning;

my $echo = 0;

sub import {
    shift;
    my %args = @_;
    $echo = $args{echo} if exists $args{echo};
    return;
}

my $_orig_warn_handler = $SIG{__WARN__};
## no critic (Variables::RequireLocalizedPunctuationVars)
$SIG{__WARN__} = sub {
    my $w = $_[0];
    $w =~ s/\n+$//g;

    context_do {
        my $ctx = shift;
        $ctx->send_event(
            'Warning',
            warning => "Unexpected warning: $w",
        );
    }
    $_[0];

    return unless $echo;

    return if $_orig_warn_handler && $_orig_warn_handler eq 'IGNORE';

    # The rest was copied from Test::Warnings

    # TODO: this doesn't handle blessed coderefs... does anyone care?
    goto &$_orig_warn_handler
        if $_orig_warn_handler
        and (
        ( ref $_orig_warn_handler eq 'CODE' )
        or (    $_orig_warn_handler ne 'DEFAULT'
            and $_orig_warn_handler ne 'IGNORE'
            and defined &$_orig_warn_handler )
        );

    if ( $_[0] =~ /\n$/ ) {
        warn $_[0];
    }
    else {
        require Carp;
        Carp::carp( $_[0] );
    }
};

1;

# ABSTRACT: Fail if tests warn

__END__

=pod

=encoding UTF-8

=head1 NAME

Test2::Plugin::NoWarnings - Fail if tests warn

=head1 VERSION

version 0.07

=head1 SYNOPSIS

    use Test2::V0;
    use Test2::Plugin::NoWarnings;

    ...;

=head1 DESCRIPTION

Loading this plugin causes your tests to fail if there any warnings while they
run. Each warning generates a new failing test and the warning content is
outputted via C<diag>.

This module uses C<$SIG{__WARN__}>, so if the code you're testing sets this,
then this module will stop working.

=head1 ECHOING WARNINGS

By default, this module suppresses the warning itself so it does not go to
C<STDERR>. If you'd like to also have the warning go to C<STDERR> untouched,
you can ask for this with the C<echo> import argument:

    use Test2::Plugin::NoWarnings echo => 1;

=head1 SUPPORT

Bugs may be submitted at L<http://rt.cpan.org/Public/Dist/Display.html?Name=Test2-Plugin-NoWarnings> or via email to L<bug-test2-plugin-nowarnings@rt.cpan.org|mailto:bug-test2-plugin-nowarnings@rt.cpan.org>.

I am also usually active on IRC as 'autarch' on C<irc://irc.perl.org>.

=head1 SOURCE

The source code repository for Test2-Plugin-NoWarnings can be found at L<https://github.com/houseabsolute/Test2-Plugin-NoWarnings>.

=head1 DONATIONS

If you'd like to thank me for the work I've done on this module, please
consider making a "donation" to me via PayPal. I spend a lot of free time
creating free software, and would appreciate any support you'd care to offer.

Please note that B<I am not suggesting that you must do this> in order for me
to continue working on this particular software. I will continue to do so,
inasmuch as I have in the past, for as long as it interests me.

Similarly, a donation made in this way will probably not make me work on this
software much more, unless I get so many donations that I can consider working
on free software full time (let's all have a chuckle at that together).

To donate, log into PayPal and send money to autarch@urth.org, or use the
button at L<http://www.urth.org/~autarch/fs-donation.html>.

=head1 AUTHOR

Dave Rolsky <autarch@urth.org>

=head1 COPYRIGHT AND LICENSE

This software is Copyright (c) 2019 by Dave Rolsky.

This is free software, licensed under:

  The Artistic License 2.0 (GPL Compatible)

The full text of the license can be found in the
F<LICENSE> file included with this distribution.

=cut
