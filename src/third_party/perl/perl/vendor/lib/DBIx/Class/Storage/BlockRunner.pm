package # hide from pause until we figure it all out
  DBIx::Class::Storage::BlockRunner;

use warnings;
use strict;

use DBIx::Class::Exception;
use DBIx::Class::Carp;
use Context::Preserve 'preserve_context';
use DBIx::Class::_Util qw(is_exception qsub);
use Scalar::Util qw(weaken blessed reftype);
use Try::Tiny;
use Moo;
use namespace::clean;

=head1 NAME

DBIx::Class::Storage::BlockRunner - Try running a block of code until success with a configurable retry logic

=head1 DESCRIPTION

=head1 METHODS

=cut

has storage => (
  is => 'ro',
  required => 1,
);

has wrap_txn => (
  is => 'ro',
  required => 1,
);

# true - retry, false - rethrow, or you can throw your own (not catching)
has retry_handler => (
  is => 'ro',
  required => 1,
  isa => qsub q{
    (Scalar::Util::reftype($_[0])||'') eq 'CODE'
      or DBIx::Class::Exception->throw('retry_handler must be a CODE reference')
  },
);

has retry_debug => (
  is => 'rw',
  # use a sub - to be evaluated on the spot lazily
  default => qsub '$ENV{DBIC_STORAGE_RETRY_DEBUG}',
  lazy => 1,
);

has max_attempts => (
  is => 'ro',
  default => 20,
);

has failed_attempt_count => (
  is => 'ro',
  init_arg => undef,  # ensures one can't pass the value in
  writer => '_set_failed_attempt_count',
  default => 0,
  lazy => 1,
  trigger => qsub q{
    $_[0]->throw_exception( sprintf (
      'Reached max_attempts amount of %d, latest exception: %s',
      $_[0]->max_attempts, $_[0]->last_exception
    )) if $_[0]->max_attempts <= ($_[1]||0);
  },
);

has exception_stack => (
  is => 'ro',
  init_arg => undef,
  clearer => '_reset_exception_stack',
  default => qsub q{ [] },
  lazy => 1,
);

sub last_exception { shift->exception_stack->[-1] }

sub throw_exception { shift->storage->throw_exception (@_) }

sub run {
  my $self = shift;

  $self->_reset_exception_stack;
  $self->_set_failed_attempt_count(0);

  my $cref = shift;

  $self->throw_exception('run() requires a coderef to execute as its first argument')
    if ( reftype($cref)||'' ) ne 'CODE';

  my $storage = $self->storage;

  return $cref->( @_ ) if (
    $storage->{_in_do_block}
      and
    ! $self->wrap_txn
  );

  local $storage->{_in_do_block} = 1 unless $storage->{_in_do_block};

  return $self->_run($cref, @_);
}

# this is the actual recursing worker
sub _run {
  # internal method - we know that both refs are strong-held by the
  # calling scope of run(), hence safe to weaken everything
  weaken( my $self = shift );
  weaken( my $cref = shift );

  my $args = @_ ? \@_ : [];

  # from this point on (defined $txn_init_depth) is an indicator for wrap_txn
  # save a bit on method calls
  my $txn_init_depth = $self->wrap_txn ? $self->storage->transaction_depth : undef;
  my $txn_begin_ok;

  my $run_err = '';

  return preserve_context {
    try {
      if (defined $txn_init_depth) {
        $self->storage->txn_begin;
        $txn_begin_ok = 1;
      }
      $cref->( @$args );
    } catch {
      $run_err = $_;
      (); # important, affects @_ below
    };
  } replace => sub {
    my @res = @_;

    my $storage = $self->storage;
    my $cur_depth = $storage->transaction_depth;

    if (defined $txn_init_depth and ! is_exception $run_err) {
      my $delta_txn = (1 + $txn_init_depth) - $cur_depth;

      if ($delta_txn) {
        # a rollback in a top-level txn_do is valid-ish (seen in the wild and our own tests)
        carp (sprintf
          'Unexpected reduction of transaction depth by %d after execution of '
        . '%s, skipping txn_commit()',
          $delta_txn,
          $cref,
        ) unless $delta_txn == 1 and $cur_depth == 0;
      }
      else {
        $run_err = eval { $storage->txn_commit; 1 } ? '' : $@;
      }
    }

    # something above threw an error (could be the begin, the code or the commit)
    if ( is_exception $run_err ) {

      # attempt a rollback if we did begin in the first place
      if ($txn_begin_ok) {
        # some DBDs go crazy if there is nothing to roll back on, perform a soft-check
        my $rollback_exception = $storage->_seems_connected
          ? (! eval { $storage->txn_rollback; 1 }) ? $@ : ''
          : 'lost connection to storage'
        ;

        if ( $rollback_exception and (
          ! defined blessed $rollback_exception
            or
          ! $rollback_exception->isa('DBIx::Class::Storage::NESTED_ROLLBACK_EXCEPTION')
        ) ) {
          $run_err = "Transaction aborted: $run_err. Rollback failed: $rollback_exception";
        }
      }

      push @{ $self->exception_stack }, $run_err;

      # this will throw if max_attempts is reached
      $self->_set_failed_attempt_count($self->failed_attempt_count + 1);

      # init depth of > 0 ( > 1 with AC) implies nesting - no retry attempt queries
      $storage->throw_exception($run_err) if (
        (
          defined $txn_init_depth
            and
          # FIXME - we assume that $storage->{_dbh_autocommit} is there if
          # txn_init_depth is there, but this is a DBI-ism
          $txn_init_depth > ( $storage->{_dbh_autocommit} ? 0 : 1 )
        ) or ! $self->retry_handler->($self)
      );

      # we got that far - let's retry
      carp( sprintf 'Retrying %s (attempt %d) after caught exception: %s',
        $cref,
        $self->failed_attempt_count + 1,
        $run_err,
      ) if $self->retry_debug;

      $storage->ensure_connected;
      # if txn_depth is > 1 this means something was done to the
      # original $dbh, otherwise we would not get past the preceding if()
      $storage->throw_exception(sprintf
        'Unexpected transaction depth of %d on freshly connected handle',
        $storage->transaction_depth,
      ) if (defined $txn_init_depth and $storage->transaction_depth);

      return $self->_run($cref, @$args);
    }

    return wantarray ? @res : $res[0];
  };
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
