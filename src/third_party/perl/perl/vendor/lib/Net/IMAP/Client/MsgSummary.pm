package Net::IMAP::Client::MsgSummary;

use Encode ();
use Net::IMAP::Client::MsgAddress ();

sub new {
    my ($class, $data, $part_id, $has_headers) = @_;

    bless my $self = {}, $class;

    if ($part_id) {
        $self->{part_id} = $part_id;
    }

    my $tmp = $data->{BODY};
    if ($tmp) {
        $self->_parse_body($tmp);
    }

    $tmp = $data->{BODYSTRUCTURE};
    if ($tmp) {
        $self->_parse_bodystructure($tmp);
    }

    $tmp = $data->{ENVELOPE};
    if ($tmp) {
        $self->_parse_envelope($tmp);
    }

    $self->{flags} = $data->{FLAGS};
    $self->{internaldate} = $data->{INTERNALDATE};
    $self->{rfc822_size} = $data->{'RFC822.SIZE'};
    $self->{uid} = $data->{UID};

    if ($has_headers) {
        while (my ($key, $val) = each %$data) {
            if ($key =~ /^body(?:\.peek)?\s*\[\s*header\.fields/i) {
                $self->{headers} = $val;
                last;
            }
        }
    }

    return $self;
}

sub _decode {
    my ($str) = @_;
    if (defined($str)) {
        eval { $str = Encode::decode('MIME-Header', $str); };
    }
    return $str;
}

sub type { $_[0]->{type} }

sub subtype { $_[0]->{subtype} }

sub parameters { $_[0]->{parameters} }

sub cid { $_[0]->{cid} }

sub description { _decode($_[0]->{description}) }

sub transfer_encoding { $_[0]->{transfer_encoding} }

sub encoded_size { $_[0]->{encoded_size} }

sub content_type {
    my ($self) = @_;
    if ($self->type) {
        return $self->type . '/' . $self->subtype;
    }
    if ($self->multipart) {
        return 'multipart/' . $self->multipart;
    }
    return undef;
}

sub charset { $_[0]->{parameters}->{charset} }

sub filename {
    my ($self) = @_;
    my $disp = $self->{disposition};
    my $filename;
    if ($disp) {
        while (my ($key, $val) = each %$disp) {
            if (ref($val) eq 'HASH') {
                $filename = $val->{filename};
                last if $filename;
            }
        }
    }
    unless ($filename) {
        $filename = $_[0]->{parameters}->{name};
    }
    return _decode($filename);
}

sub name { _decode($_[0]->{parameters}->{name}) }

sub multipart { $_[0]->{multipart_type} }

sub parts { $_[0]->{parts} }

sub rfc822_size { $_[0]->{rfc822_size} }

sub internaldate { $_[0]->{internaldate} }

sub flags { $_[0]->{flags} }

sub uid { $_[0]->{uid} }

sub part_id { $_[0]->{part_id } }

sub md5 { $_[0]->{md5} }

sub disposition { $_[0]->{disposition} }

sub language { $_[0]->{language} }

# envelope

sub date { $_[0]->{date} }

sub subject { _decode($_[0]->{subject}) }

sub from { $_[0]->{from} }

sub sender { $_[0]->{sender} }

sub reply_to { $_[0]->{reply_to} }

sub to { $_[0]->{to} }

sub cc { $_[0]->{cc} }

sub bcc { $_[0]->{bcc} }

sub in_reply_to { $_[0]->{in_reply_to} }

sub message_id { $_[0]->{message_id} }

sub seq_id { $_[0]->{seq_id} }

sub headers { $_[0]->{headers} }

# utils

sub get_subpart {
    my ($self, $part) = @_;
    foreach my $index (split(/\./, $part)) {
        $self = $self->parts->[$index - 1];
    }
    return $self;
}

my %MT_HAS_ATTACHMENT = ( mixed => 1 );

sub has_attachments {
    my ($self) = @_;
    my $mt = $self->multipart;
    return $mt && $MT_HAS_ATTACHMENT{$mt} ? 1 : 0;
}

sub is_message { $_[0]->content_type eq 'message/rfc822' }

sub message { $_[0]->{message} }

sub _parse_body {
    my ($self, $struct) = @_;

    if (ref($struct->[0]) eq 'ARRAY') {
        my @tmp = @$struct;
        my $multipart = pop @tmp;
        my $part_id = $self->{part_id} || '';
        $part_id .= '.'
          if $part_id;
        my $i = 0;
        @tmp = map { __PACKAGE__->new({ BODY => $_}, $part_id . ++$i) } @tmp;
        $self->{multipart_type} = lc $multipart;
        $self->{parts} = \@tmp;
    } else {
        $self->{type} = lc $struct->[0];
        $self->{subtype} = lc $struct->[1];
        if ($struct->[2]) {
            my %tmp = @{$struct->[2]};
            $self->{parameters} = \%tmp;
        }
        $self->{cid} = $struct->[3];
        $self->{description} = $struct->[4];
        $self->{transfer_encoding} = $struct->[5];
        $self->{encoded_size} = $struct->[6];

        if ($self->is_message && $struct->[7] && $struct->[8]) {
            # continue parsing attached message
            $self->{message} = __PACKAGE__->new({
                ENVELOPE => $struct->[7],
                BODY => $struct->[8],
            });
        }
    }
}

sub _parse_bodystructure {
    my ($self, $struct) = @_;

    if (ref($struct->[0]) eq 'ARRAY') {
        my $multipart;
        my @tmp;
        foreach (@$struct) {
            if (ref($_) eq 'ARRAY') {
                push @tmp, $_;
            } else {
                $multipart = $_;
                last;           # XXX: ignoring the rest (extension data) for now.
            }
        }
        my $part_id = $self->{part_id} || '';
        $part_id .= '.'
          if $part_id;
        my $i = 0;
        @tmp = map { __PACKAGE__->new({ BODYSTRUCTURE => $_}, $part_id . ++$i) } @tmp;
        $self->{multipart_type} = lc $multipart;
        $self->{parts} = \@tmp;
    } else {
        $self->{type} = lc $struct->[0];
        $self->{subtype} = lc $struct->[1];
        my $a = $struct->[2];
        if ($a) {
            __lc_key_in_array($a);
            my %tmp = @$a;
            $self->{parameters} = \%tmp;
        }
        $self->{cid} = $struct->[3];
        $self->{description} = $struct->[4];
        $self->{transfer_encoding} = $struct->[5];
        $self->{encoded_size} = $struct->[6];

        if ($self->is_message && $struct->[7] && $struct->[8]) {
            # continue parsing attached message
            $self->{message} = __PACKAGE__->new({
                ENVELOPE => $struct->[7],
                BODYSTRUCTURE => $struct->[8],
            });
        } elsif ($self->type ne 'text') {
            $self->{md5} = $struct->[7];
            my $a = $struct->[8];
            if ($a) {
                for (my $i = 0; $i < @$a; ++$i) {
                    $a->[$i] = lc $a->[$i];
                    ++$i;
                    if (ref($a->[$i]) eq 'ARRAY') {
                        __lc_key_in_array($a->[$i]);
                        my %foo = @{ $a->[$i] };
                        $a->[$i] = \%foo;
                    }
                }
                my %tmp = @$a;
                $self->{disposition} = \%tmp;
            }
            $self->{language} = $struct->[9];
        }
    }
}

sub __lc_key_in_array {
    my ($a) = @_;
    for (my $i = 0; $i < @$a; $i += 2) {
        $a->[$i] = lc $a->[$i];
    }
}

sub _parse_envelope {
    my ($self, $struct)  = @_;
    $self->{date}        = $struct->[0];
    $self->{subject}     = $struct->[1];
    $self->{from}        = _parse_address($struct->[2]);
    $self->{sender}      = _parse_address($struct->[3]);
    $self->{reply_to}    = _parse_address($struct->[4]);
    $self->{to}          = _parse_address($struct->[5]);
    $self->{cc}          = _parse_address($struct->[6]);
    $self->{bcc}         = _parse_address($struct->[7]);
    $self->{in_reply_to} = $struct->[8];
    $self->{message_id}  = $struct->[9];
}

sub _parse_address {
    my ($adr) = @_;
    if ($adr) {
        $adr = [ map { Net::IMAP::Client::MsgAddress->new($_) } @$adr ];
    }
    return $adr;
}

1;

__END__

=pod

=head1 NAME

Net::IMAP::Client::MsgSummary - parse message (+ subparts) summary info

=head1 SYNOPSIS

This object is created internally in Net::IMAP::Client->get_summaries.
You shouldn't need to instantiate it directly.  You can skip the
SYNOPSIS, these notes are intended for developers.

    my $imap = Net::IMAP::Client->new( ... )
    $imap->select('INBOX');

    # retrieve FETCH lines
    my ($ok, $lines) = $imap->_tell_imap(FETCH => "$msg_id FULL");
    die 'FETCH failed: ' . $imap->last_error
      unless $ok;

    # build parsed tokens
    my @tokens = map { Net::IMAP::Client::_parse_tokens($_) } @$lines;

    # they look like this:
    [ '*', 'MSGID', 'FETCH',
      [ 'FLAGS', [ '\\Seen', '\\Answered' ],
        'INTERNALDATE', '13-Aug-2008 14:43:50 +0300',
        'RFC822.SIZE', '867',
        'ENVELOPE', [
           ...
        ]
      ...
    ]

Basically it's the IMAP response parsed into a Perl structure (array
of tokens).  FIXME: this stuff should be documented in
Net::IMAP::Client.

    # make summaries
    my @summaries = map {
        my $tokens = $_->[3];
        my %hash = @$tokens;
        Net::IMAP::Client::MsgSummary->new(\%hash);
    } @tokens;

    my $summary = shift @summaries;

    print $summary->subject;
    print $summary->from->[0];

=head1 DESCRIPTION

This object can represent a message or a message part.  For example,
for a message containing attachments you will be able to call parts()
in order to fetch parsed Net::IMAP::Client::MsgSummary objects for
each part.  Each part in turn may contain other subparts!  For
example, if a part is of type C<message/rfc822> then its C<parts>
method will return it's subparts, if any.

There's a distinction between a message and a message part, although
we use the same object to represent both.  A message will have
additional information, fetched from its ENVELOPE (i.e. C<subject>,
C<from>, C<to>, C<date>, etc.).  For a part only, this information
will be missing.

If all this sounds confusing, you might want to use Data::Dumper to
inspect the structure of a complex message.  See also the
documentation of L<Net::IMAP::Client>'s get_summaries method for an
example.

=head1 API REFERENCE

It contains only accessors that return data as retrieved by the FETCH
command.  Parts that may be MIME-word encoded are automatically
undecoded.

=head2 C<new>  # constructor

Parses/creates a new object from the given FETCH data.

=over

=item C<type>

Returns the base MIME type (i.e. 'text')

=item C<subtype>

Returns the subtype (i.e. 'plain')

=item C<parameters>

Returns any parameters passed in BODY(STRUCTURE).  You shouldn't need
this.

=item C<cid>

Returns the part's unique identifier (CID).

=item C<description>

Returns the part's description (usually I<undef>).

=item C<transfer_encoding>

Returns the part's content transfer encoding.  You'll need this in
order to decode binary parts.

=item C<encoded_size>

Returns the size of the encoded part.  This is actually the size in
octets that will be downloaded from the IMAP server if you fetch this
part only.

=item C<content_type>

Shortcut for C<$self->type . '/' .$self->subtype>.

=item C<charset>

Returns the charset declaration for this part.

=item C<name>

Returns the name of this part, if found in FETCH response.

=item C<filename>

Returns the file name of this part, if found in FETCH response.  If
there's no filename it will try C<name>.

=item C<multipart>

Returns the multipart type (i.e. 'mixed', 'alternative')

=item C<parts>

Returns the subparts of this part.

=item C<part_id>

Returns the "id" (path) of this part starting from the toplevel
message, i.e. "2.1" (meaning that this is the first subpart of the
second subpart of the toplevel message).

=item C<md5>

Returns a MD5 of this part or I<undef> if not present.

=item C<disposition>

Returns the disposition of this part (I<undef> if not present).  It's
a hash actually that looks like this:

  { inline => { filename => 'foobar.png' } }

=item C<language>

Returns the language of this part or I<undef> if not present.

=item C<rfc822_size>

Returns the size of the full message body.

=item C<internaldate>

Returns the INTERNALDATE of this message.

=item C<flags>

Returns the flags of this message.

=item C<uid>

Returns the UID of this message.

=item C<seq_id>

Returns the sequence number of this message, if it has been retrieved!

=item C<date>

Returns the date of this message (from the Date header).

=item C<subject>

Returns the subject of this message.

=item C<from>, C<sender>, C<reply_to>, C<to>, C<cc>, C<bcc>

Returns an array of Net::IMAP::Client::MsgAddress objects containing
the respective addresses.  Note that sometimes this array can be
empty!

=item C<in_reply_to>

Returns the ID of the "parent" message (to which this one has been
replied).  This is NOT the "UID" of the message!

=item C<message_id>

Returns the ID of this message (from the Message-ID header).

=item C<get_subpart> ($path)

Returns the subpart of this message identified by $path, which is in
form '1.2' etc.  Returns undef if no such path was found.

Here's a possible message structure:

 - Container (multipart/mixed) has no path ID; it's the toplevel
   message.  It contains the following subparts:

   1 multipart/related
     1.1 text/html
     1.2 image/png (embedded in HTML)

   2 message/rfc822 (decoded type is actually multipart/related)
     2.1 text/html
     2.2 image/png (also embedded)

C<get_subpart> called on the container will return the respective
Net::IMAP::Client::MsgSummary part, i.e. get_subpart('2.1') will
return the text/html part of the attached message.

=item C<has_attachments>

Tries to determine if this message has attachments.  For now this
checks if the multipart type is 'mixed', which isn't really accurate.

=item C<is_message>

Returns true if this object represents a message (i.e. has
content_type eq 'message/rfc822').  Note that it won't return true for
the toplevel part, but you B<know> that that part represents a
message. ;-)

=item C<message>

Returns the attached rfc822 message

=item C<headers>

Returns (unparsed, as plain text) additional message headers if they
were fetched by get_summaries.  You can use L<MIME::Head> to parse
them.

=back

=head1 TODO

Fix C<has_attachments>

=head1 SEE ALSO

L<Net::IMAP::Client>, L<Net::IMAP::Client::MsgAddress>

=head1 AUTHOR

Mihai Bazon, <mihai.bazon@gmail.com>
    http://www.dynarchlib.com/
    http://www.bazon.net/mishoo/

=head1 COPYRIGHT

Copyright (c) Mihai Bazon 2008.  All rights reserved.

This module is free software; you can redistribute it and/or modify it
under the same terms as Perl itself.

=head1 DISCLAIMER OF WARRANTY

BECAUSE THIS SOFTWARE IS LICENSED FREE OF CHARGE, THERE IS NO WARRANTY
FOR THE SOFTWARE, TO THE EXTENT PERMITTED BY APPLICABLE LAW. EXCEPT
WHEN OTHERWISE STATED IN WRITING THE COPYRIGHT HOLDERS AND/OR OTHER
PARTIES PROVIDE THE SOFTWARE "AS IS" WITHOUT WARRANTY OF ANY KIND,
EITHER EXPRESSED OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
PURPOSE. THE ENTIRE RISK AS TO THE QUALITY AND PERFORMANCE OF THE
SOFTWARE IS WITH YOU. SHOULD THE SOFTWARE PROVE DEFECTIVE, YOU ASSUME
THE COST OF ALL NECESSARY SERVICING, REPAIR, OR CORRECTION.

IN NO EVENT UNLESS REQUIRED BY APPLICABLE LAW OR AGREED TO IN WRITING
WILL ANY COPYRIGHT HOLDER, OR ANY OTHER PARTY WHO MAY MODIFY AND/OR
REDISTRIBUTE THE SOFTWARE AS PERMITTED BY THE ABOVE LICENCE, BE LIABLE
TO YOU FOR DAMAGES, INCLUDING ANY GENERAL, SPECIAL, INCIDENTAL, OR
CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OR INABILITY TO USE THE
SOFTWARE (INCLUDING BUT NOT LIMITED TO LOSS OF DATA OR DATA BEING
RENDERED INACCURATE OR LOSSES SUSTAINED BY YOU OR THIRD PARTIES OR A
FAILURE OF THE SOFTWARE TO OPERATE WITH ANY OTHER SOFTWARE), EVEN IF
SUCH HOLDER OR OTHER PARTY HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH
DAMAGES.

=cut
