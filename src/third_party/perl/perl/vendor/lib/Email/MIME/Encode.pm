use strict;
use warnings;
package Email::MIME::Encode;
# ABSTRACT: a private helper for MIME header encoding
$Email::MIME::Encode::VERSION = '1.946';
use Carp ();
use Encode ();
use MIME::Base64();
use Module::Runtime ();
use Scalar::Util;

our @CARP_NOT;

my %no_mime_headers = map { $_ => undef } qw(date message-id in-reply-to references downgraded-message-id downgraded-in-reply-to downgraded-references);

sub maybe_mime_encode_header {
  my ($header, $val, $charset) = @_;

  $header = lc $header;

  my $header_name_length = length($header) + length(": ");

  if (Scalar::Util::blessed($val) && $val->can("as_mime_string")) {
    return $val->as_mime_string({
      charset => $charset,
      header_name_length => $header_name_length,
    });
  }

  return _object_encode($val, $charset, $header_name_length, $Email::MIME::Header::header_to_class_map{$header})
    if exists $Email::MIME::Header::header_to_class_map{$header};

  my $min_wrap_length = 78 - $header_name_length + 1;

  return $val
    unless _needs_mime_encode($val) || $val =~ /[^\s]{$min_wrap_length,}/;

  return $val
    if exists $no_mime_headers{$header};

  return mime_encode($val, $charset, $header_name_length);
}

sub _needs_mime_encode {
  my ($val) = @_;
  return defined $val && $val =~ /(?:\P{ASCII}|=\?|[^\s]{79,}|^\s+|\s+$)/s;
}

sub _needs_mime_encode_addr {
  my ($val) = @_;
  return _needs_mime_encode($val) || ( defined $val && $val =~ /[:;,]/ );
}

sub _object_encode {
  my ($val, $charset, $header_name_length, $class) = @_;

  local @CARP_NOT = qw(Email::MIME Email::MIME::Header);

  {
    local $@;
    Carp::croak("Cannot load package '$class': $@") unless eval { Module::Runtime::require_module($class) };
  }

  Carp::croak("Class '$class' does not have method 'from_string'") unless $class->can('from_string');

  my $object = $class->from_string(ref $val eq 'ARRAY' ? @{$val} : $val);

  Carp::croak("Object from class '$class' does not have method 'as_mime_string'") unless $object->can('as_mime_string');

  return $object->as_mime_string({
    charset => $charset,
    header_name_length => $header_name_length,
  });
}

# XXX this is copied directly out of Courriel::Header
# eventually, this should be extracted out into something that could be shared
sub mime_encode {
  my ($text, $charset, $header_name_length) = @_;

  $header_name_length = 0 unless defined $header_name_length;
  $charset = 'UTF-8' unless defined $charset;

  my $enc_obj = Encode::find_encoding($charset);

  my $head = '=?' . $enc_obj->mime_name() . '?B?';
  my $tail = '?=';

  my $mime_length = length($head) + length($tail);

  # This code is copied from Mail::Message::Field::Full in the Mail-Box
  # distro.
  my $real_length = int( ( 75 - $mime_length ) / 4 ) * 3;
  my $first_length = int( ( 75 - $header_name_length - $mime_length ) / 4 ) * 3;

  my @result;
  my $chunk = q{};
  my $first_processed = 0;
  while ( length( my $chr = substr( $text, 0, 1, '' ) ) ) {
    my $chr = $enc_obj->encode( $chr, 0 );

    if ( length($chunk) + length($chr) > ( $first_processed ? $real_length : $first_length ) ) {
      if ( length($chunk) > 0 ) {
        push @result, $head . MIME::Base64::encode_base64( $chunk, q{} ) . $tail;
        $chunk = q{};
      }
      $first_processed = 1
        unless $first_processed;
    }

    $chunk .= $chr;
  }

  push @result, $head . MIME::Base64::encode_base64( $chunk, q{} ) . $tail
    if length $chunk;

  return join q{ }, @result;
}

sub maybe_mime_decode_header {
  my ($header, $val) = @_;

  $header = lc $header;

  return _object_decode($val, $Email::MIME::Header::header_to_class_map{$header})
    if exists $Email::MIME::Header::header_to_class_map{$header};

  return $val
    if exists $no_mime_headers{$header};

  return $val
    unless $val =~ /=\?/;

  return mime_decode($val);
}

sub _object_decode {
  my ($string, $class) = @_;

  local @CARP_NOT = qw(Email::MIME Email::MIME::Header);

  {
    local $@;
    Carp::croak("Cannot load package '$class': $@") unless eval { Module::Runtime::require_module($class) };
  }

  Carp::croak("Class '$class' does not have method 'from_mime_string'") unless $class->can('from_mime_string');

  my $object = $class->from_mime_string($string);

  Carp::croak("Object from class '$class' does not have method 'as_string'") unless $object->can('as_string');

  return $object->as_string();
}

sub mime_decode {
  my ($text) = @_;
  return undef unless defined $text;

  # The eval is to cope with unknown encodings, like Latin-62, or other
  # nonsense that gets put in there by spammers and weirdos
  # -- rjbs, 2014-12-04
  local $@;
  my $result = eval { Encode::decode("MIME-Header", $text) };
  return defined $result ? $result : $text;
}

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Email::MIME::Encode - a private helper for MIME header encoding

=head1 VERSION

version 1.946

=head1 AUTHORS

=over 4

=item *

Ricardo SIGNES <rjbs@cpan.org>

=item *

Casey West <casey@geeknest.com>

=item *

Simon Cozens <simon@cpan.org>

=back

=head1 COPYRIGHT AND LICENSE

This software is copyright (c) 2004 by Simon Cozens and Casey West.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut
