package Email::MIME::Kit::ManifestReader::YAML;
# ABSTRACT: read manifest.yaml files
$Email::MIME::Kit::ManifestReader::YAML::VERSION = '3.000006';
use Moose;

with 'Email::MIME::Kit::Role::ManifestReader';
with 'Email::MIME::Kit::Role::ManifestDesugarer';

use YAML::XS ();

sub read_manifest {
  my ($self) = @_;

  my $yaml_ref = $self->kit->kit_reader->get_kit_entry('manifest.yaml');

  # YAML::XS is documented as expecting UTF-8 bytes, which we give it.
  my ($content) = YAML::XS::Load($$yaml_ref);

  return $content;
}

no Moose;
__PACKAGE__->meta->make_immutable;
1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Email::MIME::Kit::ManifestReader::YAML - read manifest.yaml files

=head1 VERSION

version 3.000006

=head1 AUTHOR

Ricardo Signes <rjbs@cpan.org>

=head1 COPYRIGHT AND LICENSE

This software is copyright (c) 2018 by Ricardo Signes.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut
