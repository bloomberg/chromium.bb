package Email::Sender::Failure::Permanent;
# ABSTRACT: a permanent delivery failure
$Email::Sender::Failure::Permanent::VERSION = '1.300031';
use Moo;
extends 'Email::Sender::Failure';

no Moo;
1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Email::Sender::Failure::Permanent - a permanent delivery failure

=head1 VERSION

version 1.300031

=head1 AUTHOR

Ricardo Signes <rjbs@cpan.org>

=head1 COPYRIGHT AND LICENSE

This software is copyright (c) 2017 by Ricardo Signes.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut
