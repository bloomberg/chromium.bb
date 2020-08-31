use strict; use warnings;
package IO::All::Pipe;

use IO::All -base;
use IO::File;

const type => 'pipe';

sub pipe {
    my $self = shift;
    bless $self, __PACKAGE__;
    $self->name(shift) if @_;
    return $self->_init;
}

sub _assert_open {
    my $self = shift;
    return if $self->is_open;
    $self->mode(shift) unless $self->mode;
    $self->open;
}

sub open {
    my $self = shift;
    $self->is_open(1);
    require IO::Handle;
    $self->io_handle(IO::Handle->new)
      unless defined $self->io_handle;
    my $command = $self->name;
    $command =~ s/(^\||\|$)//;
    my $mode = shift || $self->mode || '<';
    my $pipe_mode =
      $mode eq '>' ? '|-' :
      $mode eq '<' ? '-|' :
      $self->throw("Invalid usage mode '$mode' for pipe");
    CORE::open($self->io_handle, $pipe_mode, $command);
    $self->_set_binmode;
}

my %mode_msg = (
    '>' => 'output',
    '<' => 'input',
    '>>' => 'append',
);
sub open_msg {
    my $self = shift;
    my $name = defined $self->name
      ? " '" . $self->name . "'"
      : '';
    my $direction = defined $mode_msg{$self->mode}
      ? ' for ' . $mode_msg{$self->mode}
      : '';
    return qq{Can't open pipe$name$direction:\n$!};
}

1;
