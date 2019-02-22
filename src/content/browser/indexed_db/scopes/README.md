# LevelDB Scopes Implementation Design

This document is the implementation plan and design for LevelDB Scopes. It serves to both document the current state and the future plans for implementing LevelDB Scopes.

See LevelDB Scopes general design doc [here](https://docs.google.com/document/d/16_igCI15Gfzb6UYqeuJTmJPrzEtawz6Y1tVOKNtYgiU/edit).

# Status / Current State

The only thing implemented so far is the lock manager interface & descrete implementation.

# Vocabulary

**Scope**

A scope encompases a group of changes that can be reverted. It is basically synonymous with a transaction, and would be used for readwrite and versionchange transactions in LevelDB. The scope has a defined list of key ranges where the changes can occur. It operates by keeping an undo log, which is either discarded on commit, or replayed on revert.

**Undo Log**

Each scope saves an undo log, which has all the information needed to undo all changes performed in the scope.

**Scope Number (scope#)**

Each scope has an identifier unique to the backing store that is auto-incrementing. During recovery, it is set to the minimum unused scope (see more in the recovery section).

**Sequence Number (seq#)**

Every undo log entry has a unique sequence number within the scope. These should start at <max int> (using Fixed64) and decrement. The sequence number '0' is reserved for the commit point

**Commit Point**

This signifies that a scope has been committed. It also means that a specific sequence number was written for a scope.

**Key Range**

Represents a range of LevelDB keys. Every operation has a key or a key range associated with it. Sometimes key ranges are expected to be re-used or modified again by subsequent operations, and sometimes key ranges are known to be never used again.

**Lock**

To prevent reading uncommitted data, IndexedDB 'locks' objects stores when there are (readwrite) transactions operating on them.

## New LevelDB Table Data

|Purpose|Key |Final format|Value (protobufs)|
|---|---|---|---|
|Metadata|undo-metadata|`prefix-0`|`{version: 1}`|
|Scope Metadata|undo-scopes-<scope#>|`prefix-1-<scope#>`|key ranges if scope is active, or \<empty\> if committed.|
|Undo log operations|undo-log-<scope#>-<seq#>|`prefix-2-<scope#>-<seq#>`|undo operation|
|Commit point|undo-log-<scope#>-0|`prefix-2-<scope#>-0`| \<empty\> OR \<ranges to delete\> |


### Key Format

* Allow the 'user' of the scopes system to choose the key prefix (`prefix`).
* Scope # is a varint
* Sequence # is a Fixed64

### Value Format

All values will be protobufs

# Operation Flows

## Creating & Using a Scope
IndexedDB Sequence

* Input - key ranges for the scope. Eg - the applicable object stores (and indexes) for a readwrite txn, or a whole database for a versionchange txn.
* If any of the key ranges are currently locked, then wait until they are free. This would replace the IDB transaction sequencing logic.
* Create the scope 
    * Use the next available scope # (and increment the next scope #)
    * Signal the locks service that the key ranges for this scope are now locked
    * Write undo-scopes-scope# -> key_ranges to LevelDB
* Enable operations on the scope (probably eventually by binding it using mojo)
* For every operation, the scope must read the database to generate the undo log
    * See the [Undo Operation Generation](#undo-operations) section below
* Output - a Scope

## Committing a Scope
**IndexedDB Sequence**

* Input - a Scope
* The scope is marked as 'committed', the commit point is written to the undo log (undo-scope#-0), and the metadatais updated to remove the key ranges (undo-scopes-scope# -> <empty>). This change is flushed to disk.
* The Cleanup & Revert Sequence is signalled for cleaning up the committed scope #.
* Output - Scope is committed, and lock is released.

## Reverting a Scope
**Cleanup & Revert Sequence**

* Input - revert a given scope number.
* Opens a cursor to undo-<scope#>-0
    * Cursor should be at the first undo entry
    * If sequence #0 exists (this key exists), then error - reverting a committed scope is an invalid state in this design
* Iterate through undo log, commiting operations and deleting each undo entry.
* Delete the lock entry at undo-scopes-<scope#>, and flush this change to disk.
* Output - Scope was successfully reverted, and lock released.

## Startup & Recovery
**IndexedDB Sequence**

* Input - the database
* Reads undo-metadata (fail for unknown version)
* Opens a cursor to undo-scopes- & iterates to read in all scopes
    * If the scope metadata has key ranges, then those are considered locked
    * The maximum scope # is found and used to determine the next scope number (used in scope creation)
* Signals the lock system which locks are held
* Signals IndexedDB that it can start accepting operations (IndexedDB can now start running)
* For every undo-scopes- metadata that is empty
    * Kick off an [Undo Log Cleanup](#undo-log-cleanup) task to eventually clean this up.
* For every undo-scopes- metadata with key ranges
    * If there is a commit point for any of these scopes, then that is an invalid state / corruption (as the scopes value should have been emptied in the same writebatch that wrote the commit point).
    * Kick off a [Reverting a Scope](#reverting-a-scope) task. This state indicates a shutdown while a revert was in progress.
* Output - nothing, done

## Undo Log Cleanup
**Cleanup & Revert Sequence**

* Input - nothing
* Scan the undo- namespace for commit points. If a undo-scope#-0 is found, then start deleting that undo log
    * This can happen in multiple write batches
* If ranges are found in the undo-scope#-0 (commit point), then
    * Those ranges are also deleted, in batches.
    * Possibly compact these ranges at the end.
* The undo-scope#-0 and undo-scopes-scope# entries must be deleted last, in the last deletion write batch, so this scope isn't mistaken later as something to be reverted.
* Output - nothing

# Lock Manager

The scopes feature needs a fairly generic lock manager. This lock manager needs two levels, because versionchange transactions need to lock a whole database, where other transactions will lock smaller ranges within the level one range.

### API Methods

#### AcquireLock

Accepts a lock type (shared or exclusive), a level number, a key range, and a callback which is given a moveable-only lock object, which releases its lock on destruction. The levels are totally independent from the perspective of the lock manager. IF an application wants to use multiple levels, they should employ an acquisition order (like, always acquire locks in increasing level order) so they don't deadlock.

### Internal Data Structure

The lock manager basically holds ranges, and needs to know if a new range intersects any old ranges. A good data structure for this is an Interval Tree.

# Undo Operations

## Types

* `Put(key, value)`
* `Delete(key)`
* `DeleteRange(key_range)`

## Generation

### Normal Cases

Note - all cases where "old_value" is used requires reading the current value from the database.

**`Put(key, value)`**

* `Delete(key)` if an old value doesn't exist,
* `Put(key, old_value)` if an old value does exist, or
* Nothing if the old value and new value matches

**`Add(key, value)`**

* Delete(key), trusting the client that there wasn't a value here before.

**`Delete(key)`**

* `Put(key, old_value)` if the old_value exists, or
* Nothing if no old_value exists.

**`DeleteRange(key_range)`**

* `Put(key, old_value)` for every entry in that key range. This requires iterating the database using the key_range to find all entries.

### Special Case - Empty Unique Key Range

#### Creation - key range is empty initially

If the values being created are in a key range that is initially empty (we trust the API caller here - there is no verification), and if the key range will never be reused if it is reverted, then the undo log can consist of a single:

`DeleteRange(key_range)`

Examples of this are creating new databases or indexes in a versionchange transaction. The scopes system can check to make sure it's range is empty before doing operations. This can either be done as a hint (per-range, per-scope), or always.

#### Deletion - key range will never be used again.

This is done by having commit ranges in the value of the commit point. A new scope is created, and commited, with the key ranges never-to-be-used-again will be the value of the commit point record.
