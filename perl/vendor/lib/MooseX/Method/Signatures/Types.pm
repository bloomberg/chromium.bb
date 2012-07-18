package MooseX::Method::Signatures::Types;
BEGIN {
  $MooseX::Method::Signatures::Types::AUTHORITY = 'cpan:FLORA';
}
{
  $MooseX::Method::Signatures::Types::VERSION = '0.42';
}

use MooseX::Types 0.19 -declare => [qw/ Injections PrototypeInjections Params /];
use MooseX::Types::Moose qw/Str ArrayRef/;
use MooseX::Types::Structured 0.24 qw/Dict/;
use Parse::Method::Signatures::Types qw/Param/;

subtype Injections,
    as ArrayRef[Str];

subtype PrototypeInjections,
    as Dict[declarator => Str, injections => Injections];

subtype Params,
    as ArrayRef[Param];

1;

__END__
=pod

=encoding utf-8

=head1 NAME

MooseX::Method::Signatures::Types

=head1 AUTHORS

=over 4

=item *

Florian Ragwitz <rafl@debian.org>

=item *

Ash Berlin <ash@cpan.org>

=item *

Cory Watson <gphat@cpan.org>

=item *

Daniel Ruoso <daniel@ruoso.com>

=item *

Dave Rolsky <autarch@urth.org>

=item *

Hakim Cassimally <hakim.cassimally@gmail.com>

=item *

Jonathan Scott Duff <duff@pobox.com>

=item *

Justin Hunter <justin.d.hunter@gmail.com>

=item *

Kent Fredric <kentfredric@gmail.com>

=item *

Maik Hentsche <maik.hentsche@amd.com>

=item *

Matt Kraai <kraai@ftbfs.org>

=item *

Rhesa Rozendaal <rhesa@cpan.org>

=item *

Ricardo SIGNES <rjbs@cpan.org>

=item *

Steffen Schwigon <ss5@renormalist.net>

=item *

Yanick Champoux <yanick@babyl.dyndns.org>

=item *

Nicholas Perez <nperez@cpan.org>

=item *

Karen Etheridge <ether@cpan.org>

=back

=head1 COPYRIGHT AND LICENSE

This software is copyright (c) 2012 by Florian Ragwitz.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut

