use strict;
use warnings;

# ABSTRACT: Activate syntax extensions

package syntax;
{
  $syntax::VERSION = '0.004';
}
BEGIN {
  $syntax::AUTHORITY = 'cpan:PHAYLON';
}

use Carp                qw( carp );
use Data::OptList 0.104 qw( mkopt );

use namespace::clean;

$Carp::Internal{ +__PACKAGE__ }++;
$Carp::Internal{ 'Devel::Declare' } ||= 1;

sub import_into {
    my ($class, $into, @args) = @_;

    my $import = mkopt \@args;

    for my $declaration (@$import) {
        my ($feature, $options) = @$declaration;

        $class->_install_feature(
            $feature,
            $into,
            $options,
            [@args],
        );
    }

    return 1;
}

sub unimport_from {
    my ($class, $from, @args) = @_;

    for my $feature (@args) {

        $class->_uninstall_feature(
            $feature,
            $from,
        );
    }

    return 1;
}

sub import {
    my ($class, @args) = @_;

    my $caller = caller;

    return $class->import_into($caller, @args);
}

sub unimport {
    my ($class, @args) = @_;

    my $caller = caller;

    return $class->unimport_from($caller, @args);
}

sub _parse_feature_name {
    my ($class, $feature) = @_;

    my $name =
        join '/',
        map ucfirst,
        split m{/},
        join '',
        map ucfirst,
        split qr{_}, $feature;

    my $file    = "Syntax/Feature/${name}.pm";
    my $package = $file;
    s{ \/ }{::}xg, s{ \.pm \Z }{}xgi
        for $package;

    return $package, $file;
}

sub _uninstall_feature {
    my ($class, $feature, $target) = @_;

    my ($package, $file) = $class->_parse_feature_name($feature);

    require $file;
    unless ($package->can('uninstall')) {
        carp "Syntax extension $package does not know how to uninstall";
        return;
    }
    return $package->uninstall(
        from        => $target,
        identifier  => $feature,
    );
}

sub _install_feature {
    my ($class, $feature, $target, $options, $all_params) = @_;

    my ($package, $file) = $class->_parse_feature_name($feature);

    require $file;
    return $package->install(
        into        => $target,
        options     => $options,
        identifier  => $feature,
        outer       => $all_params,
    );
}

1;


__END__
=pod

=head1 NAME

syntax - Activate syntax extensions

=head1 VERSION

version 0.004

=head1 SYNOPSIS

    # either
    use syntax 'foo';

    # or
    use syntax foo => { ... };

    # or
    use syntax qw( foo bar ), baz => { ... };

=head1 DESCRIPTION

This module activates community provided syntax extensions to Perl. You pass it
a feature name, and optionally a scalar with arguments, and the dispatching
system will load and install the extension in your package.

The import arguments are parsed with L<Data::OptList>. There are no
standardised options. Please consult the documentation for the specific syntax
feature to find out about possible configuration options.

The passed in feature names are simply transformed: C<function> becomes
L<Syntax::Feature::Function> and C<foo_bar> would become
C<Syntax::Feature::FooBar>.

=head1 METHODS

=head2 import

    syntax->import( @spec );

This method will dispatch the syntax extension setup to the specified feature
handlers for the calling package.

=head2 import_into

    syntax->import_into( $into, @spec );

Same as L</import>, but performs the setup in C<$into> instead of the calling
package.

=head2 unimport

    syntax->unimport( @features );

This method will trigger uninstallations of the C<@features> from the
calling package.

=head2 unimport_from

    syntax->unimport_from( $from, @features );

Same as L</unimport>, but will uninstall the C<@features> from C<$from>.

=head1 RECOMMENDED FEATURES

=over

=item * L<Syntax::Feature::Function>

Activates functions with parameter signatures.

=back

=head1 SEE ALSO

L<Syntax::Feature::Function>,
L<Devel::Declare>

=head1 BUGS

Please report any bugs or feature requests to bug-syntax@rt.cpan.org or through the web interface at:
 http://rt.cpan.org/Public/Dist/Display.html?Name=syntax

=head1 AUTHOR

Robert 'phaylon' Sedlacek <rs@474.at>

=head1 COPYRIGHT AND LICENSE

This software is copyright (c) 2012 by Robert 'phaylon' Sedlacek.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut

