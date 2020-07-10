use strict; use warnings;
package IO::All::STDIO;

use IO::All -base;
use IO::File;

const type => 'stdio';

sub stdio {
    my $self = shift;
    bless $self, __PACKAGE__;
    return $self->_init;
}

sub stdin {
    my $self = shift;
    $self->open('<');
    return $self;
}

sub stdout {
    my $self = shift;
    $self->open('>');
    return $self;
}

sub stderr {
    my $self = shift;
    $self->open_stderr;
    return $self;
}

sub open {
    my $self = shift;
    $self->is_open(1);
    my $mode = shift || $self->mode || '<';
    my $fileno = $mode eq '>'
    ? fileno(STDOUT)
    : fileno(STDIN);
    $self->io_handle(IO::File->new);
    $self->io_handle->fdopen($fileno, $mode);
    $self->_set_binmode;
}

sub open_stderr {
    my $self = shift;
    $self->is_open(1);
    $self->io_handle(IO::File->new);
    $self->io_handle->fdopen(fileno(STDERR), '>') ? $self : 0;
}

# XXX Add overload support

1;
