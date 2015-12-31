/*
 *  This file is part of seq24/sequencer64.
 *
 *  seq24 is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  seq24 is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with seq24; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/**
 * \file          eventslots.cpp
 *
 *  This module declares/defines the base class for displaying events in their
 *  editing slotss.
 *
 * \library       sequencer64 application
 * \author        Chris Ahlstrom
 * \date          2015-12-05
 * \updates       2015-12-31
 * \license       GNU GPLv2 or above
 *
 *  This module is user-interface code.  It is loosely based on the workings
 *  of the perfnames class.
 *
 *  Now, we have an issue when loading one of the larger sequences in our main
 *  test tune, where X stops the application and Gtk says it got a bad memory
 *  allocation.  So we need to page through the sequence.
 */

#include <gtkmm/adjustment.h>

#include "click.hpp"                    /* SEQ64_CLICK_LEFT(), etc.    */
#include "font.hpp"
#include "eventedit.hpp"
#include "perform.hpp"
#include "eventslots.hpp"

namespace seq64
{

/**
 *  Principal constructor for this user-interface object.
 */

eventslots::eventslots
(
    perform & p,
    eventedit & parent,
    sequence & seq,
    Gtk::Adjustment & vadjust
) :
    gui_drawingarea_gtk2    (p, adjustment_dummy(), vadjust, 360, 10),
    m_parent                (parent),
    m_seq                   (seq),
    m_event_container       (seq, p.get_beats_per_minute()),
    m_slots_chars           (64),                               // 24
    m_char_w                (font_render().char_width()),
    m_setbox_w              (m_char_w),                         // * 2
    m_slots_box_w           (m_char_w * 62),                    // 22
    m_slots_x               (m_slots_chars * m_char_w),
    m_slots_y               (font_render().char_height() + 4),  // c_names_y
    m_xy_offset             (2),
    m_event_count           (0),
    m_line_count            (0),
    m_line_maximum          (43),   /* need a way to calculate this value   */
    m_line_overlap          (5),
    m_top_index             (0),    /* (SEQ64_NULL_EVENT_INDEX),            */
    m_current_index         (SEQ64_NULL_EVENT_INDEX),
    m_top_iterator          (),
    m_bottom_iterator       (),
    m_current_iterator      (),
    m_pager_index           (0)
{
    load_events();
    grab_focus();
}

/**
 *  Grabs the event list from the sequence and uses it to fill the
 *  editable-event list.  Determines how many events can be shown in the
 *  GUI [later] and adjusts the top and bottom editable-event iterators to
 *  show the first page of events.
 *
 * \return
 *      Returns true if the event iterators were able to be set up as valid.
 */

bool
eventslots::load_events ()
{
    bool result = m_event_container.load_events();
    if (result)
    {
        m_event_count = m_event_container.count();
        if (m_event_count > 0)
        {
            m_line_count = m_line_maximum;
            if (m_event_count < m_line_count)
                m_line_count = m_event_count;

            m_current_iterator = m_bottom_iterator =
                m_top_iterator = m_event_container.begin();

            for (int i = 0; i < m_line_count - 1; ++i)
            {
                if (increment_bottom() == SEQ64_NULL_EVENT_INDEX)
                    break;
            }
        }
        else
            result = false;
    }
    if (! result)
    {
        m_line_count = 0;
        m_current_iterator = m_bottom_iterator =
            m_top_iterator = m_event_container.end();
    }
    return result;
}

/**
 *  Set the current event, which is the event highlighted in yellow.  Note in
 *  the snprintf() calls that the first digit is part of the data byte, so
 *  that translation is easier.
 *
 * \param ei
 *      The iterator that points to the event.
 *
 * \param
 *      The index (re 0) of the event, starting at m_event_container.begin().
 */

void
eventslots::set_current_event (const editable_events::iterator ei, int index)
{
    char tmp[32];
    midibyte d0, d1;
    const editable_event & ev = ei->second;
    ev.get_data(d0, d1);
    snprintf(tmp, sizeof tmp, "%d (0x%02x)", int(d0), int(d0));
    std::string data_0(tmp);
    snprintf(tmp, sizeof tmp, "%d (0x%02x)", int(d1), int(d1));
    std::string data_1(tmp);
    set_text
    (
        ev.category_string(), ev.timestamp_string(), ev.status_string(),
        data_0, data_1
    );
    m_current_index = index;
    m_current_iterator = ei;
    enqueue_draw();
}

/**
 *  Sets the text in the parent dialog, eventedit.
 *
 * \param evtimestamp
 *      The event time-stamp to be set in the parent.
 *
 * \param evname
 *      The event name to be set in the parent.
 *
 * \param evdata0
 *      The first event data byte to be set in the parent.
 *
 * \param evdata1
 *      The second event data byte to be set in the parent.
 */

void
eventslots::set_text
(
    const std::string & evcategory,
    const std::string & evtimestamp,
    const std::string & evname,
    const std::string & evdata0,
    const std::string & evdata1
)
{
    m_parent.set_event_timestamp(evtimestamp);
    m_parent.set_event_category(evcategory);
    m_parent.set_event_name(evname);
    m_parent.set_event_data_0(evdata0);
    m_parent.set_event_data_1(evdata1);
}

/**
 *  Inserts an event.
 *
 *  What actually happens here depends if the new event is before the frame,
 *  within the frame, or after the frame, based on the timestamp.
 *
 *  If before the frame: To keep the previous events visible, we do not need
 *  to increment the iterators (insertion does not affect multimap iterators),
 *  but we do need to increment their indices.  The contents shown in the frame
 *  should not change.
 *
 *  If at the frame top:  The new timestamp equals the top timestamp. We don't
 *  know exactly where the new event goes in the multimap, but we do have an
 *  new event.
 *
 *  If at the frame bottom:  TODO
 *
 *  If after the frame: No action needed if the bottom event is actually at
 *  the bottom of the frame.  But if the frame is not yet filled, we need to
 *  increment the bottom iterator, and its index.
 *
 * \note
 *      Actually, it is far easier to just adjust all the counts and iterators
 *      and redraw the screen, as done by the page_topper() function.
 *
 * \param ev
 *      The event to insert, prebuilt.
 *
 * \return
 *      Returns true if the event was inserted.
 */

bool
eventslots::insert_event (const editable_event & edev)
{
    bool result = m_event_container.add(edev);
    if (result)
    {
        m_event_count = m_event_container.count();
        if (m_event_count == 1)
        {
            m_top_index = 0;
            m_current_index = 0;
            m_top_iterator = m_current_iterator =
                m_bottom_iterator = m_event_container.begin();

            if (result)
                select_event(m_current_index);
        }
        else
        {
            /*
             * This iterator is a short-lived [changed after the next add()
             * call] pointer to the added event.
             */

            editable_events::iterator nev = m_event_container.current_event();

#ifdef USE_THIS_CODE
            midipulse top_ts = m_top_iterator->second.get_timestamp();
            midipulse bot_ts = m_bottom_iterator->second.get_timestamp();
            midipulse new_ts = edev.get_timestamp();
            if (new_ts < top_ts)                    /* before the frame     */
            {
            }
            if (new_ts == top_ts)                   /* at the frame top     */
            {
            }
            else if (new_ts == bot_ts)              /* at the frame bottom  */
            {
            }
            else if (new_ts > bot_ts)               /* after the frame      */
            {
            }
            else                                    /* within the frame     */
            {
            }
#endif  // USE_THIS_CODE

            page_topper(nev);
        }

    }
    return result;
}

/**
 *  Inserts an event based on the setting provided, which the eventedit object
 *  gets from its Entry fields.  It calls the other insert_event() overload.
 *
 *  Note that we need to qualify the temporary event class object we create
 *  below, with the seq64 namespace, otherwise the compiler thinks we're
 *  trying to access some Gtkmm thing.
 *
 * \param evtimestamp
 *      The time-stamp of the new event, as obtained from the event-edit
 *      timestamp field.
 *
 * \param evname
 *      The type name (status name)  of the new event, as obtained from the
 *      event-edit event-name field.
 *
 * \param evdata0
 *      The first data byte of the new event, as obtained from the event-edit
 *      data 1 field.
 *
 * \param evdata1
 *      The second data byte of the new event, as obtained from the event-edit
 *      data 2 field.  Used only for two-parameter events.
 *
 * \return
 *      Returns true if the event was inserted.
 */

bool
eventslots::insert_event
(
    const std::string & evtimestamp,
    const std::string & evname,
    const std::string & evdata0,
    const std::string & evdata1
)
{
    seq64::event e;                                 /* new default event    */
    editable_event edev(m_event_container, e);
    edev.set_status_from_string(evtimestamp, evname, evdata0, evdata1);
    edev.set_channel(m_seq.get_midi_channel());
    return insert_event(edev);
}

/**
 *  Deletes the current event, and makes adjustments due to that deletion.
 *
 *  To delete the current event, this function moves the current iterator to
 *  the next event, deletes the previously-current iterator, adjusts the event
 *  count and the bottom iterator, and redraws the pixmap.  The exact changes
 *  depend upon whether the deleted event was at the top of the visible frame,
 *  within the visible frame, or at the bottom the visible frame.  Note that
 *  only visible events can be the current event, and thus get deleted.
 *
\verbatim
         Event Index
          0
          1
          2         Top
          3  <--------- Top case: The new top iterator, index becomes 2
          4
          .
          .         Inside of Visible Frame
          .
         43
         44         Bottom
         45  <--------- Top case: The new bottom iterator, index becomes 44
         46             Bottom case: Same result
\endverbatim
 *
 *  Basically, when an event is deleted, the frame (delimited by the
 *  event-index members) stays in place, while the frame iterators move to the
 *  previous event.  If the top of the frame would move to before the first
 *  event, then the frame must shrink.
 *
 *  Top case: If the current iterator is the top (of the frame) iterator, then
 *  the top iterator needs to be incremented.  The new top event has the same
 *  index as the now-gone top event. The index of the bottom event is
 *  decremented, since an event before it is now gone.  The bottom iterator
 *  moves to the next event, which is now at the bottom of the frame.  The
 *  current event is treated like the top event.
 *
 *  Inside case: If the current iterator is in the middle of the frame, the top
 *  iterator and index remain unchanged.  The current iterator is incremented,
 *  but its index is now the same as the old bottom index.  Same for the bottom
 *  iterator.
 *
 *  Bottom case: If the current iterator (and bottom iterator) point to the
 *  last event in the frame, then both of them need to be
 *  decremented.  The frame needs to be moved up by one event, so that the
 *  current event remains at the bottom (it's just simpler to manage that way).
 *
 *  If there is no event after the bottom of the frame, the iterators that now
 *  point to end() must backtrack one event.  If the container becomes empty,
 *  then everything is invalidated.
 *
 * \return
 *      Returns true if the delete was possible.  If the container was empty
 *      or became empty, then false is returned.
 */

bool
eventslots::delete_current_event ()
{
    bool result = m_event_count > 0;
    if (result)
        result = m_current_iterator != m_event_container.end();

    if (result)
    {
        editable_events::iterator oldcurrent = m_current_iterator;
        int oldcount = m_event_container.count();
        if (oldcount > 1)
        {
            if (m_current_index == 0)           // m_top_index)
            {
                (void) increment_top();         /* bypass to-delete event   */
                (void) increment_current();     /* ditto                    */
                (void) increment_bottom();      /* next event up to bottom  */
            }
            else if (m_current_index == (m_line_count - 1))
            {
                /*
                 * If we are before the last event in the event container, we
                 * can increment the iterators.  Otherwise, we have to back
                 * off by one so we are above the last event, which will be
                 * deleted.
                 */

                if (m_current_index < (m_event_count - 1))
                {
                    (void) increment_current();
                    (void) increment_bottom();
                }
                else
                {
                    /*
                     * The frame must shrink.
                     */

                    m_current_index = decrement_current();
                    (void) decrement_bottom();
                    if (m_line_count > 0)
                        --m_line_count;
                }
            }
            else
            {
                (void) increment_current();
                (void) increment_bottom();
            }
        }

        /*
         * Has to be done after the adjustment, otherwise iterators are
         * invalid and cannot be adjusted.
         */

        m_event_container.remove(oldcurrent);   /* wrapper for erase()      */

        int newcount = m_event_container.count();
        if (newcount == 0)
        {
            m_top_index = 0;
            m_current_index = 0;
            m_top_iterator = m_current_iterator =
                m_bottom_iterator = m_event_container.end();
        }

        bool ok = newcount == (oldcount - 1);
        if (ok)
        {
            m_event_count = newcount;
            result = newcount > 0;
            if (result)
                select_event(m_current_index);
            else
                select_event(SEQ64_NULL_EVENT_INDEX);
        }
    }
    return result;
}

/**
 *  Modifies the data in the currently-selected event.  If the timestamp has
 *  changed, however, we can't just modify the event in place.  Instead, we
 *  finish modifying the event, but tell the caller to delete and reinsert the
 *  new event (in its proper new location based on timestamp).
 *
 * \param evtimestamp
 *      Provides the new event time-stamp as edited by the user.
 *
 * \param evname
 *      Provides the event name as edited by the user.
 *
 * \param evdata0
 *      Provides the time-stamp as edited by the user.
 *
 * \param evtimestamp
 *      Provides the time-stamp as edited by the user.
 *
 * \return
 *      Returns true simply if the event-count is greater than 0.
 */

bool
eventslots::modify_current_event
(
    const std::string & evtimestamp,
    const std::string & evname,
    const std::string & evdata0,
    const std::string & evdata1
)
{
    bool result = m_event_count > 0;
    if (result)
    {
        editable_event & ev = m_current_iterator->second;
        midipulse oldtimestamp = ev.get_timestamp();
        bool same_time = ev.get_timestamp() == oldtimestamp;
        ev.set_status_from_string(evtimestamp, evname, evdata0, evdata1);
        ev.set_channel(m_seq.get_midi_channel());
        if (! same_time)
        {
            /*
             * Copy the modified event, delete the original, and insert the
             * modified event into the editable-event container.
             */

            editable_event ev_copy = ev;
            result = delete_current_event();
            if (result)
                result = m_event_container.add(ev);
        }
        if (result)
        {
            /*
             * Does this work?
             */

            select_event(m_current_index);
        }
    }
    return result;
}

/**
 *  Writes the events back to the sequence.  Also sets the dirty flag for the
 *  sequence, via the sequence::add_event() function, but this doesn't seem to
 *  set the perform dirty flag.  So now we pass the modification buck to the
 *  parent, who passes it to the perform object.
 *
 * \return
 *      Returns true if the operations succeeded.
 */

bool
eventslots::save_events ()
{
    bool result = m_event_count > 0 && m_event_count == m_event_container.count();
    if (result)
    {
        event_list & seqevents = m_seq.events();
        seqevents.clear();
        for
        (
            editable_events::iterator ei = m_event_container.begin();
            ei != m_event_container.end();
            ++ei
        )
        {
            seq64::event e = ei->second;
            m_seq.add_event(e);
        }
        result = m_seq.event_count() == m_event_count;
        if (result)
            m_parent.modify();
    }
    return result;
}

/**
 *  Change the vertical offset of events.  Note that m_vadjust is
 *  the Gtk::Adjustment object that the eventedit parent passes to the
 *  gui_drawingarea_gtk2 constructor.
 *
 *  The top-event and bottom-event indices (and their corresponding
 *  editable-event iterators) delimit the part of the event container that is
 *  displayed in the eventslots user-interface.  The top-event index starts at
 *  0, and the bottom-event is larger (initially, by 42 slots).
 *
 *  When the scroll-bar thumb moves up or down, we need to change both event
 *  indices and both event iterators by the corresponding amount.  Luckily,
 *  the std::multimap iterator is bidirectional.
 *
 *  Note that we may need to reduce the movement of events to a value less
 *  than a page; it can be limited backwards by the value of the top index,
 *  and forward by the value of the bottom index.
 */

void
eventslots::change_vert ()
{
    int new_value = m_vadjust.get_value();
#ifdef SEQ64_USE_DEBUG_OUTPUT               /* PLATFORM_DEBUG               */
    infoprintf("New page --> %d\n", new_value);
#endif
    if (new_value != m_pager_index)
        page_movement(new_value);
}

/**
 *  Adjusts the vertical position of the frame according to the given new
 *  scrollbar/vadjust value.  The adjustment is done via movement from the
 *  current position.
 *
 *  Do we even need a way to detect excess movement?  The scrollbar, if
 *  properly set up, should never move the frame too high or too low.
 *  Verified by testing.
 *
 * \param new_value
 *      Provides the new value of the scrollbar position.
 */

void
eventslots::page_movement (int new_value)
{
    int movement = new_value - m_pager_index;       /* can be negative */
    int absmovement = movement >= 0 ? movement : -movement;
    m_pager_index = new_value;
    if (movement != 0)
    {
        m_top_index += movement;                // or just SET IT?
        if (movement > 0)
        {
            for (int i = 0; i < movement; ++i)
            {
                (void) increment_top();
                (void) increment_bottom();
            }
        }
        else if (movement < 0)
        {
            for (int i = 0; i < absmovement; ++i)
            {
                (void) decrement_top();
                (void) decrement_bottom();
            }
        }

        /*
         * Don't move the current event (highlighted in yellow) unless
         * we move more than one event.  Annoying to the user.
         */

        if (absmovement > 1)
            set_current_event(m_top_iterator, 0);   // m_top_index);
        else
        {
            set_current_event
            (
                m_current_iterator, m_current_index + movement
            );
        }
    }
}

/**
 *  Adjusts the vertical position of the frame according to the given new
 *  bottom iterator.  The adjustment is done "from scratch".  We've found page
 *  movement to be an insoluable problem in some editing circumstances.  So
 *  now we move to the inserted event, and make it the top
 *
 *  However, moving an inserted event to the top is a bit annoying.  So now we
 *  backtrack so that the inserted event is at the bottom.
 *
 * \param newcurrent
 *      Provides the iterator to the event to be shown at the bottom of the
 *      frame.
 */

void
eventslots::page_topper (editable_events::iterator newcurrent)
{
    bool ok = newcurrent != m_event_container.end();
    if (ok)
        ok = m_event_count > 0;

    if (ok)
    {
        editable_events::iterator ei = m_event_container.begin();
        int botindex = 0;
        while (ei != newcurrent)
        {
            ++botindex;
            ++ei;
            if (botindex == m_event_count)
            {
                ok = false;                     /* never found the event!   */
                break;
            }
        }
        if (m_event_count <= m_line_maximum)    /* fewer events than lines  */
        {
            if (ok)
            {
                m_pager_index = 0;
                m_top_index = 0;
                m_top_iterator = m_event_container.begin();
                m_line_count = m_event_count;
                m_current_iterator = newcurrent;
                m_current_index = botindex;
            }
        }
        else
        {
            m_line_count = line_maximum();
            if (ok)
            {
                /*
                 * Backtrack by up to m_line_maximum events so that the new
                 * event is shown at the bottom or at its natural location; it
                 * also needs to be the current event, for highlighting.
                 * Count carefully!
                 */

                editable_events::iterator ei = m_event_container.begin();
                int pageup = botindex - line_maximum();
                if (pageup < 0)
                {
                    m_top_index = m_pager_index = pageup = 0;
                }
                else
                {
                    int countdown = pageup;
                    while (countdown-- > 0)
                        ++ei;

                    m_top_index = m_pager_index = pageup + 1;   /* re map   */
                }

                m_top_iterator = ei;
                m_current_iterator = newcurrent;
                m_current_index = botindex - m_top_index;       /* re frame */
            }
        }
        if (ok)
            select_event(m_current_index);
    }
}

/**
 *  Wraps queue_draw().
 */

void
eventslots::enqueue_draw ()
{
    draw_events();
    queue_draw();
}

/**
 *  Draw the given slot/event.  The slot contains the event details in
 *  (so far) one line of text in the box:
 *
 *  | timestamp | event kind | channel
 *      | data 0 name + value
 *      | data 1 name + value
 *
 *  Currently, this view shows only events that get copied to the sequence's
 *  event list.  This rules out the following items from the view:
 *
 *      -   MThd (song header)
 *      -   MTrk and Meta TrkEnd (track marker, a sequence has only one track)
 *      -   SeqNr (sequence number)
 *      -   SeqSpec (but there are three that might appear, see below)
 *      -   Meta TrkName
 *
 *  The events that are shown in this view are:
 *
 *      -   One-data-value events:
 *          -   Program Change
 *          -   Channel Pressure
 *      -   Two-data-value events:
 *          -   Note Off
 *          -   Note On
 *          -   Aftertouch
 *          -   Control Change
 *          -   Pitch Wheel
 *      -   Other:
 *          -   SysEx events, with partial show of data bytes
 *          -   SeqSpec events (TBD):
 *              -   Key
 *              -   Scale
 *              -   Background sequence
 *
 *  The index of the event is shown in the editor portion of the eventedit
 *  dialog.
 */

void
eventslots::draw_event (editable_events::iterator ei, int index)
{
    int yloc = m_slots_y * index;
//  Color fg = grey();
    font::Color col = font::BLACK;
    if (index == m_current_index)
    {
#if USE_YELLOW_AS_CURRENT
//      fg = yellow();
        col = font::BLACK_ON_YELLOW;
#else
//      fg = black();
        col = font::CYAN_ON_BLACK;
#endif
    }
#ifdef USE_FUTURE_CODE
    else if (false)     // if (a sysex event or selected event range)
    {
//      fg = dark_cyan();
        col = font::BLACK_ON_CYAN;
    }
#endif
//  else
//      fg = white();

    editable_event & evp = ei->second;
    char tmp[16];
    snprintf(tmp, sizeof tmp, "%4d-", m_top_index+index);
    std::string temp = tmp;
    temp += evp.stock_event_string();
    temp += "   ";                                          /* coloring */
    draw_rectangle(light_grey(), 0, yloc, m_slots_x, 1);
    render_string(0, yloc+2, temp, col);
}

/**
 *  Converts a y-value into an event index relative to 0 (the top of the
 *  eventslots window/pixmap) and returns it.
 *
 * \param y
 *      The y coordinate of the position of the mouse click in the eventslot
 *      window/pixmap.
 *
 * \return
 *      Returns the index of the event position in the user-interface, which
 *      should range from 0 to m_line_count.
 */

int
eventslots::convert_y (int y)
{
    int eventindex = y / m_slots_y;
    if (eventindex >= m_line_count)
        eventindex = m_line_count;
    else if (eventindex < 0)
        eventindex = 0;

    return eventindex;
}

/**
 *  Draws all of the events in the current eventslots frame.
 *  It first clears the whole bitmap to white, so that no artifacts from the
 *  previous state of the frame are left behind.
 *
 *  Need to figure out how to calculate the number of displayable events.
 *
 *      m_line_maximum = ???
 */

void
eventslots::draw_events ()
{
    int x = 0;
    int y = 1;                                      // tweakage
    int lx = m_slots_x;                             //  - 3 - m_setbox_w;
    int ly = m_slots_y * m_line_maximum;           // 42
    draw_rectangle(white(), x, y, lx, ly);          // clear the frame
    if (m_event_count > 0)                          // m_line_maximum > 0
    {
        editable_events::iterator ei = m_top_iterator;
        for (int ev = 0; ev < m_line_count; ++ev)
        {
            if (ei != m_event_container.end())
            {
                draw_event(ei, ev);
                ++ei;
            }
            else
                break;
        }
    }
}

/**
 *  Selects and highlight the event that is located in the frame at the given
 *  event index.  The event index is provided by converting the y-coordinate
 *  of the mouse pointer into a slot number, and then an event index (actually
 *  the slot-distance from the m_top_iterator.  Confusing, yes no?
 *
 *  Note that, if the event index is negative, then we just queue up a draw
 *  operation, which should paint an empty frame -- the event container is
 *  empty.
 *
 * \param event_index
 *      Provides the numeric index of the event in the event frame, or
 *      SEQ64_NULL_EVENT if there is no event to draw.
 */

void
eventslots::select_event (int event_index)
{
    bool ok = event_index != SEQ64_NULL_EVENT_INDEX;
    if (ok)
        ok = event_index < m_line_count;

    if (ok)
    {
        editable_events::iterator ei = m_top_iterator;
        ok = ei != m_event_container.end();
        if (ok && (event_index > 0))        // m_top_index)
        {
            int i = 0;                      // m_top_index;
            while (i++ < event_index)
            {
                ++ei;
                ok = ei != m_event_container.end();
                if (! ok)
                    break;
            }
        }
        if (ok)
            set_current_event(ei, event_index);
    }
    else
        enqueue_draw();                 /* for drawing an empty frame */
}

/**
 *  Decrements the top iterator, if possible.
 *
 * \return
 *      Returns 0, or SEQ64_NULL_EVENT_INDEX if the iterator could not be
 *      decremented.
 */

int
eventslots::decrement_top ()
{
    if (m_top_iterator != m_event_container.begin())
    {
        --m_top_iterator;
        return 0;                       // m_top_index - 1;
    }
    else
        return SEQ64_NULL_EVENT_INDEX;
}

/**
 *  Increments the top iterator, if possible.  Also handles the top-event
 *  index, so that the GUI can display the proper event numbers.
 *
 * \return
 *      Returns the top index, or SEQ64_NULL_EVENT_INDEX if the iterator could
 *      not be incremented, or would increment to the end of the container.
 */

int
eventslots::increment_top ()
{
    editable_events::iterator ei = m_top_iterator;
    if (ei != m_event_container.end())
    {
        ++ei;
        if (ei != m_event_container.end())
        {
            m_top_iterator = ei;
            return m_top_index + 1;
        }
        else
            return SEQ64_NULL_EVENT_INDEX;
    }
    else
        return SEQ64_NULL_EVENT_INDEX;
}

/**
 *  Decrements the current iterator, if possible.
 *
 * \return
 *      Returns the decremented index, or SEQ64_NULL_EVENT_INDEX if the
 *      iterator could not be decremented.  Remember that the index ranges
 *      only from 0 to m_line_count-1, and that is enforced here.
 */

int
eventslots::decrement_current ()
{
    if (m_current_iterator != m_event_container.begin())
    {
        --m_current_iterator;
        int result = m_current_index - 1;
        if (result < 0)
            result = 0;

        return result;
    }
    else
        return SEQ64_NULL_EVENT_INDEX;
}

/**
 *  Increments the current iterator, if possible.
 *
 * \return
 *      Returns the incremented index, or SEQ64_NULL_EVENT_INDEX if the
 *      iterator could not be incremented.  Remember that the index ranges
 *      only from 0 to m_line_count-1, and that is enforced here.
 */

int
eventslots::increment_current ()
{
    if (m_current_iterator != m_event_container.end())
    {
        ++m_current_iterator;
        int result = m_current_index + 1;
        if (result >= m_line_count)
            result = m_line_count - 1;

        return result;
    }
    else
        return SEQ64_NULL_EVENT_INDEX;
}

/**
 *  Decrements the bottom iterator, if possible.
 *
 * \return
 *      Returns 0, or SEQ64_NULL_EVENT_INDEX if the iterator could not be
 *      decremented.
 */

int
eventslots::decrement_bottom ()
{
    if (m_bottom_iterator != m_event_container.begin())
    {
        --m_bottom_iterator;
        return 0;
    }
    else
        return SEQ64_NULL_EVENT_INDEX;
}

/**
 *  Increments the bottom iterator, if possible.  There is an issue in paging
 *  down using the scrollbar where, at the bottom of the scrolling, the bottom
 *  iterator ends up bad.  Not yet sure how this happens, so for now we
 *  backtrack one event if this happens.
 *
 * \return
 *      Returns the incremented index, or SEQ64_NULL_EVENT_INDEX if the
 *      iterator could not be incremented.
 */

int
eventslots::increment_bottom ()
{
    int result = SEQ64_NULL_EVENT_INDEX;
    if (m_bottom_iterator != m_event_container.end())
    {
        editable_events::iterator old = m_bottom_iterator++;
        if (m_bottom_iterator != m_event_container.end())
            result = 0;
        else
        {
            m_bottom_iterator = old;        /* backtrack to initial value   */
#ifdef SEQ64_USE_DEBUG_OUTPUT               /* PLATFORM_DEBUG               */
            errprint("increment to bad bottom iterator");
#endif
        }
    }
    return result;
}

/**
 *  Handles the callback when the window is realized.  It first calls the
 *  base-class version of on_realize().  Then it allocates any additional
 *  resources needed.
 */

void
eventslots::on_realize ()
{
    gui_drawingarea_gtk2::on_realize();
    m_pixmap = Gdk::Pixmap::create
    (
        m_window, m_slots_x, m_slots_y * m_line_maximum + 1, -1
    );
    m_vadjust.signal_value_changed().connect        /* moved from ctor */
    (
        mem_fun(*(this), &eventslots::change_vert)
    );

    /*
     *  Weird, we have to do this dance to get the all of the edit fields to
     *  be filled when the dialog first appears on-screen.
     */

    if (m_event_count > 0)
    {
        select_event(0);
        if (m_event_count > 1)
        {
            select_event(1);
            select_event(0);
        }
    }
}

/**
 *  Handles an on-expose event.  It draws all of the sequences.
 */

bool
eventslots::on_expose_event (GdkEventExpose *)
{
    draw_events();
    return true;
}

/**
 *  Provides the callback for a button press, and it handles only a left
 *  mouse button.
 */

bool
eventslots::on_button_press_event (GdkEventButton * ev)
{
    int y = int(ev->y);
    int eventindex = convert_y(y);
    if (SEQ64_CLICK_LEFT(ev->button))
    {
        select_event(eventindex);
    }
    return true;
}

/**
 *  Handles a button-release for the right button, bringing up a popup
 *  menu.
 */

bool
eventslots::on_button_release_event (GdkEventButton * p0)
{
//  if (SEQ64_CLICK_RIGHT(p0->button))
//      popup_menu();

    return false;
}

/**
 *  This callback is an attempt to get keyboard focus into the eventslots
 *  pixmap area.  See the same function in the perfroll module.
 */

bool
eventslots::on_focus_in_event (GdkEventFocus *)
{
    set_flags(Gtk::HAS_FOCUS);
    return false;
}

/**
 *  This callback handles an out-of-focus event by resetting the flag
 *  HAS_FOCUS.
 */

bool
eventslots::on_focus_out_event (GdkEventFocus *)
{
    unset_flags(Gtk::HAS_FOCUS);
    return false;
}


/**
 *  Trial balloon for keystroke actions.
 */

bool
eventslots::on_key_press_event (GdkEventKey * ev)
{
    infoprint("KEYSTROKE!");
    return true;
}

/**
 *  Handle the scrolling of the window.
 */

bool
eventslots::on_scroll_event (GdkEventScroll * ev)
{
    double val = m_vadjust.get_value();
    if (CAST_EQUIVALENT(ev->direction, SEQ64_SCROLL_UP))
        val -= m_vadjust.get_step_increment();
    else if (CAST_EQUIVALENT(ev->direction, SEQ64_SCROLL_DOWN))
        val += m_vadjust.get_step_increment();

    m_vadjust.clamp_page(val, val + m_vadjust.get_page_size());
    return true;
}

/**
 *  Handles a size-allocation event.  It first calls the base-class
 *  version of this function.
 */

void
eventslots::on_size_allocate (Gtk::Allocation & a)
{
    gui_drawingarea_gtk2::on_size_allocate(a);
    m_window_x = a.get_width();                     /* side-effect  */
    m_window_y = a.get_height();                    /* side-effect  */
}

}           // namespace seq64

/*
 * eventslots.cpp
 *
 * vim: sw=4 ts=4 wm=4 et ft=cpp
 */

