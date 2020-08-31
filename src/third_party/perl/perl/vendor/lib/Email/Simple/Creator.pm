use strict;
use warnings;
package Email::Simple::Creator;
# ABSTRACT: private helper for building Email::Simple objects
$Email::Simple::Creator::VERSION = '2.216';
sub _crlf {
  "\x0d\x0a";
}

sub _date_header {
  require Email::Date::Format;
  Email::Date::Format::email_date();
}

our @CARP_NOT = qw(Email::Simple Email::MIME);

sub _add_to_header {
  my ($class, $header, $key, $value) = @_;
  $value = '' unless defined $value;

  if ($value =~ s/[\x0a\x0b\x0c\x0d\x85\x{2028}\x{2029}]+/ /g) {
    Carp::carp("replaced vertical whitespace in $key header with space; this will become fatal in a future version");
  }

  $$header .= Email::Simple::Header->__fold_objless("$key: $value", 78, q{ }, $class->_crlf);
}

sub _finalize_header {
  my ($class, $header) = @_;
  $$header .= $class->_crlf;
}

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Email::Simple::Creator - private helper for building Email::Simple objects

=head1 VERSION

version 2.216

=head1 AUTHORS

=over 4

=item *

Simon Cozens

=item *

Casey West

=item *

Ricardo SIGNES

=back

=head1 COPYRIGHT AND LICENSE

This software is copyright (c) 2003 by Simon Cozens.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut
