package Email::MIME::Kit::Role::ManifestReader;
# ABSTRACT: things that read kit manifests
$Email::MIME::Kit::Role::ManifestReader::VERSION = '3.000006';
use Moose::Role;
with 'Email::MIME::Kit::Role::Component';

#pod =head1 IMPLEMENTING
#pod
#pod This role also performs L<Email::MIME::Kit::Role::Component>.
#pod
#pod Classes implementing this role must provide a C<read_manifest> method, which is
#pod expected to locate and read a manifest for the kit.  Classes implementing this
#pod role should probably include L<Email::MIME::Kit::Role::ManifestDesugarer>, too.
#pod
#pod =cut

requires 'read_manifest';

no Moose::Role;
1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Email::MIME::Kit::Role::ManifestReader - things that read kit manifests

=head1 VERSION

version 3.000006

=head1 IMPLEMENTING

This role also performs L<Email::MIME::Kit::Role::Component>.

Classes implementing this role must provide a C<read_manifest> method, which is
expected to locate and read a manifest for the kit.  Classes implementing this
role should probably include L<Email::MIME::Kit::Role::ManifestDesugarer>, too.

=head1 AUTHOR

Ricardo Signes <rjbs@cpan.org>

=head1 COPYRIGHT AND LICENSE

This software is copyright (c) 2018 by Ricardo Signes.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut
