package Syntax::Feature::Junction;

use strict;
use warnings;

our $VERSION = '0.003008'; # VERSION

# ABSTRACT: Provide keywords for any, all, none, or one

require Syntax::Keyword::Junction;

sub install {
  my ($class, %args) = @_;

  my $target  = $args{into};
  my $options = $args{options} || {};

  Syntax::Keyword::Junction->import({ into => $target }, %$options );

  return 1;
}

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Syntax::Feature::Junction - Provide keywords for any, all, none, or one

=head1 VERSION

version 0.003008

=head1 SYNOPSIS

  use syntax 'junction';

  if (any(@grant) eq 'su') {
    ...
  }

  if (all($foo, $bar) >= 10) {
    ...
  }

  if (qr/^\d+$/ == all(@answers)) {
    ...
  }

  if (all(@input) <= @limits) {
    ...
  }

  if (none(@pass) eq 'password') {
    ...
  }

  if (one(@answer) == 42) {
    ...
  }

or if you want to rename an export, use L<Sub::Exporter> options:

  use syntax 'junction' => {
    any => { -as => 'robot_any' }
  };

  if (robot_any(@grant) eq 'su') {
    ...
  }

The full documentation for this module is in L<Syntax::Keyword::Junction>.  This
is just a way to use the sugar that L<syntax> gives us.

=head1 AUTHORS

=over 4

=item *

Arthur Axel "fREW" Schmidt <frioux+cpan@gmail.com>

=item *

Carl Franks

=back

=head1 COPYRIGHT AND LICENSE

This software is copyright (c) 2014 by Arthur Axel "fREW" Schmidt.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut
