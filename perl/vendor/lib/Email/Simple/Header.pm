use strict;
use warnings;
package Email::Simple::Header;
# ABSTRACT: the header of an Email::Simple message
$Email::Simple::Header::VERSION = '2.216';
use Carp ();

require Email::Simple;

#pod =head1 SYNOPSIS
#pod
#pod   my $email = Email::Simple->new($text);
#pod
#pod   my $header = $email->header_obj;
#pod   print $header->as_string;
#pod
#pod =head1 DESCRIPTION
#pod
#pod This method implements the headers of an Email::Simple object.  It is a very
#pod minimal interface, and is mostly for private consumption at the moment.
#pod
#pod =method new
#pod
#pod   my $header = Email::Simple::Header->new($head, \%arg);
#pod
#pod C<$head> is a string containing a valid email header, or a reference to such a
#pod string.  If a reference is passed in, don't expect that it won't be altered.
#pod
#pod Valid arguments are:
#pod
#pod   crlf - the header's newline; defaults to CRLF
#pod
#pod =cut

# We need to be able to:
#   * get all values by lc name
#   * produce all pairs, with case intact

sub new {
  my ($class, $head, $arg) = @_;

  my $head_ref = ref $head ? $head : \$head;

  my $self = { mycrlf => $arg->{crlf} || "\x0d\x0a", };

  my $headers = $class->_header_to_list($head_ref, $self->{mycrlf});

  #  for my $header (@$headers) {
  #    push @{ $self->{order} }, $header->[0];
  #    push @{ $self->{head}{ $header->[0] } }, $header->[1];
  #  }
  #
  #  $self->{header_names} = { map { lc $_ => $_ } keys %{ $self->{head} } };
  $self->{headers} = $headers;

  bless $self => $class;
}

sub _header_to_list {
  my ($self, $head, $mycrlf) = @_;

  my @headers;

  my $crlf = Email::Simple->__crlf_re;

  while ($$head =~ m/\G(.+?)$crlf/go) {
    local $_ = $1;

    if (/^\s+/ or not /^([^:]+):\s*(.*)/) {
      # This is a continuation line. We fold it onto the end of
      # the previous header.
      next if !@headers;  # Well, that sucks.  We're continuing nothing?

      (my $trimmed = $_) =~ s/^\s+//;
      $headers[-1][0] .= $headers[-1][0] =~ /\S/ ? " $trimmed" : $trimmed;
      $headers[-1][1] .= "$mycrlf$_";
    } else {
      push @headers, $1, [ $2, $_ ];
    }
  }

  return \@headers;
}

#pod =method as_string
#pod
#pod   my $string = $header->as_string(\%arg);
#pod
#pod This returns a stringified version of the header.
#pod
#pod =cut

# RFC 2822, 3.6:
# ...for the purposes of this standard, header fields SHOULD NOT be reordered
# when a message is transported or transformed.  More importantly, the trace
# header fields and resent header fields MUST NOT be reordered, and SHOULD be
# kept in blocks prepended to the message.

sub as_string {
  my ($self, $arg) = @_;
  $arg ||= {};

  my $header_str = '';

  my $headers = $self->{headers};

  my $fold_arg = {
    # at     => (exists $arg->{fold_at} ? $arg->{fold_at} : $self->default_fold_at),
    # indent => (exists $arg->{fold_indent} ? $arg->{fold_indent} : $self->default_fold_indent),
    at     => $self->_default_fold_at,
    indent => $self->_default_fold_indent,
  };

  for (my $i = 0; $i < @$headers; $i += 2) {
    if (ref $headers->[ $i + 1 ]) {
      $header_str .= $headers->[ $i + 1 ][1] . $self->crlf;
    } else {
      my $header = "$headers->[$i]: $headers->[$i + 1]";

      $header_str .= $self->_fold($header, $fold_arg);
    }
  }

  return $header_str;
}

#pod =method header_names
#pod
#pod This method returns a list of the unique header names found in this header, in
#pod no particular order.
#pod
#pod =cut

sub header_names {
  my $headers = $_[0]->{headers};

  my %seen;
  grep  { !$seen{ lc $_ }++ }
    map { $headers->[ $_ * 2 ] } 0 .. @$headers / 2 - 1;
}

#pod =method header_raw_pairs
#pod
#pod   my @pairs = $header->header_raw_pairs;
#pod   my $first_name  = $pairs[0];
#pod   my $first_value = $pairs[1];
#pod
#pod This method returns a list of all the field/value pairs in the header, in the
#pod order that they appear in the header.  (Remember: don't try assigning that to a
#pod hash.  Some fields may appear more than once!)
#pod
#pod =method header_pairs
#pod
#pod L<header_pairs> is another name for L<header_raw_pairs>, which was the original
#pod name for the method and which you'll see most often.  In general, though, it's
#pod better to be explicit and use L<header_raw_pairs>.  (In Email::MIME,
#pod L<header_str_pairs> exists for letting the library do the header decoding for
#pod you.)
#pod
#pod =cut

sub header_raw_pairs {
  my ($self) = @_;

  my @pairs = map {; _str_value($_) } @{ $self->{headers} };

  return @pairs;
}

sub header_pairs {
  my ($self) = @_;
  $self->header_raw_pairs;
}

#pod =method header_raw
#pod
#pod   my $first_value = $header->header_raw($field);
#pod   my $nth_value   = $header->header_raw($field, $index);
#pod   my @all_values  = $header->header_raw($field);
#pod
#pod This method returns the value or values of the given header field.  If the
#pod named field does not appear in the header, this method returns false.
#pod
#pod =method header
#pod
#pod This method just calls C<header_raw>.  It's the older name for C<header_raw>,
#pod but it can be a problem because L<Email::MIME>, a subclass of Email::Simple,
#pod makes C<header> return the header's decoded value.
#pod
#pod =cut

sub _str_value { return ref $_[0] ? $_[0][0] : $_[0] }

sub header_raw {
  my ($self, $field, $index) = @_;

  my $headers  = $self->{headers};
  my $lc_field = lc $field;

  if (wantarray and not defined $index) {
    return map { _str_value($headers->[ $_ * 2 + 1 ]) }
      grep { lc $headers->[ $_ * 2 ] eq $lc_field } 0 .. @$headers / 2 - 1;
  } else {
    $index = 0 unless defined $index;
    my $max = @$headers / 2 - 1;
    my @indexes = $index >= 0 ? (0 .. $max) : reverse(0 .. $max);
    $index = -1-$index if $index < 0;
    for (@indexes) {
      next unless lc $headers->[ $_ * 2 ] eq $lc_field;
      return _str_value($headers->[ $_ * 2 + 1 ]) if $index-- == 0;
    }
    return undef;
  }
}

*header = \&header_raw;

#pod =method header_raw_set
#pod
#pod   $header->header_raw_set($field => @values);
#pod
#pod This method updates the value of the given header.  Existing headers have their
#pod values set in place.  Additional headers are added at the end.  If no values
#pod are given to set, the header will be removed from to the message entirely.
#pod
#pod =method header_set
#pod
#pod L<header_set> is another name for L<header_raw_set>, which was the original
#pod name for the method and which you'll see most often.  In general, though, it's
#pod better to be explicit and use L<header_raw_set>.  (In Email::MIME,
#pod L<header_str_set> exists for letting the library do the header encoding for
#pod you.)
#pod
#pod =cut

# Header fields are lines composed of a field name, followed by a colon (":"),
# followed by a field body, and terminated by CRLF.  A field name MUST be
# composed of printable US-ASCII characters (i.e., characters that have values
# between 33 and 126, inclusive), except colon.  A field body may be composed
# of any US-ASCII characters, except for CR and LF.

# However, a field body may contain CRLF when used in header "folding" and
# "unfolding" as described in section 2.2.3.

sub header_raw_set {
  my ($self, $field, @data) = @_;

  # I hate this block. -- rjbs, 2006-10-06
  if ($Email::Simple::GROUCHY) {
    Carp::croak "field name contains illegal characters"
      unless $field =~ /^[\x21-\x39\x3b-\x7e]+$/;
    Carp::carp "field name is not limited to hyphens and alphanumerics"
      unless $field =~ /^[\w-]+$/;
  }

  my $headers = $self->{headers};

  my $lc_field = lc $field;
  my @indices = grep { lc $headers->[$_] eq $lc_field }
    map { $_ * 2 } 0 .. @$headers / 2 - 1;

  if (@indices > @data) {
    my $overage = @indices - @data;
    splice @{$headers}, $_, 2 for reverse @indices[ -$overage .. -1 ];
    pop @indices for (1 .. $overage);
  } elsif (@data > @indices) {
    my $underage = @data - @indices;
    for (1 .. $underage) {
      push @$headers, $field, undef;  # temporary value
      push @indices, $#$headers - 1;
    }
  }

  for (0 .. $#indices) {
    $headers->[ $indices[$_] + 1 ] = $data[$_];
  }

  return wantarray ? @data : $data[0];
}

sub header_set {
  my ($self, $field, @data) = @_;
  $self->header_raw_set($field, @data);
}

#pod =method header_raw_prepend
#pod
#pod   $header->header_raw_prepend($field => $value);
#pod
#pod This method adds a new instance of the name field as the first field in the
#pod header.
#pod
#pod =cut

sub header_raw_prepend {
  my ($self, $field, $value) = @_;

  Carp::confess("tried to prepend raw header with undefined field name")
    unless defined $field;

  Carp::confess(qq{tried to prepend raw header "$field" with undefined value})
    unless defined $value;

  unshift @{ $self->{headers} }, $field => $value;

  return;
}

#pod =method crlf
#pod
#pod This method returns the newline string used in the header.
#pod
#pod =cut

sub crlf { $_[0]->{mycrlf} }

# =method fold
#
#   my $folded = $header->fold($line, \%arg);
#
# Given a header string, this method returns a folded version, if the string is
# long enough to warrant folding.  This method is used internally.
#
# Valid arguments are:
#
#   at      - fold lines to be no longer than this length, if possible
#             if given and false, never fold headers
#   indent  - indent lines with this string

# =cut

sub _fold {
  my ($self, $line, $arg) = @_;
  $arg ||= {};

  $arg->{at} = $self->_default_fold_at unless exists $arg->{at};

  $arg->{indent} = $self->_default_fold_indent unless exists $arg->{indent};

  my $indent = $arg->{indent} || $self->_default_fold_indent;

  # We will not folder headers if...
  # * the header has vertical whitespace
  # * all vertical whitespace is followed by horizontal whitespace or END
  if ($line =~ /\n/) {
    if ($line =~ s/\n([^\s\t])/\n$indent$1/g) {
      Carp::carp("bad space in header: newline followed by non-space: $line");
    } else {
      $line .= $self->crlf unless $line =~ /\n$/;
      return $line;
    }
  }

  return $line . $self->crlf unless $arg->{at} and $arg->{at} > 0;

  my $limit  = ($arg->{at} || $self->_default_fold_at) - 1;

  return $line . $self->crlf if length $line <= $limit;

  return $self->__fold_objless($line, $limit, $indent, $self->crlf);

}

sub __fold_objless {
  my ($self, $line, $limit, $indent, $crlf) = @_;

  # We know it will not contain any new lines at present
  my $folded = "";
  while (length $line) {
    if ($line =~ s/^(.{0,$limit})(\s|\z)//) {
      $folded .= $1 . $crlf;
      $folded .= $indent if length $line;
    } else {
      # Basically nothing we can do. :(
      $folded .= $line . $crlf;
      last;
    }
  }

  return $folded;
}

# =method default_fold_at
#
# This method (provided for subclassing) returns the default length at which to
# try to fold header lines.  The default default is 78.
#
# =cut

sub _default_fold_at { 78 }

# =method default_fold_indent
#
# This method (provided for subclassing) returns the default string used to
# indent folded headers.  The default default is a single space.
#
# =cut

sub _default_fold_indent { " " }

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Email::Simple::Header - the header of an Email::Simple message

=head1 VERSION

version 2.216

=head1 SYNOPSIS

  my $email = Email::Simple->new($text);

  my $header = $email->header_obj;
  print $header->as_string;

=head1 DESCRIPTION

This method implements the headers of an Email::Simple object.  It is a very
minimal interface, and is mostly for private consumption at the moment.

=head1 METHODS

=head2 new

  my $header = Email::Simple::Header->new($head, \%arg);

C<$head> is a string containing a valid email header, or a reference to such a
string.  If a reference is passed in, don't expect that it won't be altered.

Valid arguments are:

  crlf - the header's newline; defaults to CRLF

=head2 as_string

  my $string = $header->as_string(\%arg);

This returns a stringified version of the header.

=head2 header_names

This method returns a list of the unique header names found in this header, in
no particular order.

=head2 header_raw_pairs

  my @pairs = $header->header_raw_pairs;
  my $first_name  = $pairs[0];
  my $first_value = $pairs[1];

This method returns a list of all the field/value pairs in the header, in the
order that they appear in the header.  (Remember: don't try assigning that to a
hash.  Some fields may appear more than once!)

=head2 header_pairs

L<header_pairs> is another name for L<header_raw_pairs>, which was the original
name for the method and which you'll see most often.  In general, though, it's
better to be explicit and use L<header_raw_pairs>.  (In Email::MIME,
L<header_str_pairs> exists for letting the library do the header decoding for
you.)

=head2 header_raw

  my $first_value = $header->header_raw($field);
  my $nth_value   = $header->header_raw($field, $index);
  my @all_values  = $header->header_raw($field);

This method returns the value or values of the given header field.  If the
named field does not appear in the header, this method returns false.

=head2 header

This method just calls C<header_raw>.  It's the older name for C<header_raw>,
but it can be a problem because L<Email::MIME>, a subclass of Email::Simple,
makes C<header> return the header's decoded value.

=head2 header_raw_set

  $header->header_raw_set($field => @values);

This method updates the value of the given header.  Existing headers have their
values set in place.  Additional headers are added at the end.  If no values
are given to set, the header will be removed from to the message entirely.

=head2 header_set

L<header_set> is another name for L<header_raw_set>, which was the original
name for the method and which you'll see most often.  In general, though, it's
better to be explicit and use L<header_raw_set>.  (In Email::MIME,
L<header_str_set> exists for letting the library do the header encoding for
you.)

=head2 header_raw_prepend

  $header->header_raw_prepend($field => $value);

This method adds a new instance of the name field as the first field in the
header.

=head2 crlf

This method returns the newline string used in the header.

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
