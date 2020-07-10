# $Id: $
# This is free software, you may use it and distribute it under the same terms as
# Perl itself.
#
# Copyright 2011 Joachim Zobel
#
package XML::LibXML::Devel;

use strict;
use warnings;

use XML::LibXML;

use vars qw ($VERSION);
$VERSION = "2.0200"; # VERSION TEMPLATE: DO NOT CHANGE

use 5.008_000;

use parent qw(Exporter);

use vars qw( @EXPORT @EXPORT_OK %EXPORT_TAGS );

# This allows declaration	use XML::LibXML::Devel ':all';
# If you do not need this, moving things directly into @EXPORT or @EXPORT_OK
# will save memory.
our %EXPORT_TAGS = ( 'all' => [ qw(
  node_to_perl
  node_from_perl
  refcnt_inc
  refcnt_dec
  refcnt
  fix_owner
  mem_used
) ] );

our @EXPORT_OK = ( @{ $EXPORT_TAGS{'all'} } );

# Preloaded methods go here.

1;
__END__

=head1 NAME

XML::LibXML::Devel - makes functions from LibXML.xs available

=head1 SYNOPSIS

  /**********************************************
   * C functions you want to access
   */
  xmlNode *return_node();
  void receive_node(xmlNode *);

  ###############################################
  # XS Code
  void *
    xs_return_node
    CODE:
        RETVAL = return_node();
    OUTPUT:
        RETVAL

  void
    xs_receive_node
        void *n
    CODE:
        receive_node(n);

  ###############################################
  # Perl code
  use XML::LibXML::Devel;

  sub return_node
  {
    my $raw_node = xs_return_node();
    my $node = XML::LibXML::Devel::node_to_perl($raw_node);
    XML::LibXML::Devel::refcnt_inc($raw_node);
    return $node;
  }

  sub receive_node
  {
    my ($node) = @_;
    my $raw_node = XML::LibXML::Devel::node_from_perl($node);
    xs_receive_node($raw_node);
    XML::LibXML::Devel::refcnt_inc($raw_node);
  }


=head1 DESCRIPTION

C<XML::LibXML::Devel> makes functions from LibXML.xs available that
are needed to wrap libxml2 nodes in and out of XML::LibXML::Nodes.
This gives cleaner dependencies than using LibXML.so directly.

To XS a library that uses libxml2 nodes the first step is to
do this so that xmlNodePtr is passed as void *. These raw nodes
are then turned into libxml nodes by using this C<Devel> functions.

Be aware that this module is currently rather experimental. The function
names may change if I XS more functions and introduce a reasonable
naming convention.

Be also aware that this module is a great tool to cause segfaults and
introduce memory leaks. It does however provide a partial cure by making
C<xmlMemUsed> available as C<mem_used>.

=head1 FUNCTIONS

=head2 NODE MANAGEMENT

=over 1

=item node_to_perl

  node_to_perl($raw_node);

Returns a LibXML::Node object. This has a proxy node with a reference
counter and an owner attached. The raw node will be deleted as soon
as the reference counter reaches zero.
If the C library is keeping a
pointer to the raw node, you need to call refcnt_inc immediately.
You also need to replace xmlFreeNode by a call to refcnt_dec.

=item node_to_perl

  node_from_perl($node);

Returns a raw node. This is a void * pointer and you can do nothing
but passing it to functions that treat it as an xmlNodePtr. The
raw node will be freed as soon as its reference counter reaches zero.
If the C library is keeping a
pointer to the raw node, you need to call refcnt_inc immediately.
You also need to replace xmlFreeNode by a call to refcnt_dec.

=item refcnt_inc

  refcnt_inc($raw_node);

Increments the raw nodes reference counter. The raw node must already
be known to perl to have a reference counter.

=item refcnt_dec

  refcnt_dec($raw_node);

Decrements the raw nodes reference counter and returns the value it
had before. if the counter becomes zero or less,
this method will free the proxy node holding the reference counter.
If the node is part of a
subtree, refcnt_dec will fix the reference counts and delete
the subtree if it is not required any more.

=item refcnt

  refcnt($raw_node);

Returns the value of the reference counter.

=item fix_owner

  fix_owner($raw_node, $raw_parent);

This functions fixes the reference counts for an entire subtree.
it is very important to fix an entire subtree after node operations
where the documents or the owner node may get changed. this method is
aware about nodes that already belong to a certain owner node.

=back

=head2 MEMORY DEBUGGING

=over 1

=item $ENV{DEBUG_MEMORY}

  BEGIN {$ENV{DEBUG_MEMORY} = 1;};
  use XML::LibXML;

This turns on libxml2 memory debugging. It must be set before
XML::LibXML is loaded.


=item mem_used

  mem_used();

Returns the number of bytes currently allocated.

=back

=head2 EXPORT

None by default.

=head1 SEE ALSO

This was created to support the needs of Apache2::ModXml2. So this
can serve as an example.

=head1 AUTHOR

Joachim Zobel E<lt>jz-2011@heute-morgen.deE<gt>

=head1 COPYRIGHT AND LICENSE

Copyright (C) 2011 by Joachim Zobel

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself, either Perl version 5.10.1 or,
at your option, any later version of Perl 5 you may have available.


=cut

