#ifndef SEQ64_MIDI_ALSA_HPP
#define SEQ64_MIDI_ALSA_HPP

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
 * \file          midi_alsa.hpp
 *
 *  This module declares/defines the base class for MIDI I/O under Linux.
 *
 * \library       sequencer64 application
 * \author        Seq24 team; modifications by Chris Ahlstrom
 * \date          2016-12-18
 * \updates       2016-12-28
 * \license       GNU GPLv2 or above
 *
 *  The midi_alsa module is the Linux version of the midi_alsa module.
 *  There's almost enough commonality to be worth creating a base class
 *  for both classes.
 *
 *  We moved the mastermidi_alsa class into its own module.
 */

#include "seq64-config.h"
#include "midi_api.hpp"

#if SEQ64_HAVE_LIBASOUND
#include <alsa/asoundlib.h>
#include <alsa/seq_midi_event.h>
#else
#error ALSA not supported in this build, fix the project configuration.
#endif

/*
 *  Do not document a namespace; it breaks Doxygen.
 */

namespace seq64
{
    class event;

/**
 *  This class implements with ALSA version of the midi_alsa object.
 */

class midi_alsa : public midi_api
{
    /**
     *  The master MIDI bus sets up the buss.
     */

    friend class mastermidi_alsa;

private:

    /**
     *  ALSA sequencer client handle.
     */

    snd_seq_t * const m_seq;

    /**
     *  Destination address of client.  Could potentially be replaced by
     *  midibase::m_bus_id.
     */

    const int m_dest_addr_client;

    /**
     *  Destination port of client.  Could potentially be replaced by
     *  midibase::m_port_id.
     */

    const int m_dest_addr_port;

    /**
     *  Local address of client.
     */

    const int m_local_addr_client;

    /**
     *  Local port of client.
     */

    int m_local_addr_port;

public:

    /*
     *  Normal port constructor.
     *  This version is used when querying for existing input ports in the
     *  ALSA system.  It is also used when creating the "announce buss".
     *  Does not yet directly include the concept of buss ID and port ID.
     *
     *  Compare to the output midibus constructor called in seq_alsamidi's
     *  mastermidibus module.  Also note we'll need midi_info::midi_mode()
     *  to set up for midi_alsa_in versus midi_alsa_out.
     */

    midi_alsa
    (
        midi_info & masterinfo,
        int index                           /* a display ordinal    */
    );

    virtual ~midi_alsa ();

    /**
     * \getter m_dest_addr_client
     *      The address of client.  Can we replace it with get_client_id()?
     */

    virtual int get_client () const
    {
        return m_dest_addr_client;
    }

    /**
     * \getter m_dest_addr_port
     *      Can we replace it with get_port_id()?
     */

    virtual int get_port () const
    {
        return m_dest_addr_port;
    }

protected:

    virtual bool api_init_out ();
    virtual bool api_init_in ();
    virtual bool api_init_out_sub ();
    virtual bool api_init_in_sub ();
    virtual bool api_deinit_in ();
    virtual void api_play (event * e24, midibyte channel);
    virtual void api_sysex (event * e24);
    virtual void api_flush ();
    virtual void api_continue_from (midipulse tick, midipulse beats);
    virtual void api_start ();
    virtual void api_stop ();
    virtual void api_clock (midipulse tick);

};          // class midi_alsa

/**
 *  This class implements the ALSA version of a MIDI input object.
 */

class midi_in_alsa : public midi_alsa
{

public:

    midi_in_alsa (midi_info & masterinfo, int index);

};          // class midi_in_alsa

/**
 *  This class implements the ALSA version of a MIDI output object.
 */

class midi_out_alsa : public midi_alsa
{

public:

    midi_out_alsa (midi_info & masterinfo, int index);

};          // class midi_out_alsa

}           // namespace seq64

#endif      // SEQ64_MIDI_ALSA_HPP

/*
 * midi_alsa.hpp
 *
 * vim: sw=4 ts=4 wm=4 et ft=cpp
 */
