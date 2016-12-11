#ifndef SEQ64_MIDI_INFO_HPP
#define SEQ64_MIDI_INFO_HPP

/**
 * \file          midi_info.hpp
 *
 *      A class for holding the current status of the MIDI system on the host.
 *
 * \author        Gary P. Scavone; refactoring by Chris Ahlstrom
 * \date          2016-12-05
 * \updates       2016-12-10
 * \license       See the rtexmidi.lic file.  Too big for a header file.
 *
 *      We need to have a way to get all of the API information from each
 *      framework, without supporting the full API.  The Sequencer64
 *      masteridibus and midibus classes require certain information to be
 *      known when they are created:
 *
 *      -   Port counts.  The number of input ports and output ports needs to
 *          be known so that we can iterate properly over them to create
 *          midibus objects.
 *      -   Port information.  We want to assemble port names just once, and
 *          never have to deal with it again (assuming that MIDI ports do not
 *          come and go during the execution of Sequencer64.
 *      -   Client information.  We want to assemble client names or numbers
 *          just once.
 *
 *      Note that, while the other midi_api-based classes access port via the
 *      port numbers assigned by the MIDI subsystem, midi_info-based classes
 *      use the concept of an "index", which ranges from 0 to one less than
 *      the number of input or output ports.  These values are indices into a
 *      vector of port_info_t structures, and are easily looked up when
 *      mastermidibus creates a midibus object.
 */

#include "midi_api.hpp"

/*
 * Do not document the namespace; it breaks Doxygen.
 */

namespace seq64
{

/**
 *  A class for holding port information.
 */

class midi_port_info
{

private:

    /**
     *  Hold the information for a single port.
     */

    typedef struct
    {
        unsigned m_client_number;   /**< The major buss number of the port. */
        unsigned m_port_number;     /**< The minor port number of the port. */
        std::string m_port_name;    /**< The system's name for the port.    */

    } port_info_t;

    /**
     *  Holds the number of ports counted.
     */

    unsigned m_port_count;

    /**
     *  Holds information on all of the ports that were "scanned".
     */

    std::vector<port_info_t> m_port_container;

public:

    midi_port_info ();

    void add
    (
        unsigned clientnumber,
        unsigned portnumber,
        const std::string & portname
    );

    void clear ()
    {
        m_port_container.clear();
    }

    unsigned get_port_count () const
    {
        return m_port_count;
    }

    unsigned get_client_id (unsigned index) const
    {
        if (index < get_port_count())
            return m_port_container[index].m_client_number;
        else
            return SEQ64_BAD_PORT_ID;
    }

    unsigned get_port_number (unsigned index) const
    {
        if (index < get_port_count())
            return m_port_container[index].m_port_number;
        else
            return SEQ64_BAD_PORT_ID;
    }

    std::string get_port_name (unsigned index) const
    {
        if (index < get_port_count())
            return m_port_container[index].m_port_name;
        else
            return std::string("");
    }

};          // class midi_port_info

/**
 *  The class for holding basic information on the MIDI input and output ports
 *  currently present in the system.
 */

class midi_info : public midi_api
{

private:

    /**
     *  Indicates which mode we're in, input or output.  We have to pick the
     *  mode we need to be in with the set_mode() function before we do a
     *  series of operations.  This clumsy two-step is needed in order to
     *  preserve the midi_api interface.
     */

    bool m_midi_mode_input;

    /**
     *  Holds data on the ALSA/JACK/Core/WinMM inputs.
     */

    midi_port_info m_input;

    /**
     *  Holds data on the ALSA/JACK/Core/WinMM outputs.
     */

    midi_port_info m_output;

public:

    midi_info ();

    virtual ~midi_info ()
    {
        // Empty body
    }

    /**
     * \setter m_midi_mode_input
     */

    virtual void midi_mode (bool flag)
    {
        m_midi_mode_input = flag;
    }

    /**
     * \getter m_input
     */

    midi_port_info & input_ports ()
    {
        return m_input;
    }

    /**
     * \getter m_output
     */

    midi_port_info & output_ports ()
    {
        return m_output;
    }

    /**
     *
     */

    unsigned get_port_count () /*const*/
    {
        midi_port_info & mpi = nc_midi_port_info();
        return mpi.get_port_count();
    }

    unsigned get_client_id (unsigned index) /*const*/
    {
        midi_port_info & mpi = nc_midi_port_info();
        return mpi.get_client_id(index);
    }

    unsigned get_port_number (unsigned index) /*const*/
    {
        midi_port_info & mpi = nc_midi_port_info();
        return mpi.get_port_number(index);
    }

    std::string get_port_name (unsigned index) /*const*/
    {
        midi_port_info & mpi = nc_midi_port_info();
        return mpi.get_port_name(index);
    }

    std::string port_list () const;

    /*
     * We don't want yet another base class, but we have to override these
     * pure virtual functions from midi_api.  We might actually be able to
     * eventually migrate common code into some of these functions.
     */

    virtual rtmidi_api get_current_api () const
    {
        return RTMIDI_API_UNSPECIFIED;
    }

    virtual void open_port
    (
        unsigned /*portnumber*/, const std::string & /*portname*/
    )
    {
        // No action at this time
    }

    virtual void open_virtual_port (const std::string & /*portname*/)
    {
        // No action at this time
    }

    virtual void close_port ()
    {
        // No action at this time
    }

    virtual bool poll_queue () const
    {
        return false;
    }

protected:

    virtual unsigned get_all_port_info () = 0;

private:

    midi_port_info & nc_midi_port_info ()
    {
        return m_midi_mode_input ?
            const_cast<midi_port_info &>(m_input) :
            const_cast<midi_port_info &>(m_output) ;
    }

};          // midi_info

}           // namespace seq64

#endif      // SEQ64_MIDI_INFO_HPP

/*
 * midi_info.hpp
 *
 * vim: sw=4 ts=4 wm=4 et ft=cpp
 */

