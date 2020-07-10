package Tie::Array::CSV::HoldRow;

use strict;
use warnings;

use Carp;

use Tie::File;
use Text::CSV;

use Scalar::Util qw/weaken/;

use Tie::Array::CSV;
our @ISA = ('Tie::Array::CSV');

# This is essentially the same TIEARRAY method as T::A::CSV, 
# but initializes active_rows. This isn't strictly necessary, thanks to autoviv
sub TIEARRAY {
  my $class = shift;

  my $self = $class->SUPER::TIEARRAY(@_);

  $self->{active_rows} = {},

  # rebless
  bless $self, $class;

  return $self;
}

sub FETCH {
  my $self = shift;
  my $index = shift;

  if ($self->{active_rows}{$index}) {
    return $self->{active_rows}{$index}
  }

  my $line_array = $self->SUPER::FETCH($index);

  weaken(
    $self->{active_rows}{$index} = $line_array
  );

  return $line_array;
}

sub STORE {
  my $self = shift;
  my ($index, $value) = @_;

  $self->{file}[$index] = $self->_combine($value);
}

sub SPLICE {
  my $self = shift;
  my $size = $self->FETCHSIZE;
  my $offset = @_ ? shift : 0;
  $offset += $size if $offset < 0;
  my $length = @_ ? shift : $size-$offset;

  my @replace_rows = map { $self->_combine($_) } @_;

  ## reindex active_rows ##

  # assuming removing items
  my @active_rows = 
    sort { $a <=> $b } 
    grep { defined $self->{active_rows}{$_} }
    keys %{ $self->{active_rows} };
  my $delta = @replace_rows - $length;

  # if instead adding items
  if ($length < @replace_rows) {
    # reverse ot avoid overwriting active items
    @active_rows = reverse @active_rows;
    $delta = @replace_rows + $length;
  }

  foreach my $index (@active_rows) {
    # skip lines before those affected
    next if ($index < $offset);

    if ($index >= $offset and $index < ($offset + $length)) { #items that are being removed
      tied(@{$self->{active_rows}{$index}})->{line_num} = undef;
    } else { #shifting affected items
      tied(@{$self->{active_rows}{$index}})->{line_num} = $index+$delta;
      $self->{active_rows}{$index+$delta} = delete $self->{active_rows}{$index}; 
    }
  }

  ## end reindexing logic ##

  my @return = map { $self->_parse($_) }
    splice(@{ $self->{file} },$offset,$length,@replace_rows);

  return @return

}

sub SHIFT {
  my $self = shift;
  my ($return) = $self->SPLICE(0,1);
  return $return;
}

sub UNSHIFT { scalar shift->SPLICE(0,0,@_) }

sub PUSH {
  my $self = shift;
  my $i = $self->FETCHSIZE;
  $self->STORE($i++, shift) while (@_);
}

sub POP {
  my $self = shift;
  my $newsize = $self->FETCHSIZE - 1;
  my $val;
  if ($newsize >= 0) {
    $val = $self->FETCH($newsize);
    $self->STORESIZE($newsize);
  }
  return $val;
}

sub CLEAR { shift->STORESIZE(0) }

sub EXTEND  { }

package Tie::Array::CSV::HoldRow::Row;

use Carp;

use Tie::Array::CSV;
our @ISA = ('Tie::Array::CSV::Row');

sub TIEARRAY {
  my $class = shift;
  my $self = $class->SUPER::TIEARRAY(@_);

  # rebless
  bless $self, $class;

  $self->{need_update} = 0;

  return $self;
}

# _update now marks for deferred update
sub _update {
  my $self = shift;
  $self->{need_update} = 1;
}

sub _deferred_update {
  my $self = shift;
  unless (defined $self->{line_num}) {
    carp "Attempted to write out from a severed row";
    return undef;
  }

  $self->SUPER::_update();
}

sub DESTROY {
  my $self = shift;
  $self->_deferred_update if $self->{need_update} == 1;
}

__END__
__POD__

=head1 NAME

Tie::Array::CSV::HoldRow - A Tie::Array::CSV subclass for deferring row operations

=head1 SYNOPSIS

 use strict; use warnings;
 use Tie::Array::CSV::HoldRow;
 tie my @file, 'Tie::Array::CSV::HoldRow', 'filename';

 print $file[0][2];
 $file[3][5] = "Camel";

=head1 DESCRIPTION

This module is an experimental subclass of L<Tie::Array::CSV>, see usage information in that documentation. 

While the usage is the same, the timing of the file IO is different. As opposed to the base module, the file is not updated while the reference to the row is still in scope. Note that for both modules, the parsed row is still held in memory while the row is in scope, the ONLY difference is that the file reflects changes immediately when C<hold_row> is false. To reiterate, this option only affects file IO, not memory usage.

When multiple rows are kept alive/removed/modified there was the possibility that conflicting directives could be given to a single physical line. To combat this possibility, as of version 0.05, all (living) child row objects are made aware of line number changes in the parent (outer array) should these occur. Futher if a row object is alive, but the parent object removes that line, the row object is remains intact, but the links between the row object and parent/file are severed.

=head1 SOURCE REPOSITORY

L<http://github.com/jberger/Tie-Array-CSV>

=head1 AUTHOR

Joel Berger, E<lt>joel.a.berger@gmail.comE<gt>

=head1 COPYRIGHT AND LICENSE

Copyright (C) 2013 by Joel Berger

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself.

=cut

