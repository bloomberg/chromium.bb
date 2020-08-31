package DBM::Deep::Null;

use 5.008_004;

use strict;
use warnings FATAL => 'all';

=head1 NAME

DBM::Deep::Null - NULL object

=head1 PURPOSE

This is an internal-use-only object for L<DBM::Deep>. It acts as a NULL object
in the same vein as MARCEL's L<Class::Null>. I couldn't use L<Class::Null>
because DBM::Deep needed an object that always evaluated as undef, not an
implementation of the Null Class pattern.

=head1 OVERVIEW

It is used to represent null sectors in DBM::Deep.

=cut

use overload
    'bool'   => sub { undef },
    '""'     => sub { undef },
    '0+'     => sub { 0 },
   ('cmp'    => 
    '<=>'    => sub {
                  return 0 if !defined $_[1] || !length $_[1];
                  return $_[2] ? 1 : -1;
                }
   )[0,2,1,2], # same sub for both ops
    '%{}'    => sub {
                  require Carp;
                  Carp::croak("Can't use a stale reference as a HASH");
                },
    '@{}'    => sub {
                  require Carp;
                  Carp::croak("Can't use a stale reference as an ARRAY");
                },
    fallback => 1,
    nomethod => 'AUTOLOAD';

sub AUTOLOAD { return; }

1;
__END__
