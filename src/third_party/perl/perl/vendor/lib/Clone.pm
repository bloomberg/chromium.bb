# $Id: Clone.pm,v 0.31 2009/01/20 04:54:37 ray Exp $
package Clone;

use strict;
use Carp;
use vars qw($VERSION @ISA @EXPORT @EXPORT_OK $AUTOLOAD);

require Exporter;
require DynaLoader;
require AutoLoader;

@ISA = qw(Exporter DynaLoader);
# Items to export into callers namespace by default. Note: do not export
# names by default without a very good reason. Use EXPORT_OK instead.
# Do not simply export all your public functions/methods/constants.
@EXPORT = qw();
@EXPORT_OK = qw( clone );

$VERSION = '0.31';

bootstrap Clone $VERSION;

# Preloaded methods go here.

# Autoload methods go after =cut, and are processed by the autosplit program.

1;
__END__

=head1 NAME

Clone - recursively copy Perl datatypes

=head1 SYNOPSIS

  use Clone;
  
  push @Foo::ISA, 'Clone';

  $a = new Foo;
  $b = $a->clone();
  
  # or

  use Clone qw(clone);
  
  $a = { 'foo' => 'bar', 'move' => 'zig' };
  $b = [ 'alpha', 'beta', 'gamma', 'vlissides' ];
  $c = new Foo();

  $d = clone($a);
  $e = clone($b);
  $f = clone($c);

=head1 DESCRIPTION

This module provides a clone() method which makes recursive
copies of nested hash, array, scalar and reference types, 
including tied variables and objects.


clone() takes a scalar argument and an optional parameter that 
can be used to limit the depth of the copy. To duplicate lists,
arrays or hashes, pass them in by reference. e.g.
    
    my $copy = clone (\@array);

    # or

    my %copy = %{ clone (\%hash) };
    

For a slower, but more flexible solution see Storable's dclone().

=head1 AUTHOR

Ray Finch, rdf@cpan.org

Copyright 2001 Ray Finch.

This module is free software; you can redistribute it and/or
modify it under the same terms as Perl itself.

=head1 SEE ALSO

Storable(3).

=cut
