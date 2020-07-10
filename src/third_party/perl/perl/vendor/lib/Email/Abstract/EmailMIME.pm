use strict;
use warnings;
package Email::Abstract::EmailMIME;
# ABSTRACT: Email::Abstract wrapper for Email::MIME
$Email::Abstract::EmailMIME::VERSION = '3.008';
use Email::Abstract::EmailSimple;
BEGIN { @Email::Abstract::EmailMIME::ISA = 'Email::Abstract::EmailSimple' };

sub target { "Email::MIME" }

sub construct {
    require Email::MIME;
    my ($class, $rfc822) = @_;
    Email::MIME->new($rfc822);
}

sub get_body {
    my ($class, $obj) = @_;

    # Return the same thing you'd get from Email::Simple.
    #
    # Ugh.  -- rjbs, 2014-12-27
    return $obj->body_raw;
}

1;

#pod =head1 DESCRIPTION
#pod
#pod This module wraps the Email::MIME mail handling library with an
#pod abstract interface, to be used with L<Email::Abstract>
#pod
#pod =head1 SEE ALSO
#pod
#pod L<Email::Abstract>, L<Email::MIME>.
#pod
#pod =cut

__END__

=pod

=encoding UTF-8

=head1 NAME

Email::Abstract::EmailMIME - Email::Abstract wrapper for Email::MIME

=head1 VERSION

version 3.008

=head1 DESCRIPTION

This module wraps the Email::MIME mail handling library with an
abstract interface, to be used with L<Email::Abstract>

=head1 SEE ALSO

L<Email::Abstract>, L<Email::MIME>.

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
