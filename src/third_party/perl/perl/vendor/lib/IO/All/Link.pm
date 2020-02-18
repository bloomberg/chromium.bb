use strict; use warnings;
package IO::All::Link;

use IO::All::File -base;

const type => 'link';

sub link {
    my $self = shift;
    bless $self, __PACKAGE__;
    $self->name(shift) if @_;
    $self->_init;
}

sub readlink {
    my $self = shift;
    $self->_constructor->(CORE::readlink($self->name));
}

sub symlink {
    my $self = shift;
    my $target = shift;
    $self->assert_filepath if $self->_assert;
    CORE::symlink($target, $self->pathname);
}

sub AUTOLOAD {
    my $self = shift;
    our $AUTOLOAD;
    (my $method = $AUTOLOAD) =~ s/.*:://;
    my $target = $self->target;
    unless ($target) {
        $self->throw("Can't call $method on symlink");
        return;
    }
    $target->$method(@_);
}

sub target {
    my $self = shift;
    return *$self->{target} if *$self->{target};
    my %seen;
    my $link = $self;
    my $new;
    while ($new = $link->readlink) {
        my $type = $new->type or return;
        last if $type eq 'file';
        last if $type eq 'dir';
        return unless $type eq 'link';
        return if $seen{$new->name}++;
        $link = $new;
    }
    *$self->{target} = $new;
}

sub exists { -l shift->pathname }

1;
