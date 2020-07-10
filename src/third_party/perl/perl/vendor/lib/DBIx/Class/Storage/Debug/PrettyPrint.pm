package DBIx::Class::Storage::Debug::PrettyPrint;

use strict;
use warnings;

use base 'DBIx::Class::Storage::Statistics';

use SQL::Abstract::Tree;

__PACKAGE__->mk_group_accessors( simple => '_sqlat' );
__PACKAGE__->mk_group_accessors( simple => '_clear_line_str' );
__PACKAGE__->mk_group_accessors( simple => '_executing_str' );
__PACKAGE__->mk_group_accessors( simple => '_show_progress' );
__PACKAGE__->mk_group_accessors( simple => '_last_sql' );
__PACKAGE__->mk_group_accessors( simple => 'squash_repeats' );

sub new {
   my $class = shift;
   my $args  = shift;

   my $clear_line = $args->{clear_line} || "\r\x1b[J";
   my $executing  = $args->{executing}  || (
      eval { require Term::ANSIColor } ? do {
          my $c = \&Term::ANSIColor::color;
          $c->('blink white on_black') . 'EXECUTING...' . $c->('reset');
      } : 'EXECUTING...'
   );
   my $show_progress = $args->{show_progress};

   my $squash_repeats = $args->{squash_repeats};
   my $sqlat = SQL::Abstract::Tree->new($args);
   my $self = $class->next::method(@_);
   $self->_clear_line_str($clear_line);
   $self->_executing_str($executing);
   $self->_show_progress($show_progress);

   $self->squash_repeats($squash_repeats);

   $self->_sqlat($sqlat);
   $self->_last_sql('');

   return $self
}

sub print {
  my $self = shift;
  my $string = shift;
  my $bindargs = shift || [];

  my ($lw, $lr);
  ($lw, $string, $lr) = $string =~ /^(\s*)(.+?)(\s*)$/s;

  local $self->_sqlat->{fill_in_placeholders} = 0 if defined $bindargs
    && defined $bindargs->[0] && $bindargs->[0] eq q('__BULK_INSERT__');

  my $use_placeholders = !!$self->_sqlat->fill_in_placeholders;

  my $sqlat = $self->_sqlat;
  my $formatted;
  if ($self->squash_repeats && $self->_last_sql eq $string) {
     my ( $l, $r ) = @{ $sqlat->placeholder_surround };
     $formatted = '... : ' . join(', ', map "$l$_$r", @$bindargs)
  } else {
     $self->_last_sql($string);
     $formatted = $sqlat->format($string, $bindargs);
     $formatted = "$formatted : " . join ', ', @{$bindargs}
        unless $use_placeholders;
  }

  $self->next::method("$lw$formatted$lr", @_);
}

sub query_start {
  my ($self, $string, @bind) = @_;

  if (defined $self->callback) {
    $string =~ m/^(\w+)/;
    $self->callback->($1, "$string: ".join(', ', @bind)."\n");
    return;
  }

  $string =~ s/\s+$//;

  $self->print("$string\n", \@bind);

  $self->debugfh->print($self->_executing_str) if $self->_show_progress
}

sub query_end {
  $_[0]->debugfh->print($_[0]->_clear_line_str) if $_[0]->_show_progress
}

1;

=pod

=head1 NAME

DBIx::Class::Storage::Debug::PrettyPrint - Pretty Printing DebugObj

=head1 SYNOPSIS

 DBIC_TRACE_PROFILE=~/dbic.json perl -Ilib ./foo.pl

Where dbic.json contains:

 {
   "profile":"console",
   "show_progress":1,
   "squash_repeats":1
 }

=head1 METHODS

=head2 new

 my $pp = DBIx::Class::Storage::Debug::PrettyPrint->new({
   show_progress  => 1,             # tries it's best to make it clear that a SQL
                                    # statement is still running
   executing      => '...',         # the string that is added to the end of SQL
                                    # if show_progress is on.  You probably don't
                                    # need to set this
   clear_line     => '<CR><ESC>[J', # the string used to erase the string added
                                    # to SQL if show_progress is on.  Again, the
                                    # default is probably good enough.

   squash_repeats => 1,             # set to true to make repeated SQL queries
                                    # be ellided and only show the new bind params
   # any other args are passed through directly to SQL::Abstract::Tree
 });


