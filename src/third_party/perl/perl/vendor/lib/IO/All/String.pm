use strict; use warnings;
package IO::All::String;

use IO::All -base;

const type => 'string';

sub string_ref {
   my ($self, $ref) = @_;

   no strict 'refs';
   *$self->{ref} = $ref if exists $_[1];

   return *$self->{ref}
}

sub string {
    my $self = shift;
    bless $self, __PACKAGE__;
    $self->_init;
}

sub open {
    my $self = shift;
    my $str = '';
    my $ref = \$str;
    $self->string_ref($ref);
    open my $fh, '+<', $ref;
    $self->io_handle($fh);
    $self->_set_binmode;
    $self->is_open(1);
}

1;
