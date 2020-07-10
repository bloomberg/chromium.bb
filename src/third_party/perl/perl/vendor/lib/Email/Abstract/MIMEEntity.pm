use strict;
package Email::Abstract::MIMEEntity;
# ABSTRACT: Email::Abstract wrapper for MIME::Entity
$Email::Abstract::MIMEEntity::VERSION = '3.008';
use Email::Abstract::Plugin;
BEGIN { @Email::Abstract::MIMEEntity::ISA = 'Email::Abstract::MailInternet' };

my $is_avail;
sub is_available {
  return $is_avail if defined $is_avail;
  eval { require MIME::Entity; MIME::Entity->VERSION(5.501); 1 };
  return $is_avail = $@ ? 0 : 1;
}

sub target { "MIME::Entity" }

sub construct {
    require MIME::Parser;
    my $parser = MIME::Parser->new;
    $parser->output_to_core(1);
    my ($class, $rfc822) = @_;
    $parser->parse_data($rfc822);
}

sub get_body {
  my ($self, $obj) = @_;
  my $handle = $obj->bodyhandle;
  return $handle ? $handle->as_string : join('', @{ $obj->body });
}

sub set_body {
    my ($class, $obj, $body) = @_;
    my @lines = split /\n/, $body;
    my $io = $obj->bodyhandle->open("w");
    foreach (@lines) { $io->print($_."\n") }
    $io->close;
}

1;

#pod =head1 DESCRIPTION
#pod
#pod This module wraps the MIME::Entity mail handling library with an
#pod abstract interface, to be used with L<Email::Abstract>
#pod
#pod =head1 SEE ALSO
#pod
#pod L<Email::Abstract>, L<MIME::Entity>.
#pod
#pod =cut

__END__

=pod

=encoding UTF-8

=head1 NAME

Email::Abstract::MIMEEntity - Email::Abstract wrapper for MIME::Entity

=head1 VERSION

version 3.008

=head1 DESCRIPTION

This module wraps the MIME::Entity mail handling library with an
abstract interface, to be used with L<Email::Abstract>

=head1 SEE ALSO

L<Email::Abstract>, L<MIME::Entity>.

=head1 AUTHORS

=over 4

=item *

Ricardo SIGNES <rjbs@cpan.org>

=item *

Simon Cozens <simon@cpan.org>

=item *

Casey West <casey@geeknest.com>

=back

=head1 COPYRIGHT AND LICENSE

This software is copyright (c) 2004 by Simon Cozens.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut
