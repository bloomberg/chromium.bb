use strict;
use warnings;
package Email::MIME::ContentType;
# ABSTRACT: Parse a MIME Content-Type or Content-Disposition Header
$Email::MIME::ContentType::VERSION = '1.022';
use Carp;
use Encode 2.87 qw(find_mime_encoding);
use Exporter 5.57 'import';
our @EXPORT = qw(parse_content_type parse_content_disposition);

#pod =head1 SYNOPSIS
#pod
#pod   use Email::MIME::ContentType;
#pod
#pod   # Content-Type: text/plain; charset="us-ascii"; format=flowed
#pod   my $ct = 'text/plain; charset="us-ascii"; format=flowed';
#pod   my $data = parse_content_type($ct);
#pod
#pod   $data = {
#pod     type       => "text",
#pod     subtype    => "plain",
#pod     attributes => {
#pod       charset => "us-ascii",
#pod       format  => "flowed"
#pod     }
#pod   };
#pod
#pod
#pod   # Content-Type: application/x-stuff;
#pod   #  title*0*=us-ascii'en'This%20is%20even%20more%20;
#pod   #  title*1*=%2A%2A%2Afun%2A%2A%2A%20;
#pod   #  title*2="isn't it!"
#pod   my $ct = q(application/x-stuff;
#pod    title*0*=us-ascii'en'This%20is%20even%20more%20;
#pod    title*1*=%2A%2A%2Afun%2A%2A%2A%20;
#pod    title*2="isn't it!");
#pod   my $data = parse_content_type($ct);
#pod
#pod   $data = {
#pod     type       => "application",
#pod     subtype    => "x-stuff",
#pod     attributes => {
#pod       title => "This is even more ***fun*** isn't it!"
#pod     }
#pod   };
#pod
#pod   # Content-Disposition: attachment; filename=genome.jpeg;
#pod   #   modification-date="Wed, 12 Feb 1997 16:29:51 -0500"
#pod   my $cd = q(attachment; filename=genome.jpeg;
#pod     modification-date="Wed, 12 Feb 1997 16:29:51 -0500");
#pod   my $data = parse_content_disposition($cd);
#pod
#pod   $data = {
#pod     type       => "attachment",
#pod     attributes => {
#pod       filename            => "genome.jpeg",
#pod       "modification-date" => "Wed, 12 Feb 1997 16:29:51 -0500"
#pod     }
#pod   };
#pod
#pod =cut

our $STRICT_PARAMS = 1;

my $ct_default = 'text/plain; charset=us-ascii';

my $re_token = qr/[\x21\x23-\x27\x2A\x2B\x2D\x2E\x30-\x39\x41-\x5A\x5E-\x7E]+/; # US-ASCII except SPACE, CTLs and tspecials ()<>@,;:\\"/[]?=
my $re_token_non_strict = qr/([\x00-\x08\x0B\x0C\x0E-\x1F\x7E-\xFF]+|$re_token)/; # allow CTLs and above ASCII

my $re_qtext = qr/[\x01-\x08\x0B\x0C\x0E-\x1F\x21\x23-\x5B\x5D-\x7E\x7F]/; # US-ASCII except CR, LF, white space, backslash and quote
my $re_quoted_pair = qr/\\[\x00-\x7F]/;
my $re_quoted_string = qr/"((?:[ \t]*(?:$re_qtext|$re_quoted_pair))*[ \t]*)"/;

my $re_qtext_non_strict = qr/[\x80-\xFF]|$re_qtext/;
my $re_quoted_pair_non_strict = qr/\\[\x00-\xFF]/;
my $re_quoted_string_non_strict = qr/"((?:[ \t]*(?:$re_qtext_non_strict|$re_quoted_pair_non_strict))*[ \t]*)"/;

my $re_charset = qr/[!"#\$%&'+\-0-9A-Z\\\^_`a-z\{\|\}~]+/;
my $re_language = qr/[A-Za-z]{1,8}(?:-[0-9A-Za-z]{1,8})*/;
my $re_exvalue = qr/($re_charset)?'(?:$re_language)?'(.*)/;

sub parse_content_type {
    my $ct = shift;

    # If the header isn't there or is empty, give default answer.
    return parse_content_type($ct_default) unless defined $ct and length $ct;

    _unfold_lines($ct);
    _clean_comments($ct);

    # It is also recommend (sic.) that this default be assumed when a
    # syntactically invalid Content-Type header field is encountered.
    unless ($ct =~ s/^($re_token)\/($re_token)//) {
        unless ($STRICT_PARAMS and $ct =~ s/^($re_token_non_strict)\/($re_token_non_strict)//) {
            carp "Invalid Content-Type '$ct'";
            return parse_content_type($ct_default);
        }
    }

    my ($type, $subtype) = (lc $1, lc $2);

    _clean_comments($ct);
    $ct =~ s/\s+$//;

    my $attributes = {};
    if ($STRICT_PARAMS and length $ct and $ct !~ /^;/) {
        carp "Missing semicolon before first Content-Type parameter '$ct'";
    } else {
        $attributes = _process_rfc2231(_parse_attributes($ct));
    }

    return {
        type       => $type,
        subtype    => $subtype,
        attributes => $attributes,

        # This is dumb.  Really really dumb.  For backcompat. -- rjbs,
        # 2013-08-10
        discrete   => $type,
        composite  => $subtype,
    };
}

my $cd_default = 'attachment';

sub parse_content_disposition {
    my $cd = shift;

    return parse_content_disposition($cd_default) unless defined $cd and length $cd;

    _unfold_lines($cd);
    _clean_comments($cd);

    unless ($cd =~ s/^($re_token)//) {
        unless ($STRICT_PARAMS and $cd =~ s/^($re_token_non_strict)//) {
            carp "Invalid Content-Disposition '$cd'";
            return parse_content_disposition($cd_default);
        }
    }

    my $type = lc $1;

    _clean_comments($cd);
    $cd =~ s/\s+$//;

    my $attributes = {};
    if ($STRICT_PARAMS and length $cd and $cd !~ /^;/) {
        carp "Missing semicolon before first Content-Disposition parameter '$cd'";
    } else {
        $attributes = _process_rfc2231(_parse_attributes($cd));
    }

    return {
        type       => $type,
        attributes => $attributes,
    };
}

sub _unfold_lines {
    $_[0] =~ s/(?:\r\n|[\r\n])(?=[ \t])//g;
}

sub _clean_comments {
    my $ret = ($_[0] =~ s/^\s+//);
    while (length $_[0]) {
        last unless $_[0] =~ s/^\(//;
        my $level = 1;
        while (length $_[0]) {
            my $ch = substr $_[0], 0, 1, '';
            if ($ch eq '(') {
                $level++;
            } elsif ($ch eq ')') {
                $level--;
                last if $level == 0;
            } elsif ($ch eq '\\') {
                substr $_[0], 0, 1, '';
            }
        }
        carp "Unbalanced comment" if $level != 0 and $STRICT_PARAMS;
        $ret |= ($_[0] =~ s/^\s+//);
    }
    return $ret;
}

sub _process_rfc2231 {
    my ($attribs) = @_;
    my %cont;
    my %encoded;
    foreach (keys %{$attribs}) {
        next unless $_ =~ m/^(.*)\*(\d+)\*?$/;
        my ($attr, $sec) = ($1, $2);
        $cont{$attr}->[$sec] = $attribs->{$_};
        $encoded{$attr}->[$sec] = 1 if $_ =~ m/\*$/;
        delete $attribs->{$_};
    }
    foreach (keys %cont) {
        my $key = $_;
        $key .= '*' if $encoded{$_};
        $attribs->{$key} = join '', @{$cont{$_}};
    }
    foreach (keys %{$attribs}) {
        next unless $_ =~ m/^(.*)\*$/;
        my $key = $1;
        next unless $attribs->{$_} =~ m/^$re_exvalue$/;
        my ($charset, $value) = ($1, $2);
        $value =~ s/%([0-9A-Fa-f]{2})/pack('C', hex($1))/eg;
        if (length $charset) {
            my $enc = find_mime_encoding($charset);
            if (defined $enc) {
                $value = $enc->decode($value);
            } else {
                carp "Unknown charset '$charset' in attribute '$key' value";
            }
        }
        $attribs->{$key} = $value;
        delete $attribs->{$_};
    }
    return $attribs;
}

sub _parse_attributes {
    local $_ = shift;
    substr($_, 0, 0, '; ') if length $_ and $_ !~ /^;/;
    my $attribs = {};
    while (length $_) {
        s/^;// or $STRICT_PARAMS and do {
            carp "Missing semicolon before parameter '$_'";
            return $attribs;
        };
        _clean_comments($_);
        unless (length $_) {
            # Some mail software generates a Content-Type like this:
            # "Content-Type: text/plain;"
            # RFC 1521 section 3 says a parameter must exist if there is a
            # semicolon.
            carp "Extra semicolon after last parameter" if $STRICT_PARAMS;
            return $attribs;
        }
        my $attribute;
        if (s/^($re_token)=//) {
            $attribute = lc $1;
        } else {
            if ($STRICT_PARAMS) {
                carp "Illegal parameter '$_'";
                return $attribs;
            }
            if (s/^($re_token_non_strict)=//) {
                $attribute = lc $1;
            } else {
                unless (s/^([^;=\s]+)\s*=//) {
                    carp "Cannot parse parameter '$_'";
                    return $attribs;
                }
                $attribute = lc $1;
            }
        }
        _clean_comments($_);
        my $value = _extract_attribute_value();
        $attribs->{$attribute} = $value;
        _clean_comments($_);
    }
    return $attribs;
}

sub _extract_attribute_value { # EXPECTS AND MODIFIES $_
    my $value;
    while (length $_) {
        if (s/^($re_token)//) {
            $value .= $1;
        } elsif (s/^$re_quoted_string//) {
            my $sub = $1;
            $sub =~ s/\\(.)/$1/g;
            $value .= $sub;
        } elsif ($STRICT_PARAMS) {
            my $char = substr $_, 0, 1;
            carp "Unquoted '$char' not allowed";
            return;
        } elsif (s/^($re_token_non_strict)//) {
            $value .= $1;
        } elsif (s/^$re_quoted_string_non_strict//) {
            my $sub = $1;
            $sub =~ s/\\(.)/$1/g;
            $value .= $sub;
        }
        my $erased = _clean_comments($_);
        last if !length $_ or /^;/;
        if ($STRICT_PARAMS) {
            my $char = substr $_, 0, 1;
            carp "Extra '$char' found after parameter";
            return;
        }
        if ($erased) {
            # Sometimes semicolon is missing, so check for = char
            last if m/^$re_token_non_strict=/;
            $value .= ' ';
        }
        $value .= substr $_, 0, 1, '';
    }
    return $value;
}

1;

#pod =func parse_content_type
#pod
#pod This routine is exported by default.
#pod
#pod This routine parses email content type headers according to section 5.1 of RFC
#pod 2045 and also RFC 2231 (Character Set and Parameter Continuations).  It returns
#pod a hash as above, with entries for the C<type>, the C<subtype>, and a hash of
#pod C<attributes>.
#pod
#pod For backward compatibility with a really unfortunate misunderstanding of RFC
#pod 2045 by the early implementors of this module, C<discrete> and C<composite> are
#pod also present in the returned hashref, with the values of C<type> and C<subtype>
#pod respectively.
#pod
#pod =func parse_content_disposition
#pod
#pod This routine is exported by default.
#pod
#pod This routine parses email Content-Disposition headers according to RFC 2183 and
#pod RFC 2231.  It returns a hash as above, with entries for the C<type>, and a hash
#pod of C<attributes>.
#pod
#pod =head1 WARNINGS
#pod
#pod This is not a valid content-type header, according to both RFC 1521 and RFC
#pod 2045:
#pod
#pod   Content-Type: type/subtype;
#pod
#pod If a semicolon appears, a parameter must.  C<parse_content_type> will carp if
#pod it encounters a header of this type, but you can suppress this by setting
#pod C<$Email::MIME::ContentType::STRICT_PARAMS> to a false value.  Please consider
#pod localizing this assignment!
#pod
#pod Same applies for C<parse_content_disposition>.
#pod
#pod =cut

__END__

=pod

=encoding UTF-8

=head1 NAME

Email::MIME::ContentType - Parse a MIME Content-Type or Content-Disposition Header

=head1 VERSION

version 1.022

=head1 SYNOPSIS

  use Email::MIME::ContentType;

  # Content-Type: text/plain; charset="us-ascii"; format=flowed
  my $ct = 'text/plain; charset="us-ascii"; format=flowed';
  my $data = parse_content_type($ct);

  $data = {
    type       => "text",
    subtype    => "plain",
    attributes => {
      charset => "us-ascii",
      format  => "flowed"
    }
  };


  # Content-Type: application/x-stuff;
  #  title*0*=us-ascii'en'This%20is%20even%20more%20;
  #  title*1*=%2A%2A%2Afun%2A%2A%2A%20;
  #  title*2="isn't it!"
  my $ct = q(application/x-stuff;
   title*0*=us-ascii'en'This%20is%20even%20more%20;
   title*1*=%2A%2A%2Afun%2A%2A%2A%20;
   title*2="isn't it!");
  my $data = parse_content_type($ct);

  $data = {
    type       => "application",
    subtype    => "x-stuff",
    attributes => {
      title => "This is even more ***fun*** isn't it!"
    }
  };

  # Content-Disposition: attachment; filename=genome.jpeg;
  #   modification-date="Wed, 12 Feb 1997 16:29:51 -0500"
  my $cd = q(attachment; filename=genome.jpeg;
    modification-date="Wed, 12 Feb 1997 16:29:51 -0500");
  my $data = parse_content_disposition($cd);

  $data = {
    type       => "attachment",
    attributes => {
      filename            => "genome.jpeg",
      "modification-date" => "Wed, 12 Feb 1997 16:29:51 -0500"
    }
  };

=head1 FUNCTIONS

=head2 parse_content_type

This routine is exported by default.

This routine parses email content type headers according to section 5.1 of RFC
2045 and also RFC 2231 (Character Set and Parameter Continuations).  It returns
a hash as above, with entries for the C<type>, the C<subtype>, and a hash of
C<attributes>.

For backward compatibility with a really unfortunate misunderstanding of RFC
2045 by the early implementors of this module, C<discrete> and C<composite> are
also present in the returned hashref, with the values of C<type> and C<subtype>
respectively.

=head2 parse_content_disposition

This routine is exported by default.

This routine parses email Content-Disposition headers according to RFC 2183 and
RFC 2231.  It returns a hash as above, with entries for the C<type>, and a hash
of C<attributes>.

=head1 WARNINGS

This is not a valid content-type header, according to both RFC 1521 and RFC
2045:

  Content-Type: type/subtype;

If a semicolon appears, a parameter must.  C<parse_content_type> will carp if
it encounters a header of this type, but you can suppress this by setting
C<$Email::MIME::ContentType::STRICT_PARAMS> to a false value.  Please consider
localizing this assignment!

Same applies for C<parse_content_disposition>.

=head1 AUTHORS

=over 4

=item *

Simon Cozens <simon@cpan.org>

=item *

Casey West <casey@geeknest.com>

=item *

Ricardo SIGNES <rjbs@cpan.org>

=back

=head1 CONTRIBUTORS

=for stopwords Matthew Green Pali Thomas Szukala

=over 4

=item *

Matthew Green <mrg@eterna.com.au>

=item *

Pali <pali@cpan.org>

=item *

Thomas Szukala <ts@abusix.com>

=back

=head1 COPYRIGHT AND LICENSE

This software is copyright (c) 2004 by Simon Cozens.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut
