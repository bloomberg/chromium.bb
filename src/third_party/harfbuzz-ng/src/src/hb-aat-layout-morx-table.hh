/*
 * Copyright © 2017  Google, Inc.
 *
 *  This is part of HarfBuzz, a text shaping library.
 *
 * Permission is hereby granted, without written agreement and without
 * license or royalty fees, to use, copy, modify, and distribute this
 * software and its documentation for any purpose, provided that the
 * above copyright notice and the following two paragraphs appear in
 * all copies of this software.
 *
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE TO ANY PARTY FOR
 * DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES
 * ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN
 * IF THE COPYRIGHT HOLDER HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 * THE COPYRIGHT HOLDER SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING,
 * BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS
 * ON AN "AS IS" BASIS, AND THE COPYRIGHT HOLDER HAS NO OBLIGATION TO
 * PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
 *
 * Google Author(s): Behdad Esfahbod
 */

#ifndef HB_AAT_LAYOUT_MORX_TABLE_HH
#define HB_AAT_LAYOUT_MORX_TABLE_HH

#include "hb-open-type.hh"
#include "hb-aat-layout-common.hh"
#include "hb-ot-layout-common.hh"

/*
 * morx -- Extended Glyph Metamorphosis
 * https://developer.apple.com/fonts/TrueType-Reference-Manual/RM06/Chap6morx.html
 */
#define HB_AAT_TAG_morx HB_TAG('m','o','r','x')


namespace AAT {

using namespace OT;


struct RearrangementSubtable
{
  typedef void EntryData;

  struct driver_context_t
  {
    static const bool in_place = true;
    enum Flags
    {
      MarkFirst		= 0x8000,	/* If set, make the current glyph the first
					 * glyph to be rearranged. */
      DontAdvance	= 0x4000,	/* If set, don't advance to the next glyph
					 * before going to the new state. This means
					 * that the glyph index doesn't change, even
					 * if the glyph at that index has changed. */
      MarkLast		= 0x2000,	/* If set, make the current glyph the last
					 * glyph to be rearranged. */
      Reserved		= 0x1FF0,	/* These bits are reserved and should be set to 0. */
      Verb		= 0x000F,	/* The type of rearrangement specified. */
    };

    inline driver_context_t (const RearrangementSubtable *table) :
	ret (false),
	start (0), end (0) {}

    inline bool is_actionable (StateTableDriver<EntryData> *driver,
			       const Entry<EntryData> *entry)
    {
      return (entry->flags & Verb) && start < end;
    }
    inline bool transition (StateTableDriver<EntryData> *driver,
			    const Entry<EntryData> *entry)
    {
      hb_buffer_t *buffer = driver->buffer;
      unsigned int flags = entry->flags;

      if (flags & MarkFirst)
	start = buffer->idx;

      if (flags & MarkLast)
	end = MIN (buffer->idx + 1, buffer->len);

      if ((flags & Verb) && start < end)
      {
	/* The following map has two nibbles, for start-side
	 * and end-side. Values of 0,1,2 mean move that many
	 * to the other side. Value of 3 means move 2 and
	 * flip them. */
	const unsigned char map[16] =
	{
	  0x00,	/* 0	no change */
	  0x10,	/* 1	Ax => xA */
	  0x01,	/* 2	xD => Dx */
	  0x11,	/* 3	AxD => DxA */
	  0x20,	/* 4	ABx => xAB */
	  0x30,	/* 5	ABx => xBA */
	  0x02,	/* 6	xCD => CDx */
	  0x03,	/* 7	xCD => DCx */
	  0x12,	/* 8	AxCD => CDxA */
	  0x13,	/* 9	AxCD => DCxA */
	  0x21,	/* 10	ABxD => DxAB */
	  0x31,	/* 11	ABxD => DxBA */
	  0x22,	/* 12	ABxCD => CDxAB */
	  0x32,	/* 13	ABxCD => CDxBA */
	  0x23,	/* 14	ABxCD => DCxAB */
	  0x33,	/* 15	ABxCD => DCxBA */
	};

	unsigned int m = map[flags & Verb];
	unsigned int l = MIN<unsigned int> (2, m >> 4);
	unsigned int r = MIN<unsigned int> (2, m & 0x0F);
	bool reverse_l = 3 == (m >> 4);
	bool reverse_r = 3 == (m & 0x0F);

	if (end - start >= l + r)
	{
	  buffer->merge_clusters (start, MIN (buffer->idx + 1, buffer->len));
	  buffer->merge_clusters (start, end);

	  hb_glyph_info_t *info = buffer->info;
	  hb_glyph_info_t buf[4];

	  memcpy (buf, info + start, l * sizeof (buf[0]));
	  memcpy (buf + 2, info + end - r, r * sizeof (buf[0]));

	  if (l != r)
	    memmove (info + start + r, info + start + l, (end - start - l - r) * sizeof (buf[0]));

	  memcpy (info + start, buf + 2, r * sizeof (buf[0]));
	  memcpy (info + end - l, buf, l * sizeof (buf[0]));
	  if (reverse_l)
	  {
	    buf[0] = info[end - 1];
	    info[end - 1] = info[end - 2];
	    info[end - 2] = buf[0];
	  }
	  if (reverse_r)
	  {
	    buf[0] = info[start];
	    info[start] = info[start + 1];
	    info[start + 1] = buf[0];
	  }
	}
      }

      return true;
    }

    public:
    bool ret;
    private:
    unsigned int start;
    unsigned int end;
  };

  inline bool apply (hb_aat_apply_context_t *c) const
  {
    TRACE_APPLY (this);

    driver_context_t dc (this);

    StateTableDriver<void> driver (machine, c->buffer, c->face);
    driver.drive (&dc);

    return_trace (dc.ret);
  }

  inline bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (machine.sanitize (c));
  }

  protected:
  StateTable<EntryData>	machine;
  public:
  DEFINE_SIZE_STATIC (16);
};

struct ContextualSubtable
{
  struct EntryData
  {
    HBUINT16	markIndex;	/* Index of the substitution table for the
				 * marked glyph (use 0xFFFF for none). */
    HBUINT16	currentIndex;	/* Index of the substitution table for the
				 * current glyph (use 0xFFFF for none). */
    public:
    DEFINE_SIZE_STATIC (4);
  };

  struct driver_context_t
  {
    static const bool in_place = true;
    enum Flags
    {
      SetMark		= 0x8000,	/* If set, make the current glyph the marked glyph. */
      DontAdvance	= 0x4000,	/* If set, don't advance to the next glyph before
					 * going to the new state. */
      Reserved		= 0x3FFF,	/* These bits are reserved and should be set to 0. */
    };

    inline driver_context_t (const ContextualSubtable *table) :
	ret (false),
	mark_set (false),
	mark (0),
	subs (table+table->substitutionTables) {}

    inline bool is_actionable (StateTableDriver<EntryData> *driver,
			       const Entry<EntryData> *entry)
    {
      hb_buffer_t *buffer = driver->buffer;

      if (buffer->idx == buffer->len && !mark_set)
        return false;

      return entry->data.markIndex != 0xFFFF || entry->data.currentIndex != 0xFFFF;
    }
    inline bool transition (StateTableDriver<EntryData> *driver,
			    const Entry<EntryData> *entry)
    {
      hb_buffer_t *buffer = driver->buffer;

      /* Looks like CoreText applies neither mark nor current substitution for
       * end-of-text if mark was not explicitly set. */
      if (buffer->idx == buffer->len && !mark_set)
        return true;

      if (entry->data.markIndex != 0xFFFF)
      {
	const Lookup<GlyphID> &lookup = subs[entry->data.markIndex];
	hb_glyph_info_t *info = buffer->info;
	const GlyphID *replacement = lookup.get_value (info[mark].codepoint, driver->num_glyphs);
	if (replacement)
	{
	  buffer->unsafe_to_break (mark, MIN (buffer->idx + 1, buffer->len));
	  info[mark].codepoint = *replacement;
	  ret = true;
	}
      }
      if (entry->data.currentIndex != 0xFFFF)
      {
        unsigned int idx = MIN (buffer->idx, buffer->len - 1);
	const Lookup<GlyphID> &lookup = subs[entry->data.currentIndex];
	hb_glyph_info_t *info = buffer->info;
	const GlyphID *replacement = lookup.get_value (info[idx].codepoint, driver->num_glyphs);
	if (replacement)
	{
	  info[idx].codepoint = *replacement;
	  ret = true;
	}
      }

      if (entry->flags & SetMark)
      {
	mark_set = true;
	mark = buffer->idx;
      }

      return true;
    }

    public:
    bool ret;
    private:
    bool mark_set;
    unsigned int mark;
    const UnsizedOffsetListOf<Lookup<GlyphID>, HBUINT32> &subs;
  };

  inline bool apply (hb_aat_apply_context_t *c) const
  {
    TRACE_APPLY (this);

    driver_context_t dc (this);

    StateTableDriver<EntryData> driver (machine, c->buffer, c->face);
    driver.drive (&dc);

    return_trace (dc.ret);
  }

  inline bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);

    unsigned int num_entries = 0;
    if (unlikely (!machine.sanitize (c, &num_entries))) return_trace (false);

    unsigned int num_lookups = 0;

    const Entry<EntryData> *entries = machine.get_entries ();
    for (unsigned int i = 0; i < num_entries; i++)
    {
      const EntryData &data = entries[i].data;

      if (data.markIndex != 0xFFFF)
	num_lookups = MAX<unsigned int> (num_lookups, 1 + data.markIndex);
      if (data.currentIndex != 0xFFFF)
	num_lookups = MAX<unsigned int> (num_lookups, 1 + data.currentIndex);
    }

    return_trace (substitutionTables.sanitize (c, this, num_lookups));
  }

  protected:
  StateTable<EntryData>
		machine;
  LOffsetTo<UnsizedOffsetListOf<Lookup<GlyphID>, HBUINT32>, false>
		substitutionTables;
  public:
  DEFINE_SIZE_STATIC (20);
};

struct LigatureSubtable
{
  struct EntryData
  {
    HBUINT16	ligActionIndex;	/* Index to the first ligActionTable entry
				 * for processing this group, if indicated
				 * by the flags. */
    public:
    DEFINE_SIZE_STATIC (2);
  };

  struct driver_context_t
  {
    static const bool in_place = false;
    enum Flags
    {
      SetComponent	= 0x8000,	/* Push this glyph onto the component stack for
					 * eventual processing. */
      DontAdvance	= 0x4000,	/* Leave the glyph pointer at this glyph for the
					   next iteration. */
      PerformAction	= 0x2000,	/* Use the ligActionIndex to process a ligature
					 * group. */
      Reserved		= 0x1FFF,	/* These bits are reserved and should be set to 0. */
    };
    enum LigActionFlags
    {
      LigActionLast	= 0x80000000,	/* This is the last action in the list. This also
					 * implies storage. */
      LigActionStore	= 0x40000000,	/* Store the ligature at the current cumulated index
					 * in the ligature table in place of the marked
					 * (i.e. currently-popped) glyph. */
      LigActionOffset	= 0x3FFFFFFF,	/* A 30-bit value which is sign-extended to 32-bits
					 * and added to the glyph ID, resulting in an index
					 * into the component table. */
    };

    inline driver_context_t (const LigatureSubtable *table,
			     hb_aat_apply_context_t *c_) :
	ret (false),
	c (c_),
	ligAction (table+table->ligAction),
	component (table+table->component),
	ligature (table+table->ligature),
	match_length (0) {}

    inline bool is_actionable (StateTableDriver<EntryData> *driver,
			       const Entry<EntryData> *entry)
    {
      return !!(entry->flags & PerformAction);
    }
    inline bool transition (StateTableDriver<EntryData> *driver,
			    const Entry<EntryData> *entry)
    {
      hb_buffer_t *buffer = driver->buffer;
      unsigned int flags = entry->flags;

      if (flags & SetComponent)
      {
        if (unlikely (match_length >= ARRAY_LENGTH (match_positions)))
	  return false;

	/* Never mark same index twice, in case DontAdvance was used... */
	if (match_length && match_positions[match_length - 1] == buffer->out_len)
	  match_length--;

	match_positions[match_length++] = buffer->out_len;
      }

      if (flags & PerformAction)
      {
	unsigned int end = buffer->out_len;
	unsigned int action_idx = entry->data.ligActionIndex;
	unsigned int action;
	unsigned int ligature_idx = 0;
        do
	{
	  if (unlikely (!match_length))
	    return false;

	  buffer->move_to (match_positions[--match_length]);

	  const HBUINT32 &actionData = ligAction[action_idx];
	  if (unlikely (!actionData.sanitize (&c->sanitizer))) return false;
	  action = actionData;

	  uint32_t uoffset = action & LigActionOffset;
	  if (uoffset & 0x20000000)
	    uoffset += 0xC0000000;
	  int32_t offset = (int32_t) uoffset;
	  unsigned int component_idx = buffer->cur().codepoint + offset;

	  const HBUINT16 &componentData = component[component_idx];
	  if (unlikely (!componentData.sanitize (&c->sanitizer))) return false;
	  ligature_idx += componentData;

	  if (action & (LigActionStore | LigActionLast))
	  {
	    const GlyphID &ligatureData = ligature[ligature_idx];
	    if (unlikely (!ligatureData.sanitize (&c->sanitizer))) return false;
	    hb_codepoint_t lig = ligatureData;

	    match_positions[match_length++] = buffer->out_len;
	    buffer->replace_glyph (lig);

	    //ligature_idx = 0; // XXX Yes or no?
	  }
	  else
	  {
	    buffer->skip_glyph ();
	    end--;
	  }
	  /* TODO merge_clusters / unsafe_to_break */

	  action_idx++;
	}
	while (!(action & LigActionLast));
	buffer->move_to (end);
      }

      return true;
    }

    public:
    bool ret;
    private:
    hb_aat_apply_context_t *c;
    const UnsizedArrayOf<HBUINT32> &ligAction;
    const UnsizedArrayOf<HBUINT16> &component;
    const UnsizedArrayOf<GlyphID> &ligature;
    unsigned int match_length;
    unsigned int match_positions[HB_MAX_CONTEXT_LENGTH];
  };

  inline bool apply (hb_aat_apply_context_t *c) const
  {
    TRACE_APPLY (this);

    driver_context_t dc (this, c);

    StateTableDriver<EntryData> driver (machine, c->buffer, c->face);
    driver.drive (&dc);

    return_trace (dc.ret);
  }

  inline bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    /* The rest of array sanitizations are done at run-time. */
    return_trace (c->check_struct (this) && machine.sanitize (c) &&
		  ligAction && component && ligature);
  }

  protected:
  StateTable<EntryData>
		machine;
  LOffsetTo<UnsizedArrayOf<HBUINT32>, false>
		ligAction;	/* Offset to the ligature action table. */
  LOffsetTo<UnsizedArrayOf<HBUINT16>, false>
		component;	/* Offset to the component table. */
  LOffsetTo<UnsizedArrayOf<GlyphID>, false>
		ligature;	/* Offset to the actual ligature lists. */
  public:
  DEFINE_SIZE_STATIC (28);
};

struct NoncontextualSubtable
{
  inline bool apply (hb_aat_apply_context_t *c) const
  {
    TRACE_APPLY (this);

    bool ret = false;
    unsigned int num_glyphs = c->face->get_num_glyphs ();

    hb_glyph_info_t *info = c->buffer->info;
    unsigned int count = c->buffer->len;
    for (unsigned int i = 0; i < count; i++)
    {
      const GlyphID *replacement = substitute.get_value (info[i].codepoint, num_glyphs);
      if (replacement)
      {
	info[i].codepoint = *replacement;
	ret = true;
      }
    }

    return_trace (ret);
  }

  inline bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (substitute.sanitize (c));
  }

  protected:
  Lookup<GlyphID>	substitute;
  public:
  DEFINE_SIZE_MIN (2);
};

struct InsertionSubtable
{
  struct EntryData
  {
    HBUINT16	currentInsertIndex;	/* Zero-based index into the insertion glyph table.
					 * The number of glyphs to be inserted is contained
					 * in the currentInsertCount field in the flags.
					 * A value of 0xFFFF indicates no insertion is to
					 * be done. */
    HBUINT16	markedInsertIndex;	/* Zero-based index into the insertion glyph table.
					 * The number of glyphs to be inserted is contained
					 * in the markedInsertCount field in the flags.
					 * A value of 0xFFFF indicates no insertion is to
					 * be done. */
    public:
    DEFINE_SIZE_STATIC (4);
  };

  struct driver_context_t
  {
    static const bool in_place = false;
    enum Flags
    {
      SetMark		= 0x8000,	/* If set, mark the current glyph. */
      DontAdvance	= 0x4000,	/* If set, don't advance to the next glyph before
					 * going to the new state.  This does not mean
					 * that the glyph pointed to is the same one as
					 * before. If you've made insertions immediately
					 * downstream of the current glyph, the next glyph
					 * processed would in fact be the first one
					 * inserted. */
      CurrentIsKashidaLike= 0x2000,	/* If set, and the currentInsertList is nonzero,
					 * then the specified glyph list will be inserted
					 * as a kashida-like insertion, either before or
					 * after the current glyph (depending on the state
					 * of the currentInsertBefore flag). If clear, and
					 * the currentInsertList is nonzero, then the
					 * specified glyph list will be inserted as a
					 * split-vowel-like insertion, either before or
					 * after the current glyph (depending on the state
					 * of the currentInsertBefore flag). */
      MarkedIsKashidaLike= 0x1000,	/* If set, and the markedInsertList is nonzero,
					 * then the specified glyph list will be inserted
					 * as a kashida-like insertion, either before or
					 * after the marked glyph (depending on the state
					 * of the markedInsertBefore flag). If clear, and
					 * the markedInsertList is nonzero, then the
					 * specified glyph list will be inserted as a
					 * split-vowel-like insertion, either before or
					 * after the marked glyph (depending on the state
					 * of the markedInsertBefore flag). */
      CurrentInsertBefore= 0x0800,	/* If set, specifies that insertions are to be made
					 * to the left of the current glyph. If clear,
					 * they're made to the right of the current glyph. */
      MarkedInsertBefore= 0x0400,	/* If set, specifies that insertions are to be
					 * made to the left of the marked glyph. If clear,
					 * they're made to the right of the marked glyph. */
      CurrentInsertCount= 0x3E0,	/* This 5-bit field is treated as a count of the
					 * number of glyphs to insert at the current
					 * position. Since zero means no insertions, the
					 * largest number of insertions at any given
					 * current location is 31 glyphs. */
      MarkedInsertCount= 0x001F,	/* This 5-bit field is treated as a count of the
					 * number of glyphs to insert at the marked
					 * position. Since zero means no insertions, the
					 * largest number of insertions at any given
					 * marked location is 31 glyphs. */
    };

    inline driver_context_t (const InsertionSubtable *table,
			     hb_aat_apply_context_t *c_) :
	ret (false),
	c (c_),
	mark_set (false),
	mark (0),
	insertionAction (table+table->insertionAction) {}

    inline bool is_actionable (StateTableDriver<EntryData> *driver,
			       const Entry<EntryData> *entry)
    {
      return (entry->flags & (CurrentInsertCount | MarkedInsertCount)) &&
	     (entry->data.currentInsertIndex != 0xFFFF ||entry->data.markedInsertIndex != 0xFFFF);
    }
    inline bool transition (StateTableDriver<EntryData> *driver,
			    const Entry<EntryData> *entry)
    {
      hb_buffer_t *buffer = driver->buffer;
      unsigned int flags = entry->flags;

      if (entry->data.markedInsertIndex != 0xFFFF && mark_set)
      {
	unsigned int count = (flags & MarkedInsertCount);
	unsigned int start = entry->data.markedInsertIndex;
	const GlyphID *glyphs = &insertionAction[start];
	if (unlikely (!c->sanitizer.check_array (glyphs, count))) return false;

	bool before = flags & MarkedInsertBefore;

	unsigned int end = buffer->out_len;
	buffer->move_to (mark);

	if (!before)
	  buffer->copy_glyph ();
	/* TODO We ignore KashidaLike setting. */
	for (unsigned int i = 0; i < count; i++)
	  buffer->output_glyph (glyphs[i]);
	if (!before)
	  buffer->skip_glyph ();

	buffer->move_to (end + count);

	buffer->unsafe_to_break_from_outbuffer (mark, MIN (buffer->idx + 1, buffer->len));
      }

      if (entry->data.currentInsertIndex != 0xFFFF)
      {
	unsigned int count = (flags & CurrentInsertCount) >> 5;
	unsigned int start = entry->data.currentInsertIndex;
	const GlyphID *glyphs = &insertionAction[start];
	if (unlikely (!c->sanitizer.check_array (glyphs, count))) return false;

	bool before = flags & CurrentInsertBefore;

	unsigned int end = buffer->out_len;

	if (!before)
	  buffer->copy_glyph ();
	/* TODO We ignore KashidaLike setting. */
	for (unsigned int i = 0; i < count; i++)
	  buffer->output_glyph (glyphs[i]);
	if (!before)
	  buffer->skip_glyph ();

	/* Humm. Not sure where to move to.  There's this wording under
	 * DontAdvance flag:
	 *
	 * "If set, don't update the glyph index before going to the new state.
	 * This does not mean that the glyph pointed to is the same one as
	 * before. If you've made insertions immediately downstream of the
	 * current glyph, the next glyph processed would in fact be the first
	 * one inserted."
	 *
	 * This suggests that if DontAdvance is NOT set, we should move to
	 * end+count.  If it *was*, then move to end, such that newly inserted
	 * glyphs are now visible.
	 */
	buffer->move_to ((flags & DontAdvance) ? end : end + count);
      }

      if (flags & SetMark)
      {
	mark_set = true;
	mark = buffer->out_len;
      }

      return true;
    }

    public:
    bool ret;
    private:
    hb_aat_apply_context_t *c;
    bool mark_set;
    unsigned int mark;
    const UnsizedArrayOf<GlyphID> &insertionAction;
  };

  inline bool apply (hb_aat_apply_context_t *c) const
  {
    TRACE_APPLY (this);

    driver_context_t dc (this, c);

    StateTableDriver<EntryData> driver (machine, c->buffer, c->face);
    driver.drive (&dc);

    return_trace (dc.ret);
  }

  inline bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    /* The rest of array sanitizations are done at run-time. */
    return_trace (c->check_struct (this) && machine.sanitize (c) &&
		  insertionAction);
  }

  protected:
  StateTable<EntryData>
		machine;
  LOffsetTo<UnsizedArrayOf<GlyphID>, false>
		insertionAction;	/* Byte offset from stateHeader to the start of
					 * the insertion glyph table. */
  public:
  DEFINE_SIZE_STATIC (20);
};


struct Feature
{
  inline bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (c->check_struct (this));
  }

  public:
  HBUINT16	featureType;	/* The type of feature. */
  HBUINT16	featureSetting;	/* The feature's setting (aka selector). */
  HBUINT32	enableFlags;	/* Flags for the settings that this feature
				 * and setting enables. */
  HBUINT32	disableFlags;	/* Complement of flags for the settings that this
				 * feature and setting disable. */

  public:
  DEFINE_SIZE_STATIC (12);
};


struct ChainSubtable
{
  friend struct Chain;

  inline unsigned int get_size (void) const { return length; }
  inline unsigned int get_type (void) const { return coverage & SubtableType; }

  enum Coverage
  {
    Vertical		= 0x80000000,	/* If set, this subtable will only be applied
					 * to vertical text. If clear, this subtable
					 * will only be applied to horizontal text. */
    Descending		= 0x40000000,	/* If set, this subtable will process glyphs
					 * in descending order. If clear, it will
					 * process the glyphs in ascending order. */
    AllDirections	= 0x20000000,	/* If set, this subtable will be applied to
					 * both horizontal and vertical text (i.e.
					 * the state of bit 0x80000000 is ignored). */
    Logical		= 0x10000000,	/* If set, this subtable will process glyphs
					 * in logical order (or reverse logical order,
					 * depending on the value of bit 0x80000000). */
    Reserved		= 0x0FFFFF00,	/* Reserved, set to zero. */
    SubtableType	= 0x000000FF,	/* Subtable type; see following table. */
  };
  enum Type
  {
    Rearrangement	= 0,
    Contextual		= 1,
    Ligature		= 2,
    Noncontextual	= 4,
    Insertion		= 5
  };

  template <typename context_t>
  inline typename context_t::return_t dispatch (context_t *c) const
  {
    unsigned int subtable_type = get_type ();
    TRACE_DISPATCH (this, subtable_type);
    switch (subtable_type) {
    case Rearrangement:		return_trace (c->dispatch (u.rearrangement));
    case Contextual:		return_trace (c->dispatch (u.contextual));
    case Ligature:		return_trace (c->dispatch (u.ligature));
    case Noncontextual:		return_trace (c->dispatch (u.noncontextual));
    case Insertion:		return_trace (c->dispatch (u.insertion));
    default:			return_trace (c->default_return_value ());
    }
  }

  inline bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    if (!length.sanitize (c) ||
	length < min_size ||
	!c->check_range (this, length))
      return_trace (false);

    return_trace (dispatch (c));
  }

  protected:
  HBUINT32	length;		/* Total subtable length, including this header. */
  HBUINT32	coverage;	/* Coverage flags and subtable type. */
  HBUINT32	subFeatureFlags;/* The 32-bit mask identifying which subtable this is. */
  union {
  RearrangementSubtable		rearrangement;
  ContextualSubtable		contextual;
  LigatureSubtable		ligature;
  NoncontextualSubtable		noncontextual;
  InsertionSubtable		insertion;
  } u;
  public:
  DEFINE_SIZE_MIN (12);
};

struct Chain
{
  inline void apply (hb_aat_apply_context_t *c) const
  {
    uint32_t flags = defaultFlags;
    {
      /* Compute applicable flags.  TODO Should move this to planning
       * stage and take user-requested features into account. */
      unsigned int count = featureCount;
      for (unsigned i = 0; i < count; i++)
      {
        const Feature &feature = featureZ[i];
	if (false) /* XXX Check if feature enabled... */
	{
	  flags &= feature.disableFlags;
	  flags |= feature.enableFlags;
	}
      }
    }

    const ChainSubtable *subtable = &StructAtOffset<ChainSubtable> (&featureZ, featureZ[0].static_size * featureCount);
    unsigned int count = subtableCount;
    for (unsigned int i = 0; i < count; i++)
    {
      bool reverse;

      if (!(subtable->subFeatureFlags & flags))
        goto skip;

      if (!(subtable->coverage & ChainSubtable::AllDirections) &&
	  HB_DIRECTION_IS_VERTICAL (c->buffer->props.direction) !=
	  bool (subtable->coverage & ChainSubtable::Vertical))
        goto skip;

      /* Buffer contents is always in logical direction.  Determine if
       * we need to reverse before applying this subtable.  We reverse
       * back after if we did reverse indeed.
       *
       * Quoting the spac:
       * """
       * Bits 28 and 30 of the coverage field control the order in which
       * glyphs are processed when the subtable is run by the layout engine.
       * Bit 28 is used to indicate if the glyph processing direction is
       * the same as logical order or layout order. Bit 30 is used to
       * indicate whether glyphs are processed forwards or backwards within
       * that order.

		Bit 30	Bit 28	Interpretation for Horizontal Text
		0	0	The subtable is processed in layout order
				(the same order as the glyphs, which is
				always left-to-right).
		1	0	The subtable is processed in reverse layout order
				(the order opposite that of the glyphs, which is
				always right-to-left).
		0	1	The subtable is processed in logical order
				(the same order as the characters, which may be
				left-to-right or right-to-left).
		1	1	The subtable is processed in reverse logical order
				(the order opposite that of the characters, which
				may be right-to-left or left-to-right).
       */
      reverse = subtable->coverage & ChainSubtable::Logical ?
		bool (subtable->coverage & ChainSubtable::Descending) :
		bool (subtable->coverage & ChainSubtable::Descending) !=
		HB_DIRECTION_IS_BACKWARD (c->buffer->props.direction);

      if (!c->buffer->message (c->font, "start chain subtable %d", c->lookup_index))
        goto skip;

      if (reverse)
        c->buffer->reverse ();

      subtable->dispatch (c);

      if (reverse)
        c->buffer->reverse ();

      (void) c->buffer->message (c->font, "end chain subtable %d", c->lookup_index);

      if (unlikely (!c->buffer->successful)) return;

    skip:
      subtable = &StructAfter<ChainSubtable> (*subtable);
      c->set_lookup_index (c->lookup_index + 1);
    }
  }

  inline unsigned int get_size (void) const { return length; }

  inline bool sanitize (hb_sanitize_context_t *c, unsigned int version) const
  {
    TRACE_SANITIZE (this);
    if (!length.sanitize (c) ||
	length < min_size ||
	!c->check_range (this, length))
      return_trace (false);

    if (!c->check_array (featureZ.arrayZ, featureCount))
      return_trace (false);

    const ChainSubtable *subtable = &StructAtOffset<ChainSubtable> (&featureZ, featureZ[0].static_size * featureCount);
    unsigned int count = subtableCount;
    for (unsigned int i = 0; i < count; i++)
    {
      if (!subtable->sanitize (c))
	return_trace (false);
      subtable = &StructAfter<ChainSubtable> (*subtable);
    }

    return_trace (true);
  }

  protected:
  HBUINT32	defaultFlags;	/* The default specification for subtables. */
  HBUINT32	length;		/* Total byte count, including this header. */
  HBUINT32	featureCount;	/* Number of feature subtable entries. */
  HBUINT32	subtableCount;	/* The number of subtables in the chain. */

  UnsizedArrayOf<Feature>	featureZ;	/* Features. */
/*ChainSubtable	firstSubtable;*//* Subtables. */
/*subtableGlyphCoverageArray*/	/* Only if version >= 3. We don't use. */

  public:
  DEFINE_SIZE_MIN (16);
};


/*
 * The 'mort'/'morx' Tables
 */

struct morx
{
  static const hb_tag_t tableTag = HB_AAT_TAG_morx;

  inline void apply (hb_aat_apply_context_t *c) const
  {
    if (unlikely (!c->buffer->successful)) return;
    c->set_lookup_index (0);
    const Chain *chain = &firstChain;
    unsigned int count = chainCount;
    for (unsigned int i = 0; i < count; i++)
    {
      chain->apply (c);
      if (unlikely (!c->buffer->successful)) return;
      chain = &StructAfter<Chain> (*chain);
    }
  }

  inline bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    if (!version.sanitize (c) || version < 2 ||
	!chainCount.sanitize (c))
      return_trace (false);

    const Chain *chain = &firstChain;
    unsigned int count = chainCount;
    for (unsigned int i = 0; i < count; i++)
    {
      if (!chain->sanitize (c, version))
	return_trace (false);
      chain = &StructAfter<Chain> (*chain);
    }

    return_trace (true);
  }

  protected:
  HBUINT16	version;	/* Version number of the glyph metamorphosis table.
				 * 2 or 3. */
  HBUINT16	unused;		/* Set to 0. */
  HBUINT32	chainCount;	/* Number of metamorphosis chains contained in this
				 * table. */
  Chain		firstChain;	/* Chains. */

  public:
  DEFINE_SIZE_MIN (8);
};

} /* namespace AAT */


#endif /* HB_AAT_LAYOUT_MORX_TABLE_HH */
