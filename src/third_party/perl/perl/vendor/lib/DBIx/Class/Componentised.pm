package # hide from PAUSE
    DBIx::Class::Componentised;

use strict;
use warnings;

use base 'Class::C3::Componentised';
use mro 'c3';

use DBIx::Class::Carp '^DBIx::Class|^Class::C3::Componentised';
use namespace::clean;

# this warns of subtle bugs introduced by UTF8Columns hacky handling of store_column
# if and only if it is placed before something overriding store_column
sub inject_base {
  my $class = shift;
  my ($target, @complist) = @_;

  # we already did load the component
  my $keep_checking = ! (
    $target->isa ('DBIx::Class::UTF8Columns')
      ||
    $target->isa ('DBIx::Class::ForceUTF8')
  );

  my @target_isa;

  while ($keep_checking && @complist) {

    @target_isa = do { no strict 'refs'; @{"$target\::ISA"} }
      unless @target_isa;

    my $comp = pop @complist;

    # warn here on use of either component, as we have no access to ForceUTF8,
    # the author does not respond, and the Catalyst wiki used to recommend it
    for (qw/DBIx::Class::UTF8Columns DBIx::Class::ForceUTF8/) {
      if ($comp->isa ($_) ) {
        $keep_checking = 0; # no use to check from this point on
        carp_once "Use of $_ is strongly discouraged. See documentation of DBIx::Class::UTF8Columns for more info\n"
          unless $ENV{DBIC_UTF8COLUMNS_OK};
        last;
      }
    }

    # something unset $keep_checking - we got a unicode mangler
    if (! $keep_checking) {

      my $base_store_column = do { require DBIx::Class::Row; DBIx::Class::Row->can ('store_column') };

      my @broken;
      for my $existing_comp (@target_isa) {
        my $sc = $existing_comp->can ('store_column')
          or next;

        if ($sc ne $base_store_column) {
          require B;
          my $definer = B::svref_2object($sc)->STASH->NAME;
          push @broken, ($definer eq $existing_comp)
            ? $existing_comp
            : "$existing_comp (via $definer)"
          ;
        }
      }

      carp "Incorrect loading order of $comp by $target will affect other components overriding 'store_column' ("
          . join (', ', @broken)
          .'). Refer to the documentation of DBIx::Class::UTF8Columns for more info'
        if @broken;
    }

    unshift @target_isa, $comp;
  }

  $class->next::method(@_);
}

1;
