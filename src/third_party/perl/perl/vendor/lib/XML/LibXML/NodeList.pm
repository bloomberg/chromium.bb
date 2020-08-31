# $Id$
#
# This is free software, you may use it and distribute it under the same terms as
# Perl itself.
#
# Copyright 2001-2003 AxKit.com Ltd., 2002-2006 Christian Glahn, 2006-2009 Petr Pajas
#
#

package XML::LibXML::NodeList;

use strict;
use warnings;

use XML::LibXML::Boolean;
use XML::LibXML::Literal;
use XML::LibXML::Number;

use vars qw($VERSION);
$VERSION = "2.0200"; # VERSION TEMPLATE: DO NOT CHANGE

use overload
        '""' => \&to_literal,
        'bool' => \&to_boolean,
        'cmp' => sub {
            my($aa, $bb, $order) = @_;
            return ($order ? ("$bb" cmp "$aa") : ("$aa" cmp "$bb"));
        },
        ;

sub new {
    my $class = shift;
    bless [@_], $class;
}

sub new_from_ref {
    my ($class,$array_ref,$reuse) = @_;
    return bless $reuse ? $array_ref : [@$array_ref], $class;
}

sub pop {
    my $self = CORE::shift;
    CORE::pop @$self;
}

sub push {
    my $self = CORE::shift;
    CORE::push @$self, @_;
}

sub append {
    my $self = CORE::shift;
    my ($nodelist) = @_;
    CORE::push @$self, $nodelist->get_nodelist;
}

sub shift {
    my $self = CORE::shift;
    CORE::shift @$self;
}

sub unshift {
    my $self = CORE::shift;
    CORE::unshift @$self, @_;
}

sub prepend {
    my $self = CORE::shift;
    my ($nodelist) = @_;
    CORE::unshift @$self, $nodelist->get_nodelist;
}

sub size {
    my $self = CORE::shift;
    scalar @$self;
}

sub get_node {
    # uses array index starting at 1, not 0
    # this is mainly because of XPath.
    my $self = CORE::shift;
    my ($pos) = @_;
    $self->[$pos - 1];
}

sub item
{
    my ($self, $pos) = @_;
    return $self->[$pos];
}

sub get_nodelist {
    my $self = CORE::shift;
    @$self;
}

sub to_boolean {
    my $self = CORE::shift;
    return (@$self > 0) ? XML::LibXML::Boolean->True : XML::LibXML::Boolean->False;
}

# string-value of a nodelist is the string-value of the first node
sub string_value {
    my $self = CORE::shift;
    return '' unless @$self;
    return $self->[0]->string_value;
}

sub to_literal {
    my $self = CORE::shift;
    return XML::LibXML::Literal->new(
            join('', CORE::grep {defined $_} CORE::map { $_->string_value } @$self)
            );
}

sub to_literal_delimited {
    my $self = CORE::shift;
    return XML::LibXML::Literal->new(
            join(CORE::shift, CORE::grep {defined $_} CORE::map { $_->string_value } @$self)
            );
}

sub to_literal_list {
    my $self = CORE::shift;
    my @nodes = CORE::map{ XML::LibXML::Literal->new($_->string_value())->value() } @{$self};

    if (wantarray) {
        return( @nodes );
    }
    return( \@nodes );
}

sub to_number {
    my $self = CORE::shift;
    return XML::LibXML::Number->new(
            $self->to_literal
            );
}

sub iterator {
    warn "this function is obsolete!\nIt was disabled in version 1.54\n";
    return undef;
}

sub map {
    my $self = CORE::shift;
    my $sub  = __is_code(CORE::shift);
    local $_;
    my @results = CORE::map { @{[ $sub->($_) ]} } @$self;
    return unless defined wantarray;
    return wantarray ? @results : (ref $self)->new(@results);
}

sub grep {
    my $self = CORE::shift;
    my $sub  = __is_code(CORE::shift);
    local $_;
    my @results = CORE::grep { $sub->($_) } @$self;
    return unless defined wantarray;
    return wantarray ? @results : (ref $self)->new(@results);
}

sub sort {
    my $self = CORE::shift;
    my $sub  = __is_code(CORE::shift);
    my @results = CORE::sort { $sub->($a,$b) } @$self;
    return wantarray ? @results : (ref $self)->new(@results);
}

sub foreach {
    my $self = CORE::shift;
    my $sub  = CORE::shift;

    foreach my $item (@$self)
    {
        local $_ = $item;
        $sub->($item);
    }

    return wantarray ? @$self : $self;
}

sub reverse {
    my $self    = CORE::shift;
    my @results = CORE::reverse @$self;
    return wantarray ? @results : (ref $self)->new(@results);
}

sub reduce {
    my $self = CORE::shift;
    my $sub  = __is_code(CORE::shift);

    my @list = @$self;
    CORE::unshift @list, $_[0] if @_;

    my $a = CORE::shift(@list);
    foreach my $b (@list)
    {
        $a = $sub->($a, $b);
    }
    return $a;
}

sub __is_code {
    my ($code) = @_;

    if (ref $code eq 'CODE') {
        return $code;
    }

    # There are better ways of doing this, but here I've tried to
    # avoid adding any additional external dependencies.
    #
    if (UNIVERSAL::can($code, 'can')        # is blessed (sort of)
    and overload::Overloaded($code)         # is overloaded
    and overload::Method($code, '&{}')) {   # overloads '&{}'
        return $code;
    }

    # The other possibility is that $code is a coderef, but is
    # blessed into a class that doesn't overload '&{}'. In which
    # case... well, I'm stumped!

    die "Not a subroutine reference\n";
}

1;
__END__

=head1 NAME

XML::LibXML::NodeList - a list of XML document nodes

=head1 DESCRIPTION

An XML::LibXML::NodeList object contains an ordered list of nodes, as
detailed by the W3C DOM documentation of Node Lists.

=head1 SYNOPSIS

  my $results = $dom->findnodes('//somepath');
  foreach my $context ($results->get_nodelist) {
    my $newresults = $context->findnodes('./other/element');
    ...
  }

=head1 API

=head2 new(@nodes)

You will almost never have to create a new NodeList object, as it is all
done for you by XPath.

=head2 get_nodelist()

Returns a list of nodes, the contents of the node list, as a perl list.

=head2 string_value()

Returns the string-value of the first node in the list.
See the XPath specification for what "string-value" means.

=head2 to_literal()

Returns the concatenation of all the string-values of all
the nodes in the list.

=head2 to_literal_delimited($separator)

Returns the concatenation of all the string-values of all
the nodes in the list, delimited by the specified separator.

=head2 to_literal_list()

Returns all the string-values of all the nodes in the list as
a perl list.

=head2 get_node($pos)

Returns the node at $pos. The node position in XPath is based at 1, not 0.

=head2 size()

Returns the number of nodes in the NodeList.

=head2 pop()

Equivalent to perl's pop function.

=head2 push(@nodes)

Equivalent to perl's push function.

=head2 append($nodelist)

Given a nodelist, appends the list of nodes in $nodelist to the end of the
current list.

=head2 shift()

Equivalent to perl's shift function.

=head2 unshift(@nodes)

Equivalent to perl's unshift function.

=head2 prepend($nodelist)

Given a nodelist, prepends the list of nodes in $nodelist to the front of
the current list.

=head2 map($coderef)

Equivalent to perl's map function.

=head2 grep($coderef)

Equivalent to perl's grep function.

=head2 sort($coderef)

Equivalent to perl's sort function.

Caveat: Perl's magic C<$a> and C<$b> variables are not available in
C<$coderef>. Instead the two terms are passed to the coderef as arguments.

=head2 reverse()

Equivalent to perl's reverse function.

=head2 foreach($coderef)

Inspired by perl's foreach loop. Executes the coderef on each item in
the list. Similar to C<map>, but instead of returning the list of values
returned by $coderef, returns the original NodeList.

=head2 reduce($coderef, $init)

Equivalent to List::Util's reduce function. C<$init> is optional and
provides an initial value for the reduction.

Caveat: Perl's magic C<$a> and C<$b> variables are not available in
C<$coderef>. Instead the two terms are passed to the coderef as arguments.

=cut
