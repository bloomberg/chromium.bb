use strict;
use warnings;
package Email::Abstract::EmailSimple;
# ABSTRACT: Email::Abstract wrapper for Email::Simple
$Email::Abstract::EmailSimple::VERSION = '3.008';
use Email::Abstract::Plugin;
BEGIN { @Email::Abstract::EmailSimple::ISA = 'Email::Abstract::Plugin' };

sub target { "Email::Simple" }

sub construct {
    require Email::Simple;
    my ($class, $rfc822) = @_;
    Email::Simple->new($rfc822);
}

sub get_header {
    my ($class, $obj, $header) = @_;
    $obj->header($header);
}

sub get_body {
    my ($class, $obj) = @_;
    $obj->body();
}

sub set_header {
    my ($class, $obj, $header, @data) = @_;
    $obj->header_set($header, @data);
}

sub set_body   {
    my ($class, $obj, $body) = @_;
    $obj->body_set($body);
}

sub as_string {
    my ($class, $obj) = @_;
    $obj->as_string();
}

1;

#pod =head1 DESCRIPTION
#pod
#pod This module wraps the Email::Simple mail handling library with an
#pod abstract interface, to be used with L<Email::Abstract>
#pod
#pod =head1 SEE ALSO
#pod
#pod L<Email::Abstract>, L<Email::Simple>.
#pod
#pod =cut

__END__

=pod

=encoding UTF-8

=head1 NAME

Email::Abstract::EmailSimple - Email::Abstract wrapper for Email::Simple

=head1 VERSION

version 3.008

=head1 DESCRIPTION

This module wraps the Email::Simple mail handling library with an
abstract interface, to be used with L<Email::Abstract>

=head1 SEE ALSO

L<Email::Abstract>, L<Email::Simple>.

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
