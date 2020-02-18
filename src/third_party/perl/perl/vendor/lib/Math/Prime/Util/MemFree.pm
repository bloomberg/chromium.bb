package Math::Prime::Util::MemFree;
use strict;
use warnings;

BEGIN {
  $Math::Prime::Util::MemFree::AUTHORITY = 'cpan:DANAJ';
  $Math::Prime::Util::MemFree::VERSION = '0.73';
}

use base qw( Exporter );
our @EXPORT_OK = qw( );
our %EXPORT_TAGS = (all => [ @EXPORT_OK ]);


use Math::Prime::Util;
use Carp qw/carp croak confess/;

my $memfree_instances = 0;
sub new {
  my $self = bless {}, shift;
  $memfree_instances++;
  return $self;
}
sub DESTROY {
  my $self = shift;
  confess "instances count mismatch" unless $memfree_instances > 0;
  Math::Prime::Util::prime_memfree if --$memfree_instances == 0;
  return;
}

1;

__END__


# ABSTRACT: An auto-free object for Math::Prime::Util

=pod

=head1 NAME

Math::Prime::Util::MemFree - An auto-free object for Math::Prime::Util


=head1 VERSION

Version 0.73


=head1 SYNOPSIS

  use Math::Prime::Util;

  {
    my $mf = Math::Prime::Util::MemFree->new;
    # ... do things with Math::Prime::Util ...
  }
  # When the last object leaves scope, prime_memfree is called.


=head1 DESCRIPTION

This is a more robust way of making sure any cached memory is freed, as it
will be handled by the last C<MemFree> object leaving scope.  This means if
your routines were inside an eval that died, things will still get cleaned up.
If you call another function that uses a MemFree object, the cache will stay
in place because you still have an object.


=head1 FUNCTIONS

=head2 new

Creates a new auto-free object.  This object has no methods and has no data.
When it leaves scope it will call C<prime_memfree>, thereby releasing any
extra memory that the L<Math::Prime::Util> module may have allocated.

Memory is not freed until the last object goes out of scope.  C<prime_memfree>
may always be called manually.  All memory is freed at C<END> time, so this is
mainly for long running programs that want extra control over memory use.


=head1 AUTHORS

Dana Jacobsen E<lt>dana@acm.orgE<gt>


=head1 COPYRIGHT

Copyright 2012 by Dana Jacobsen E<lt>dana@acm.orgE<gt>

This program is free software; you can redistribute it and/or modify it under the same terms as Perl itself.

=cut
