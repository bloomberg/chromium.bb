package DBIx::Class::Storage::Statistics;

use strict;
use warnings;

use DBIx::Class::_Util qw(sigwarn_silencer qsub);
use IO::Handle ();
use Moo;
extends 'DBIx::Class';
use namespace::clean;

=head1 NAME

DBIx::Class::Storage::Statistics - SQL Statistics

=head1 SYNOPSIS

=head1 DESCRIPTION

This class is called by DBIx::Class::Storage::DBI as a means of collecting
statistics on its actions.  Using this class alone merely prints the SQL
executed, the fact that it completes and begin/end notification for
transactions.

To really use this class you should subclass it and create your own method
for collecting the statistics as discussed in L<DBIx::Class::Manual::Cookbook>.

=head1 METHODS

=head2 new

Returns a new L<DBIx::Class::Storage::Statistics> object.

=head2 debugfh

Sets or retrieves the filehandle used for trace/debug output.  This should
be an L<IO::Handle> compatible object (only the
L<< print|IO::Handle/METHODS >> method is used). By
default it is initially set to STDERR - although see discussion of the
L<DBIC_TRACE|DBIx::Class::Storage/DBIC_TRACE> environment variable.

Invoked as a getter it will lazily open a filehandle and set it to
L<< autoflush|perlvar/HANDLE->autoflush( EXPR ) >> (if one is not
already set).

=cut

# FIXME - there ought to be a way to fold this into _debugfh itself
# having the undef re-trigger the builder (or better yet a default
# which can be folded in as a qsub)
sub debugfh {
  my $self = shift;

  return $self->_debugfh(@_) if @_;
  $self->_debugfh || $self->_build_debugfh;
}

has _debugfh => (
  is => 'rw',
  lazy => 1,
  trigger => qsub '$_[0]->_defaulted_to_stderr(undef)',
  builder => '_build_debugfh',
);

sub _build_debugfh {
  my $fh;

  my $debug_env = $ENV{DBIX_CLASS_STORAGE_DBI_DEBUG} || $ENV{DBIC_TRACE};

  if (defined($debug_env) and ($debug_env =~ /=(.+)$/)) {
    open ($fh, '>>', $1)
      or die("Cannot open trace file $1: $!\n");
  }
  else {
    open ($fh, '>&STDERR')
      or die("Duplication of STDERR for debug output failed (perhaps your STDERR is closed?): $!\n");
    $_[0]->_defaulted_to_stderr(1);
  }

  $fh->autoflush(1);

  $fh;
}

has [qw(_defaulted_to_stderr silence callback)] => (
  is => 'rw',
);

=head2 print

Prints the specified string to our debugging filehandle.  Provided to save our
methods the worry of how to display the message.

=cut
sub print {
  my ($self, $msg) = @_;

  return if $self->silence;

  my $fh = $self->debugfh;

  # not using 'no warnings' here because all of this can change at runtime
  local $SIG{__WARN__} = sigwarn_silencer(qr/^Wide character in print/)
    if $self->_defaulted_to_stderr;

  $fh->print($msg);
}

=head2 silence

Turn off all output if set to true.

=head2 txn_begin

Called when a transaction begins.

=cut
sub txn_begin {
  my $self = shift;

  return if $self->callback;

  $self->print("BEGIN WORK\n");
}

=head2 txn_rollback

Called when a transaction is rolled back.

=cut
sub txn_rollback {
  my $self = shift;

  return if $self->callback;

  $self->print("ROLLBACK\n");
}

=head2 txn_commit

Called when a transaction is committed.

=cut
sub txn_commit {
  my $self = shift;

  return if $self->callback;

  $self->print("COMMIT\n");
}

=head2 svp_begin

Called when a savepoint is created.

=cut
sub svp_begin {
  my ($self, $name) = @_;

  return if $self->callback;

  $self->print("SAVEPOINT $name\n");
}

=head2 svp_release

Called when a savepoint is released.

=cut
sub svp_release {
  my ($self, $name) = @_;

  return if $self->callback;

  $self->print("RELEASE SAVEPOINT $name\n");
}

=head2 svp_rollback

Called when rolling back to a savepoint.

=cut
sub svp_rollback {
  my ($self, $name) = @_;

  return if $self->callback;

  $self->print("ROLLBACK TO SAVEPOINT $name\n");
}

=head2 query_start

Called before a query is executed.  The first argument is the SQL string being
executed and subsequent arguments are the parameters used for the query.

=cut
sub query_start {
  my ($self, $string, @bind) = @_;

  my $message = "$string: ".join(', ', @bind)."\n";

  if(defined($self->callback)) {
    $string =~ m/^(\w+)/;
    $self->callback->($1, $message);
    return;
  }

  $self->print($message);
}

=head2 query_end

Called when a query finishes executing.  Has the same arguments as query_start.

=cut

sub query_end {
  my ($self, $string) = @_;
}

=head1 FURTHER QUESTIONS?

Check the list of L<additional DBIC resources|DBIx::Class/GETTING HELP/SUPPORT>.

=head1 COPYRIGHT AND LICENSE

This module is free software L<copyright|DBIx::Class/COPYRIGHT AND LICENSE>
by the L<DBIx::Class (DBIC) authors|DBIx::Class/AUTHORS>. You can
redistribute it and/or modify it under the same terms as the
L<DBIx::Class library|DBIx::Class/COPYRIGHT AND LICENSE>.

=cut

1;
