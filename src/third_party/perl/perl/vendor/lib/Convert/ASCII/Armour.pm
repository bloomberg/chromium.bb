#!/usr/bin/perl -sw
##
## Convert::ASCII::Armour 
##
## Copyright (c) 2001, Vipul Ved Prakash.  All rights reserved.
## This code is free software; you can redistribute it and/or modify
## it under the same terms as Perl itself.
##
## $Id: Armour.pm,v 1.4 2001/03/19 23:15:09 vipul Exp $

package Convert::ASCII::Armour; 
use strict;
use Digest::MD5 qw(md5);
use MIME::Base64;
use Compress::Zlib qw(compress uncompress);
use vars qw($VERSION);

($VERSION)  = '$Revision: 1.4 $' =~ /\s(\d+\.\d+)\s/; 


sub new { 
    return bless {}, shift;
}


sub error { 
    my ($self, $errstr) = @_;
    $$self{errstr} = "$errstr\n";
    return; 
}


sub errstr { 
    my $self = shift;
    return $$self{errstr};
}


sub armour { 

    my ($self, %params) = @_;

    my $compress = $params{Compress} ? "COMPRESSED " : "";
    return undef unless $params{Content};
    $params{Object} = "UNKNOWN $compress DATA" unless $params{Object};

    my $head     = "-"x5 . "BEGIN $compress$params{Object}" . "-"x5;   
    my $tail     = "-"x5 . "END $compress$params{Object}" . "-"x5; 

    my $content  = $self->encode_content (%{$params{Content}}); 
       $content  = compress($content) if $compress;
    my $checksum = encode_base64 (md5 ($content));
    my $econtent = encode_base64 ($content);

    my $eheaders = "";
    for my $key (keys %{$params{Headers}}) { 
       $eheaders .= "$key: $params{Headers}->{$key}\n";
    } 

    my $message  = "$head\n$eheaders\n$econtent=$checksum$tail\n";
    return $message;

}


sub unarmour { 
    my ($self, $message) = @_;

    my ($head, $object, $headers, $content, $tail) = $message =~ 
        m:(-----BEGIN ([^\n\-]+)-----)\n(.*?\n\n)?(.+)(-----END .*?-----)$:s 
        or return $self->error ("Breached Armour.");

    my ($compress, $obj) = $object =~ /^(COMPRESSED )(.*)$/;
    $object = $obj if $obj;
    $content =~ s:=([^\n]+)$::s or return $self->error ("Breached Armour.");
    my $checksum = $1; $content = decode_base64 ($content);
    my $ncheck  = encode_base64 (md5 ($content)); $ncheck =~ s/\n//;
    return $self->error ("Checksum Failed.") unless $ncheck eq $checksum;
    $content = uncompress ($content) if $compress;
    my $dcontent = $self->decode_content ($content) || return; 

    my $dheaders; 
    if ($headers) { 
        my @pairs = split /\n/, $headers;
        for (@pairs) { 
            my ($key, $value) = split /: /, $_, 2; 
            $$dheaders{$key} = $value if $key;
        }
    }            

    my %return = ( Content => $dcontent, 
                   Object  => $object, 
                   Headers => $dheaders );

    return \%return;

}


sub encode_content { 
    my ($self, %data)  = @_;
    my $encoded = "";

    for my $key (keys %data) { 
        $encoded .= length ($key) . chr(0) . length ($data{$key}) . 
                                    chr(0) . "$key$data{$key}"; 
    }

    return $encoded; 
}


sub decode_content { 
    my ($self, $content) = @_; 
    my %data; 

    while ($content) { 
        $content =~ s/^(\d+)\x00(\d+)\x00// || 
            return $self->error ("Inconsistent content."); 
        my $keylen = $1; my $valuelen = $2; 
        my $key = substr $content, 0, $keylen; 
        my $value = substr $content, $keylen, $valuelen; 
        substr ($content, 0, $keylen + $valuelen) = "";
        $data{$key} = $value;
    }

    return \%data;
}


sub   armor {   armour (@_) }
sub unarmor { unarmour (@_) }


1;


=head1 NAME

Convert::ASCII::Armour - Convert binary octets into ASCII armoured messages.

=head1 SYNOPSIS

    my $converter = new Convert::ASCII::Armour; 

    my $message   = $converter->armour( 
                        Object   => "FOO RECORD", 
                        Headers  => { 
                                      Table   => "FooBar", 
                                      Version => "1.23", 
                                    },
                        Content  => { 
                                      Key  => "0x8738FA7382", 
                                      Name => "Zoya Hall",
                                      Pic  => "....",  # gif 
                                    },
                        Compress => 1,
                    );

    print $message; 


    -----BEGIN COMPRESSED FOO RECORD-----
    Version: 1.23
    Table: FooBar

    eJwzZzA0Z/BNLS5OTE8NycgsVgCiRIVciIAJg6EJg0tiSaqhsYJvYlFy...
    XnpOZl5qYlJySmpaekZmVnZObl5+QWFRcUlpWXlFZRWXAk7g6OTs4urm...
    Fh4VGaWAR5ehkbGJqZm5hSUeNXWKDsoGcWpaGpq68bba0dWxtTVmDOYM...
    NzuZ
    =MxpZvjkrv5XyhkVCuXmsBQ==
    -----END COMPRESSED FOO RECORD-----


    my $decoded   = $converter->unarmour( $message ) 
                     || die $converter->errstr();
                        

=head1 DESCRIPTION

This module converts hashes of binary octets into ASCII messages suitable
for transfer over 6-bit clean transport channels. The encoded ASCII
resembles PGP's armoured messages, but are in no way compatible with PGP.

=head1 METHODS

=head2 B<new()>

Constructor.

=head2 B<armour()>

Converts a hash of binary octets into an ASCII encoded message. The
encoded message has 4 parts: head and tail strings that act as identifiers
and delimiters, a cluster of headers at top of the message, Base64 encoded
message body and a Base64 encoded MD5 digest of the message body. armour()
takes a hash as argument with following keys:

=over 4

=item B<Object>

An identification string embedded in head and tail strings.

=item B<Content> 

Content is a hashref that contains the binary octets to be encoded. This
hash is serialized, compressed (if specified) and encoded into ASCII with
MIME::Base64.  The result is the body of the encoded message.

=item B<Headers>

Headers is a hashref that contains ASCII headers that are placed at top of
the encoded message. Headers are encoded as RFC822 headers.

=item B<Compress>

A boolean parameter that forces armour() to compress the message body.

=back

=head2 B<unarmour()>

Decodes an armoured ASCII message into the hash provided as argument
to armour(). The hash contains Content, Object, and Headers.
unarmour() performs several consistency checks and returns a non-true
value on failure.

=head2 B<errstr()>

Returns the error message set by unarmour() on failure.

=head1 AUTHOR

Vipul Ved Prakash, E<lt>mail@vipul.netE<gt>

=head1 LICENSE

Copyright (c) 2001, Vipul Ved Prakash. All rights reserved. This code is
free software; you can redistribute it and/or modify it under the same
terms as Perl itself.

=head1 SEE ALSO

MIME::Base64(3), Compress::Zlib(3), Digest::MD5(3)

=cut
