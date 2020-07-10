package # hide from PAUSE
    DBIx::Class::CDBICompat::Stringify;

use strict;
use warnings;

use Scalar::Util;

use overload
  '""' => sub { return shift->stringify_self; },
  fallback => 1;

sub stringify_self {
        my $self = shift;
        my @cols = $self->columns('Stringify');
        @cols = $self->primary_column unless @cols;
        my $ret = join "/", map { $self->get_column($_) || '' } @cols;
        return $ret || ref $self;
}

1;
