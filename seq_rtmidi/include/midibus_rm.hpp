#ifndef SEQ64_MIDIBUS_RM_HPP
#define SEQ64_MIDIBUS_RM_HPP

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
 * \file          midibus_rm.hpp
 *
 *  This module declares/defines the base class for MIDI I/O for Linux, Mac,
 *  and Windows, using a refactored RtMidi library..
 *
 * \library       sequencer64 application
 * \author        Seq24 team; modifications by Chris Ahlstrom
 * \date          2016-11-21
 * \updates       2016-12-27
 * \license       GNU GPLv2 or above
 *
 *  This midibus module is the RtMidi version of the midibus
 *  module.
 */

#include "midibase.hpp"                 /* seq64::midibase class (new)  */

/*
 * Do not document a namespace; it breaks Doxygen.
 */

namespace seq64
{
    class event;
    class rtmidi;
    class rtmidi_info;

/**
 *  This class implements with rtmidi version of the midibus object.
 */

class midibus : public midibase
{
    /**
     *  The master MIDI bus sets up the buss.
     */

    friend class mastermidibus;

private:

    /**
     *  The RtMidi API interface object this midibus will be creating and then
     *  using.
     */

    rtmidi * m_rt_midi;

    /**
     *  For Sequencer64, the ALSA model used requires that all the midibus
     *  objects use the same ASLA sequencer "handle".  The rtmidi_info object
     *  used for enumerating the ports is a good place to get this handle.
     *  It is an extension of the legacy RtMidi interface.
     */

    rtmidi_info & m_master_info;

public:

    /*
     * Virtual-port and non-virtual-port constructors.
    */

    midibus                                 // virtual constructor
    (
        rtmidi_info & rt,
        const std::string & clientname,     // rt.get_bus_name(index)
        int index,
        int bus_id = 0                      // rt.get_bus_id(index)
    );

    midibus                                 // normal constructor
    (
        rtmidi_info & rt,
        int index
    );

    virtual ~midibus ();

protected:


    virtual int api_poll_for_midi ();
    virtual bool api_init_in ();
    virtual bool api_init_in_sub ();
    virtual bool api_init_out ();
    virtual bool api_init_out_sub ();

    /*
     *  Provides common code between api_init_in() and api_initi_out().
     */

    bool api_init_common (rtmidi * rtm);

    virtual void api_continue_from (midipulse tick, midipulse beats);
    virtual void api_start ();
    virtual void api_stop ();
    virtual void api_clock (midipulse tick);
    virtual void api_play (event * e24, midibyte channel);

};          // class midibus (rtmidi version)

}           // namespace seq64

#endif      // SEQ64_MIDIBUS_RM_HPP

/*
 * midibus_rm.hpp
 *
 * vim: sw=4 ts=4 wm=4 et ft=cpp
 */
