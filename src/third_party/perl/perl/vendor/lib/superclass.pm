use 5.008001;
use strict;
use warnings;

package superclass;
# ABSTRACT: Like parent, but with version checks
our $VERSION = '0.003'; # VERSION

use version 0.9901; # sane UNIVERSAL::VERSION, more or less
use Module::Load 0.24 (); # apostrophe support

# module name regular expression
my $mod_re = qr/^[A-Z_a-z][0-9A-Z_a-z]*(?:(?:::|')[0-9A-Z_a-z]+)*$/;

sub import {
    my $class = shift;

    my $inheritor = caller(0);

    my $no_require;
    if ( @_ and $_[0] eq '-norequire' ) {
        shift @_;
        $no_require++;
    }

    while (@_) {
        my $module = shift @_;
        my $version = ( @_ && $_[0] !~ $mod_re ) ? shift(@_) : 0;
        if ( $module eq $inheritor ) {
            warn "Class '$inheritor' tried to inherit from itself\n";
        }
        Module::Load::load($module) unless $no_require; # dies if not found
        $module->VERSION($version) if $version;         # don't check '0'
        {
            no strict 'refs';
            push @{"$inheritor\::ISA"}, $module;
        };
    }
}

1;

=pod

=encoding UTF-8

=head1 NAME

superclass - Like parent, but with version checks

=head1 VERSION

version 0.003

=head1 SYNOPSIS

    package Baz;
    use superclass qw(Foo Bar), 'Baz' => 1.23;

=head1 DESCRIPTION

Allows you to both load one or more modules, while setting up inheritance from
those modules at the same time.

If a module in the import list is followed by something that doesn't look like
a legal module name, the C<VERSION> method will be called with it as an argument;

The synopsis example is mostly similar in effect to

    package Baz;
    BEGIN {
        require Foo;
        require Bar;
        require Baz;
        Baz->VERSION(1.23)
        push @ISA, qw(Foo Bar Baz);
    }

Dotted-decimal versions should be given as a string, not a raw v-string, and
must include at least one decimal point.

    use superclass 'Bar' => v65.65.65; # BAD: loads AAA.pm

    use superclass 'Bar' => 'v6';      # BAD: loads v6.pm

    use superclass 'Foo' => 'v0.10.0'; # OK

If the first import argument is C<-norequire>, no files will be loaded
(but any versions given will still be checked).

This is helpful for the case where a package lives within the current file
or a differently named file:

  package MyHash;
  use Tie::Hash;
  use superclass -norequire, 'Tie::StdHash';

=for Pod::Coverage method_names_here

=head1 DIAGNOSTICS

=over 4

=item Class 'Foo' tried to inherit from itself

Attempting to inherit from yourself generates a warning.

    package Foo;
    use superclass 'Foo';

=back

=head1 HISTORY

This module was forked from L<parent> to add version checks.
The L<parent> module was forked from L<base> to remove the cruft
that had accumulated in it.

Authors of or contributors to predecessor modules include Rafaël Garcia-Suarez,
Bart Lateur, Max Maischein, Anno Siegel, and Michael Schwern

=for :stopwords cpan testmatrix url annocpan anno bugtracker rt cpants kwalitee diff irc mailto metadata placeholders metacpan

=head1 SUPPORT

=head2 Bugs / Feature Requests

Please report any bugs or feature requests through the issue tracker
at L<https://github.com/dagolden/superclass/issues>.
You will be notified automatically of any progress on your issue.

=head2 Source Code

This is open source software.  The code repository is available for
public review and contribution under the terms of the license.

L<https://github.com/dagolden/superclass>

  git clone https://github.com/dagolden/superclass.git

=head1 AUTHOR

David Golden <dagolden@cpan.org>

=head1 COPYRIGHT AND LICENSE

This software is copyright (c) 2013 by David Golden.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut

__END__


# vim: ts=4 sts=4 sw=4 et:
