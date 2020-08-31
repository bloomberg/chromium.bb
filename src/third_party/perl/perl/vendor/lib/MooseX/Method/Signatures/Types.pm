package MooseX::Method::Signatures::Types;
#ABSTRACT: (DEPRECATED) Provides common MooseX::Types used by MooseX::Method::Signatures

our $VERSION = '0.49';

use MooseX::Types 0.19 -declare => [qw/ Injections PrototypeInjections Params /];
use MooseX::Types::Moose qw/Str ArrayRef/;
use MooseX::Types::Structured 0.24 qw/Dict/;
use Parse::Method::Signatures::Types qw/Param/;
use if MooseX::Types->VERSION >= 0.42, 'namespace::autoclean';

subtype Injections,
    as ArrayRef[Str];

subtype PrototypeInjections,
    as Dict[declarator => Str, injections => Injections];

subtype Params,
    as ArrayRef[Param];

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

MooseX::Method::Signatures::Types - (DEPRECATED) Provides common MooseX::Types used by MooseX::Method::Signatures

=head1 VERSION

version 0.49

=head1 SUPPORT

Bugs may be submitted through L<the RT bug tracker|https://rt.cpan.org/Public/Dist/Display.html?Name=MooseX-Method-Signatures>
(or L<bug-MooseX-Method-Signatures@rt.cpan.org|mailto:bug-MooseX-Method-Signatures@rt.cpan.org>).

There is also a mailing list available for users of this distribution, at
L<http://lists.perl.org/list/moose.html>.

There is also an irc channel available for users of this distribution, at
irc://irc.perl.org/#moose.

I am also usually active on irc, as 'ether' at C<irc.perl.org>.

=head1 AUTHOR

Florian Ragwitz <rafl@debian.org>

=head1 COPYRIGHT AND LICENCE

This software is copyright (c) 2008 by Florian Ragwitz.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut
