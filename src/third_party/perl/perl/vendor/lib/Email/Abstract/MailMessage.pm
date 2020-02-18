use strict;
package Email::Abstract::MailMessage;
# ABSTRACT: Email::Abstract wrapper for Mail::Message
$Email::Abstract::MailMessage::VERSION = '3.008';
use Email::Abstract::Plugin;
BEGIN { @Email::Abstract::MailMessage::ISA = 'Email::Abstract::Plugin' };

sub target { "Mail::Message" }

sub construct {
    require Mail::Message;
    my ($class, $rfc822) = @_;
    Mail::Message->read($rfc822);
}

sub get_header {
    my ($class, $obj, $header) = @_;
    $obj->head->get($header);
}

sub get_body {
    my ($class, $obj) = @_;
    $obj->decoded->string;
}

sub set_header {
    my ($class, $obj, $header, @data) = @_;
    $obj->head->delete($header);
    $obj->head->add($header, $_) for @data;
}

sub set_body {
    my ($class, $obj, $body) = @_;
    $obj->body(Mail::Message::Body->new(data => $body));
}

sub as_string {
    my ($class, $obj) = @_;
    $obj->string;
}

1;

#pod =head1 DESCRIPTION
#pod
#pod This module wraps the Mail::Message mail handling library with an
#pod abstract interface, to be used with L<Email::Abstract>
#pod
#pod =head1 SEE ALSO
#pod
#pod L<Email::Abstract>, L<Mail::Message>.
#pod
#pod =cut

__END__

=pod

=encoding UTF-8

=head1 NAME

Email::Abstract::MailMessage - Email::Abstract wrapper for Mail::Message

=head1 VERSION

version 3.008

=head1 DESCRIPTION

This module wraps the Mail::Message mail handling library with an
abstract interface, to be used with L<Email::Abstract>

=head1 SEE ALSO

L<Email::Abstract>, L<Mail::Message>.

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
