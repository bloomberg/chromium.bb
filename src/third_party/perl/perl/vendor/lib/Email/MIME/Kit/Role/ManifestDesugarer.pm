package Email::MIME::Kit::Role::ManifestDesugarer;
# ABSTRACT: helper for desugaring manifests
$Email::MIME::Kit::Role::ManifestDesugarer::VERSION = '3.000006';
use Moose::Role;

#pod =head1 IMPLEMENTING
#pod
#pod This role also performs L<Email::MIME::Kit::Role::Component>.
#pod
#pod This is a role more likely to be consumed than implemented.  It wraps C<around>
#pod the C<read_manifest> method in the consuming class, and "desugars" the contents
#pod of the loaded manifest before returning it.
#pod
#pod At present, desugaring is what allows the C<type> attribute in attachments and
#pod alternatives to be given instead of a C<content_type> entry in the
#pod C<attributes> entry.  In other words, desugaring turns:
#pod
#pod   {
#pod     header => [ ... ],
#pod     type   => 'text/plain',
#pod   }
#pod
#pod Into:
#pod
#pod   {
#pod     header => [ ... ],
#pod     attributes => { content_type => 'text/plain' },
#pod   }
#pod
#pod More behavior may be added to the desugarer later.
#pod
#pod =cut

my $ct_desugar;
$ct_desugar = sub {
  my ($self, $content) = @_;

  for my $thing (qw(alternatives attachments)) {
    for my $part (@{ $content->{ $thing } }) {
      my $headers = $part->{header} ||= [];
      if (my $type = delete $part->{type}) {
        confess "specified both type and content_type attribute"
          if $part->{attributes}{content_type};

        $part->{attributes}{content_type} = $type;
      }

      $self->$ct_desugar($part);
    }
  }
};

around read_manifest => sub {
  my ($orig, $self, @args) = @_;
  my $content = $self->$orig(@args);

  $self->$ct_desugar($content);

  return $content;
};

no Moose::Role;
1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Email::MIME::Kit::Role::ManifestDesugarer - helper for desugaring manifests

=head1 VERSION

version 3.000006

=head1 IMPLEMENTING

This role also performs L<Email::MIME::Kit::Role::Component>.

This is a role more likely to be consumed than implemented.  It wraps C<around>
the C<read_manifest> method in the consuming class, and "desugars" the contents
of the loaded manifest before returning it.

At present, desugaring is what allows the C<type> attribute in attachments and
alternatives to be given instead of a C<content_type> entry in the
C<attributes> entry.  In other words, desugaring turns:

  {
    header => [ ... ],
    type   => 'text/plain',
  }

Into:

  {
    header => [ ... ],
    attributes => { content_type => 'text/plain' },
  }

More behavior may be added to the desugarer later.

=head1 AUTHOR

Ricardo Signes <rjbs@cpan.org>

=head1 COPYRIGHT AND LICENSE

This software is copyright (c) 2018 by Ricardo Signes.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut
