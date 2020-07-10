use strict;
use warnings;
package Email::MIME::Encodings;
{
  $Email::MIME::Encodings::VERSION = '1.315';
}
# ABSTRACT: A unified interface to MIME encoding and decoding

use MIME::Base64 3.05;
use MIME::QuotedPrint 3.05;

sub identity { $_[0] }

for (qw(7bit 8bit binary)) {
    no strict 'refs';
    *{"encode_$_"} = *{"decode_$_"} = \&identity;
}

sub codec {
    my ($which, $how, $what, $fb) = @_;
    $how = lc $how;
    $how = "qp" if $how eq "quotedprint" or $how eq "quoted-printable";
    my $sub = __PACKAGE__->can("$which\_$how");

    if (! $sub && $fb) {
        $fb = lc $fb;
        $fb = "qp" if $fb eq "quotedprint" or $fb eq "quoted-printable";
        $sub = __PACKAGE__->can("$which\_$fb");
    }

    unless ($sub) {
        require Carp;
        Carp::croak("Don't know how to $which $how");
    }

    # RFC2822 requires all email lines to end in CRLF. The Quoted-Printable
    # RFC requires CRLF to not be encoded, when representing newlins.  We will
    # assume, in this code, that QP is being used for plain text and not binary
    # data.  This may, someday, be wrong -- but if you are using QP to encode
    # binary data, you are already doing something bizarre.
    #
    # The only way to achieve this with MIME::QuotedPrint is to replace all
    # CRLFs with just LF and then let MIME::QuotedPrint replace all LFs with
    # CRLF. Otherwise MIME::QuotedPrint (by default) encodes CR as =0D, which
    # is against RFCs and breaks MUAs (such as Thunderbird).
    #
    # We don't modify data before Base64 encoding it because that is usually
    # binary data and modifying it at all is a bad idea. We do however specify
    # that the encoder should end lines with CRLF (since that's the email
    # standard).
    # -- rjbs and mkanat, 2009-04-16
    my $eol = "\x0d\x0a";
    if ($which eq 'encode') {
        $what =~ s/$eol/\x0a/sg if $how eq 'qp';
        return $sub->($what, $eol);
    } else {
        my $txt = $sub->($what);
        $txt =~ s/\x0a/$eol/sg if $how eq 'qp';
        return $txt;
    }
}

sub decode { return codec("decode", @_) }
sub encode { return codec("encode", @_) }

1;

__END__

=pod

=head1 NAME

Email::MIME::Encodings - A unified interface to MIME encoding and decoding

=head1 VERSION

version 1.315

=head1 SYNOPSIS

  use Email::MIME::Encodings;
  my $encoded = Email::MIME::Encodings::encode(base64 => $body);
  my $decoded = Email::MIME::Encodings::decode(base64 => $encoded);

If a third argument is given, it is the encoding to which to fall back.  If no
valid codec can be found (considering both the first and third arguments) then
an exception is raised.

=head1 DESCRIPTION

This module simply wraps C<MIME::Base64> and C<MIME::QuotedPrint>
so that you can throw the contents of a C<Content-Transfer-Encoding>
header at some text and have the right thing happen.

C<MIME::Base64>, C<MIME::QuotedPrint>, C<Email::MIME>.

=head1 AUTHORS

=over 4

=item *

Simon Cozens <simon@cpan.org>

=item *

Casey West <casey@geeknest.com>

=item *

Ricardo SIGNES <rjbs@cpan.org>

=back

=head1 COPYRIGHT AND LICENSE

This software is copyright (c) 2004 by Simon Cozens and Casey West.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut
