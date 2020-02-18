package Package::DeprecationManager;

use strict;
use warnings;

our $VERSION = '0.17';

use Carp qw( croak );
use List::Util 1.33 qw( any );
use Package::Stash;
use Params::Util qw( _HASH0 );
use Sub::Install;
use Sub::Name qw( subname );

sub import {
    shift;
    my %args = @_;

    croak
        'You must provide a hash reference -deprecations parameter when importing Package::DeprecationManager'
        unless $args{-deprecations} && _HASH0( $args{-deprecations} );

    my %registry;

    my $caller = caller();

    my $orig_import = $caller->can('import');

    my $import = _build_import( \%registry, $orig_import );
    my $warn
        = _build_warn( \%registry, $args{-deprecations}, $args{-ignore} );

    # We need to remove this to prevent a 'subroutine redefined' warning.
    if ($orig_import) {
        Package::Stash->new($caller)->remove_symbol('&import');
    }

    Sub::Install::install_sub(
        {
            code => subname( $caller . '::import', $import ),
            into => $caller,
            as   => 'import',
        }
    );

    Sub::Install::install_sub(
        {
            code => subname( $caller . '::deprecated', $warn ),
            into => $caller,
            as   => 'deprecated',
        }
    );

    return;
}

sub _build_import {
    my $registry    = shift;
    my $orig_import = shift;

    return sub {
        my $class = shift;

        my @args;

        my $api_version;
        ## no critic (ControlStructures::ProhibitCStyleForLoops)
        for ( my $i = 0; $i < @_; $i++ ) {
            if ( $_[$i] eq '-api_version' || $_[$i] eq '-compatible' ) {
                $api_version = $_[ ++$i ];
            }
            else {
                push @args, $_[$i];
            }
        }
        ## use critic

        my $caller = caller();
        $registry->{$caller} = $api_version
            if defined $api_version;

        if ($orig_import) {
            @_ = ( $class, @args );
            goto &{$orig_import};
        }

        return;
    };
}

sub _build_warn {
    my $registry      = shift;
    my $deprecated_at = shift;
    my $ignore        = shift;

    my %ignore = map { $_ => 1 } grep { !ref } @{ $ignore || [] };
    my @ignore_res = grep {ref} @{ $ignore || [] };

    my %warned;

    return sub {
        my %args = @_ < 2 ? ( message => shift ) : @_;

        my ( $package, undef, undef, $sub ) = caller(1);

        my $skipped = 1;

        if ( @ignore_res || keys %ignore ) {
            while ( defined $package
                && ( $ignore{$package} || any { $package =~ $_ } @ignore_res )
                ) {
                $package = caller( $skipped++ );
            }
        }

        $package = 'unknown package' unless defined $package;

        unless ( defined $args{feature} ) {
            $args{feature} = $sub;
        }

        my $compat_version = $registry->{$package};

        my $at = $deprecated_at->{ $args{feature} };

        return
               if defined $compat_version
            && defined $deprecated_at
            && $compat_version lt $at;

        my $msg;
        if ( defined $args{message} ) {
            $msg = $args{message};
        }
        else {
            $msg = "$args{feature} has been deprecated";
            $msg .= " since version $at"
                if defined $at;
        }

        return if $warned{$package}{ $args{feature} }{$msg};

        $warned{$package}{ $args{feature} }{$msg} = 1;

        # We skip at least two levels. One for this anon sub, and one for the
        # sub calling it.
        local $Carp::CarpLevel = $Carp::CarpLevel + $skipped;

        Carp::cluck($msg);
    };
}

1;

# ABSTRACT: Manage deprecation warnings for your distribution

__END__

=pod

=encoding UTF-8

=head1 NAME

Package::DeprecationManager - Manage deprecation warnings for your distribution

=head1 VERSION

version 0.17

=head1 SYNOPSIS

  package My::Class;

  use Package::DeprecationManager -deprecations => {
      'My::Class::foo' => '0.02',
      'My::Class::bar' => '0.05',
      'feature-X'      => '0.07',
  };

  sub foo {
      deprecated( 'Do not call foo!' );

      ...
  }

  sub bar {
      deprecated();

      ...
  }

  sub baz {
      my %args = @_;

      if ( $args{foo} ) {
          deprecated(
              message => ...,
              feature => 'feature-X',
          );
      }
  }

  package Other::Class;

  use My::Class -api_version => '0.04';

  My::Class->new()->foo(); # warns
  My::Class->new()->bar(); # does not warn
  My::Class->new()->bar(); # does not warn again

=head1 DESCRIPTION

This module allows you to manage a set of deprecations for one or more modules.

When you import C<Package::DeprecationManager>, you must provide a set of
C<-deprecations> as a hash ref. The keys are "feature" names, and the values
are the version when that feature was deprecated.

In many cases, you can simply use the fully qualified name of a subroutine or
method as the feature name. This works for cases where the whole subroutine is
deprecated. However, the feature names can be any string. This is useful if
you don't want to deprecate an entire subroutine, just a certain usage.

You can also provide an optional array reference in the C<-ignore>
parameter.

The values to be ignored can be package names or regular expressions (made
with C<qr//>).  Use this to ignore packages in your distribution that can
appear on the call stack when a deprecated feature is used.

As part of the import process, C<Package::DeprecationManager> will export two
subroutines into its caller. It provides an C<import()> sub for the caller and a
C<deprecated()> sub.

The C<import()> sub allows callers of I<your> class to specify an C<-api_version>
parameter. If this is supplied, then deprecation warnings are only issued for
deprecations with API versions earlier than the one specified.

You must call the C<deprecated()> sub in each deprecated subroutine. When
called, it will issue a warning using C<Carp::cluck()>.

The C<deprecated()> sub can be called in several ways. If you do not pass any
arguments, it will generate an appropriate warning message. If you pass a
single argument, this is used as the warning message.

Finally, you can call it with named arguments. Currently, the only allowed
names are C<message> and C<feature>. The C<feature> argument should correspond
to the feature name passed in the C<-deprecations> hash.

If you don't explicitly specify a feature, the C<deprecated()> sub uses
C<caller()> to identify its caller, using its fully qualified subroutine name.

A given deprecation warning is only issued once for a given package. This
module tracks this based on both the feature name I<and> the error message
itself. This means that if you provide several different error messages for
the same feature, all of those errors will appear.

=head2 Other import() subs

This module works by installing an C<import> sub in any package that uses
it. If that package I<already> has an C<import> sub, then that C<import> will
be called after any arguments passed for C<Package::DeprecationManager> are
stripped out. You need to define your C<import> sub before you C<use
Package::DeprecationManager> to make this work:

  package HasExporter;

  use Exporter qw( import );

  use Package::DeprecationManager -deprecations => {
      'HasExporter::foo' => '0.02',
  };

  our @EXPORT_OK = qw( some_sub another_sub );

=head1 DONATIONS

If you'd like to thank me for the work I've done on this module, please
consider making a "donation" to me via PayPal. I spend a lot of free time
creating free software, and would appreciate any support you'd care to offer.

Please note that B<I am not suggesting that you must do this> in order
for me to continue working on this particular software. I will
continue to do so, inasmuch as I have in the past, for as long as it
interests me.

Similarly, a donation made in this way will probably not make me work on this
software much more, unless I get so many donations that I can consider working
on free software full time, which seems unlikely at best.

To donate, log into PayPal and send money to autarch@urth.org or use the
button on this page: L<http://www.urth.org/~autarch/fs-donation.html>

=head1 CREDITS

The idea for this functionality and some of its implementation was originally
created as L<Class::MOP::Deprecated> by Goro Fuji.

=head1 BUGS

Please report any bugs or feature requests to
C<bug-package-deprecationmanager@rt.cpan.org>, or through the web interface at
L<http://rt.cpan.org>.  I will be notified, and then you'll automatically be
notified of progress on your bug as I make changes.

Bugs may be submitted through L<the RT bug tracker|http://rt.cpan.org/Public/Dist/Display.html?Name=Package-DeprecationManager>
(or L<bug-package-deprecationmanager@rt.cpan.org|mailto:bug-package-deprecationmanager@rt.cpan.org>).

I am also usually active on IRC as 'drolsky' on C<irc://irc.perl.org>.

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

=head1 CONTRIBUTORS

=for stopwords Jesse Luehrs Karen Etheridge Tomas Doran

=over 4

=item *

Jesse Luehrs <doy@tozt.net>

=item *

Karen Etheridge <ether@cpan.org>

=item *

Tomas Doran <bobtfish@bobtfish.net>

=back

=head1 COPYRIGHT AND LICENCE

This software is Copyright (c) 2016 by Dave Rolsky.

This is free software, licensed under:

  The Artistic License 2.0 (GPL Compatible)

=cut
