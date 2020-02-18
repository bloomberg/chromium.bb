use 5.006;
use strict;
use warnings;

package Test::File::ShareDir::Utils;

our $VERSION = '1.001002';

our $AUTHORITY = 'cpan:KENTNL'; # AUTHORITY

# ABSTRACT: Simple utilities for File::ShareDir testing

use Exporter 5.57 qw(import);
use Carp qw( croak );

our @EXPORT_OK = qw( extract_dashes );
























sub extract_dashes {
  my ( $undashed_to, $source ) = @_;
  if ( not ref $source or 'HASH' ne ref $source ) {
    return croak(q[Must pass a hashref]);
  }
  my %input_config = %{$source};
  my $params       = {};
  for my $key ( keys %input_config ) {
    next unless $key =~ /\A-(.*)\z/msx;
    $params->{$1} = delete $input_config{$key};
  }
  $params->{$undashed_to} = {} if not exists $params->{$undashed_to};
  for my $key ( keys %input_config ) {
    $params->{$undashed_to}->{$key} = $input_config{$key};
  }
  return $params;
}

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Test::File::ShareDir::Utils - Simple utilities for File::ShareDir testing

=head1 VERSION

version 1.001002

=head1 SYNOPSIS

  use Test::File::ShareDir::Utils qw( extract_dashes );

  my $hash = extract_dashes('dists', $oldhash );

=head1 EXPORTABLE FUNCTIONS

=head2 extract_dashes

A utility that helps transform:

  -opt_a => bar
  -opt_b => baz
  NameA  => NameAValue
  NameB  => NameBValue

Into

  opt_a => bar
  opt_b => baz
  modules => {
    NameA => NameAValue
    NameB => NameBValue
  }

This is a useful approach used all over import and functional style interfaces due to explicit configuration
being needed only on rare occasions.

=head1 AUTHOR

Kent Fredric <kentnl@cpan.org>

=head1 COPYRIGHT AND LICENSE

This software is copyright (c) 2017 by Kent Fredric <kentnl@cpan.org>.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut
