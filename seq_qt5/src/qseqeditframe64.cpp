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
 * \file          qseqeditframe64.cpp
 *
 *  This module declares/defines the base class for plastering
 *  pattern/sequence data information in the data area of the pattern
 *  editor.
 *
 * \library       sequencer64 application
 * \author        Seq24 team; modifications by Chris Ahlstrom
 * \date          2018-06-15
 * \updates       2018-07-05
 * \license       GNU GPLv2 or above
 *
 *  The data pane is the drawing-area below the seqedit's event area, and
 *  contains vertical lines whose height matches the value of each data event.
 *  The height of the vertical lines is editable via the mouse.
 *
 *  https://stackoverflow.com/questions/1982986/
 *          scrolling-different-widgets-at-the-same-time
 *
 *      ...Try to make it so that each of the items that needs to scroll in
 *      concert is inside its own QScrollArea. I would then put all those
 *      widgets into one widget, with a QScrollBar underneath (and/or to the
 *      side, if needed).
 *
 *      Designate one of the interior scrolled widgets as the "master", probably
 *      the plot widget. Then do the following:
 *
 *      Set every QScrollArea's horizontal scroll bar policy to never show the
 *      scroll bars.  (Set) the master QScrollArea's horizontalScrollBar()'s
 *      rangeChanged(int min, int max) signal to a slot that sets the main
 *      widget's horizontal QScrollBar to the same range. Additionally, it
 *      should set the same range for the other scroll area widget's horizontal
 *      scroll bars.
 *
 *      The horizontal QScrollBar's valueChanged(int value) signal should be
 *      connected to every scroll area widget's horizontal scroll bar's
 *      setValue(int value) slot.  Repeat for vertical scroll bars, if doing
 *      vertical scrolling.
 *
 *  The layout of this frame is depicted here:
 *
\verbatim
                 -----------------------------------------------------------
    QHBoxLayout | seqname : gridsnap : notelength : seqlength : ...         |
                 -----------------------------------------------------------
    QHBoxLayout | undo : redo : tools : zoomin : zoomout : scale : ...      |
QVBoxLayout:     -----------------------------------------------------------
QWidget container?
    QScrollArea |   | qseqtime      (0, 1, 1, 1) Scroll horiz only      |   |
                |-- |---------------------------------------------------|---|
                | q |                                                   | v |
                | s |                                                   | e |
                | e |                                                   | r |
    QScrollArea | q | qseqroll      (1, 1, 1, 1) Scroll h/v both        | t |
                | k |                                                   | s |
                | e |                                                   | b |
                | y |                                                   | a |
                | s |                                                   | r |
                |---|---------------------------------------------------|---|
    QScrollArea |   | qtriggeredit  (2, 1, 1, 1) Scroll horiz only      |   |
                |   |---------------------------------------------------|   |
                |   |                                                   |   |
    QScrollArea |   | qseqdata      (3, 1, 1, 1) Scroll horiz only      |   |
                |   |                                                   |   |
                 -----------------------------------------------------------
                |   | Horizontal scroll bar for QWidget container       |   |
                 -----------------------------------------------------------
    QHBoxLayout | Events : ...                                              |
                 -----------------------------------------------------------
\endverbatim
 *
 */

#include <QWidget>
#include <QMenu>
#include <QPalette>
#include <QScrollArea>
#include <QScrollBar>

#include "controllers.hpp"              /* seq64::c_controller_names[]      */
#include "perform.hpp"                  /* seq64::perform reference         */
#include "qseqdata.hpp"
#include "qseqeditframe64.hpp"
#include "qseqkeys.hpp"
#include "qseqroll.hpp"
#include "qseqtime.hpp"
#include "qstriggereditor.hpp"
#include "qt5_helpers.hpp"              /* seq64::qt_set_icon()             */
#include "settings.hpp"                 /* usr()                            */

/*
 *  Qt's uic application allows a different output file-name, but not sure
 *  if qmake can change the file-name.
 */

#ifdef SEQ64_QMAKE_RULES
#include "forms/ui_qseqeditframe64.h"
#else
#include "forms/qseqeditframe64.ui.h"
#endif

/*
 *  We prefer to load the pixmaps on the fly, rather than deal with those
 *  friggin' resource files.
 */

#include "pixmaps/bus.xpm"
#include "pixmaps/down.xpm"
#include "pixmaps/follow.xpm"
#include "pixmaps/key.xpm"
#include "pixmaps/menu_empty.xpm"
#include "pixmaps/menu_full.xpm"
#include "pixmaps/midi.xpm"
#include "pixmaps/note_length.xpm"
#include "pixmaps/length_short.xpm"     /* not length.xpm, it is too long   */
#include "pixmaps/quantize.xpm"
#include "pixmaps/redo.xpm"
#include "pixmaps/scale.xpm"
#include "pixmaps/sequences.xpm"
#include "pixmaps/snap.xpm"
#include "pixmaps/tools.xpm"
#include "pixmaps/undo.xpm"
#include "pixmaps/zoom.xpm"             /* zoom_in/_out combo-box           */

#ifdef SEQ64_STAZED_CHORD_GENERATOR
#include "pixmaps/chord3-inv.xpm"
#endif

#ifdef SEQ64_STAZED_TRANSPOSE
#include "pixmaps/drum.xpm"
#include "pixmaps/transpose.xpm"
#endif

/*
 *  Do not document the name space.
 */

namespace seq64
{

/**
 *  Static data members.  These items apply to all of the instances of seqedit,
 *  and are passed on to the following constructors:
 *
 *  -   seqdata TODO
 *  -   seqevent TODO
 *  -   seqroll TODO
 *  -   seqtime TODO
 *
 *  The snap and note-length defaults would be good to write to the "user"
 *  configuration file.  The scale and key would be nice to write to the
 *  proprietary section of the MIDI song.  Or, even more flexibly, to each
 *  sequence, if that makes sense to do, since all tracks would generally be
 *  in the same key.  Right, Charles Ives?
 *
 *  Note that, currently, that some of these "initial values" are modified, so
 *  that they are "contagious".  That is, the next sequence to be opened in
 *  the sequence editor will adopt these values.  This is a long-standing
 *  feature of Seq24, but strikes us as a bit surprising.
 *
 *  If we just double the PPQN, then the snap divisor becomes 32, and the snap
 *  interval is a 32nd note.  We would like to keep it at a 16th note.  We correct
 *  the snap ticks to the actual PPQN ratio.
 */

int qseqeditframe64::m_initial_snap         = SEQ64_DEFAULT_PPQN / 4;
int qseqeditframe64::m_initial_note_length  = SEQ64_DEFAULT_PPQN / 4;

#ifdef SEQ64_STAZED_CHORD_GENERATOR
int qseqeditframe64::m_initial_chord        = 0;
#endif

/**
 * To reduce the amount of written code, we use a static array to
 * initialize the beat-width entries.
 */

static const int s_width_items [] = { 1, 2, 4, 8, 16, 32 };
static const int s_width_count = sizeof(s_width_items) / sizeof(int);

/**
 *  Looks up a beat-width value.
 */

static int
s_lookup_bw (int bw)
{
    int result = 0;
    for (int wi = 0; wi < s_width_count; ++wi)
    {
        if (s_width_items[wi] == bw)
        {
            result = wi;
            break;
        }
    }
    return result;
}

/**
 * To reduce the amount of written code, we use a static array to
 * initialize the measures entries.
 */

static const int s_measures_items [] =
{
    1, 2, 3, 4, 5, 6, 7, 8, 16, 32, 64, 128
};
static const int s_measures_count = sizeof(s_measures_items) / sizeof(int);

/**
 *  Looks up a beat-width value.
 */

static int
s_lookup_measures (int m)
{
    int result = 0;
    for (int wi = 0; wi < s_measures_count; ++wi)
    {
        if (s_measures_items[wi] == m)
        {
            result = wi;
            break;
        }
    }
    return result;
}

/**
 *  These static items are used to fill in and select the proper snap values for
 *  the grids.  Note that they are not members, though they could be.
 */

static const int s_snap_items [] =
{
    1, 2, 4, 8, 16, 32, 64, 128, 0, 3, 6, 12, 24, 48, 96, 192
};
static const int s_snap_count = sizeof(s_snap_items) / sizeof(int);

/**
 *  These static items are used to fill in and select the proper zoom values for
 *  the grids.  Note that they are not members, though they could be.
 *  Also note the features of these zoom numbers:
 *
 *      -#  The lowest zoom value is SEQ64_MINIMUM_ZOOM in app_limits.h.
 *      -#  The highest zoom value is SEQ64_MAXIMUM_ZOOM in app_limits.h.
 *      -#  The zoom values are all powers of 2.
 *      -#  The zoom values are further constrained by the configured values
 *          of usr().min_zoom() and usr().max_zoom().
 *      -#  The default zoom is specified in the user's "usr" file, and
 *          the default value of this default zoom is 2.
 *
 * \todo
 *      We still need to figure out what to do with a zoom of 0, which
 *      is supposed to tell Sequencer64 to auto-adjust to the current PPQN.
 */

static const int s_zoom_items [] =
{
    1, 2, 4, 8, 16, 32, 64, 128
};
static const int s_zoom_count = sizeof(s_zoom_items) / sizeof(int);

/**
 *  Looks up a zoom value and returns its index.
 */

static int
s_lookup_zoom (int zoom)
{
    int result = 0;
    for (int zi = 0; zi < s_zoom_count; ++zi)
    {
        if (s_zoom_items[zi] == zoom)
        {
            result = zi;
            break;
        }
    }
    return result;
}

#ifdef SEQ64_STAZED_CHORD_GENERATOR_NOT_NEEDED

/**
 *  Looks up a chord name and returns its index.  Note that the chord names
 *  are defined in the scales.h file.
 */

static int
s_lookup_chord (const std::string & chordname)
{
    int result = 0;
    for (int chord = 0; chord < c_chord_number; ++chord)
    {
        if (c_chord_table_text[chord] == chordname)
        {
            result = chord;
            break;
        }
    }
    return result;
}

#endif

/**
 *
 * \param p
 *      Provides the perform object to use for interacting with this sequence.
 *      Among other things, this object provides the active PPQN.
 *
 * \param seqid
 *      Provides the sequence number.  The sequence pointer is looked up using
 *      this number.  This number is also the pattern-slot number for this
 *      sequence and for this window.  Ranges from 0 to 1024.
 *
 * \param parent
 *      Provides the parent window/widget for this container window.  Defaults
 *      to null.
 */

qseqeditframe64::qseqeditframe64
(
    perform & p,
    int seqid,
    QWidget * parent
) :
    QFrame              (parent),
    ui                  (new Ui::qseqeditframe64),
    m_performance       (p),                            // a reference
    m_seq               (perf().get_sequence(seqid)),   // a pointer
    m_seqkeys           (nullptr),
    m_seqtime           (nullptr),
    m_seqroll           (nullptr),
    m_seqdata           (nullptr),
    m_seqevent          (nullptr),
    m_tools_popup       (nullptr),
    m_sequences_popup   (nullptr),
    m_events_popup      (nullptr),
    m_minidata_popup    (nullptr),
    m_beats_per_bar     (not_nullptr(m_seq) ? m_seq->get_beats_per_bar() : 4),
    m_beat_width        (not_nullptr(m_seq) ? m_seq->get_beat_width() : 4),
    m_initial_zoom      (SEQ64_DEFAULT_ZOOM),           // constant
    m_zoom              (SEQ64_DEFAULT_ZOOM),           // fixed below
    m_snap              (m_initial_snap),
    m_note_length       (m_initial_note_length),
    m_scale             (usr().seqedit_scale()),        // m_initial_scale
#ifdef SEQ64_STAZED_CHORD_GENERATOR
    m_chord             (0),    // (usr().seqedit_chord()),  // m_initial_chord
#endif
    m_key               (usr().seqedit_key()),          // m_initial_key
    m_bgsequence        (usr().seqedit_bgsequence()),   // m_initial_sequence
    m_measures          (0),                            // fixed below
    m_ppqn              (p.ppqn()),
#ifdef USE_STAZED_ODD_EVEN_SELECTION
    m_pp_whole          (0),
    m_pp_eighth         (0),
    m_pp_sixteenth      (0),
#endif
    m_editing_status    (0),
    m_editing_cc        (0),
    m_first_event       (0),
    m_first_event_name  ("(no events)"),
    m_have_focus        (false),
    m_edit_mode         (perf().seq_edit_mode(seqid))
{
    ui->setupUi(this);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    /*
     * Instantiate the various editable areas of the seqedit user-interface.
     * seqkeys: Not quite working as we'd hope.  The scrollbars still eat up
     * space.  They needed to be hidden.
     */

    m_seqkeys = new qseqkeys
    (
        *m_seq,
        ui->keysScrollArea,
        usr().key_height(),
        usr().key_height() * c_num_keys + 1
    );
    ui->keysScrollArea->setWidget(m_seqkeys);
    ui->keysScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    ui->keysScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    /*
     * qseqtime
     */

    m_seqtime = new qseqtime
    (
        perf(), *m_seq, SEQ64_DEFAULT_ZOOM, ui->timeScrollArea
    );
    ui->timeScrollArea->setWidget(m_seqtime);
    ui->timeScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    ui->timeScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    /*
     * qseqroll.  Note the last parameter, "this" is not really a parent
     * parameter.  It simply gives qseqroll access to the qseqeditframe64 ::
     * follow_progress() function.
     */

    m_seqroll = new qseqroll
    (
        perf(), *m_seq, m_seqkeys, m_zoom, m_snap, 0,
        EDIT_MODE_NOTE, this                            /* see note above   */
    );
    ui->rollScrollArea->setWidget(m_seqroll);
    ui->rollScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    ui->rollScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    m_seqroll->update_edit_mode(m_edit_mode);

    /*
     * qseqdata
     */

    m_seqdata = new qseqdata
    (
        perf(), *m_seq, m_zoom, m_snap, ui->dataScrollArea
    );
    ui->dataScrollArea->setWidget(m_seqdata);
    ui->dataScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    ui->dataScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    /*
     * qseqevent
     */

    m_seqevent = new qstriggereditor
    (
        perf(), *m_seq, m_seqdata, m_zoom, m_snap,
        usr().key_height(), ui->eventScrollArea
    );
    ui->eventScrollArea->setWidget(m_seqevent);
    ui->eventScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    ui->eventScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    /*
     *  Add the various scrollbar points to the qscrollmaster object,
     *  ui->rollScrollArea.
     */

    ui->rollScrollArea->add_v_scroll(ui->keysScrollArea->verticalScrollBar());
    ui->rollScrollArea->add_h_scroll(ui->timeScrollArea->horizontalScrollBar());
    ui->rollScrollArea->add_h_scroll(ui->dataScrollArea->horizontalScrollBar());
    ui->rollScrollArea->add_h_scroll(ui->eventScrollArea->horizontalScrollBar());

    /*
     *  Sequence Number Label
     */

    char tmp[32];
    snprintf(tmp, sizeof tmp, "%d", seqid);

    QString labeltext = tmp;
    ui->m_label_seqnumber->setText(labeltext);

    /*
     * Sequence Title
     */

    ui->m_entry_name->setText(m_seq->name().c_str());
    connect
    (
        ui->m_entry_name, SIGNAL(textChanged(const QString &)),
        this, SLOT(update_seq_name())
    );

    /*
     * Beats Per Bar.  Fill the options for the beats per measure combo-box,
     * and set the default.
     */

    qt_set_icon(down_xpm, ui->m_button_bpm);
#ifdef SEQ64_QSEQEDIT_BUTTON_INCREMENT
    ui->m_button_bpm->setToolTip("Beats per bar. Increments to next value.");
    connect
    (
        ui->m_button_bpm, SIGNAL(clicked(bool)),
        this, SLOT(increment_beats_per_measure())
    );
#else
    ui->m_button_bpm->setToolTip("Beats per bar. Resets to default value.");
    connect
    (
        ui->m_button_bpm, SIGNAL(clicked(bool)),
        this, SLOT(reset_beats_per_measure())
    );
#endif
    for
    (
        int b = SEQ64_MINIMUM_BEATS_PER_MEASURE - 1;
        b <= SEQ64_MAXIMUM_BEATS_PER_MEASURE - 1;
        ++b
    )
    {
        QString combo_text = QString::number(b + 1);
        ui->m_combo_bpm->insertItem(b, combo_text);
    }
    ui->m_combo_bpm->setCurrentIndex(m_beats_per_bar - 1);
    connect
    (
        ui->m_combo_bpm, SIGNAL(currentIndexChanged(int)),
        this, SLOT(update_beats_per_measure(int))
    );
    set_beats_per_measure(m_seq->get_beats_per_bar());

    /*
     * Beat Width (denominator of time signature).  Fill the options for
     * the beats per measure combo-box, and set the default.
     */

    qt_set_icon(down_xpm, ui->m_button_bw);
#ifdef SEQ64_QSEQEDIT_BUTTON_INCREMENT
    ui->m_button_bw->setToolTip
    (
        "Beats width (denominator). Increments to next value."
    );
    connect
    (
        ui->m_button_bw, SIGNAL(clicked(bool)),
        this, SLOT(next_beat_width())
    );
#else
    ui->m_button_bw->setToolTip
    (
        "Beats width (denominator). Resets to default value."
    );
    connect
    (
        ui->m_button_bw, SIGNAL(clicked(bool)),
        this, SLOT(reset_beat_width())
    );
#endif
    for (int w = 0; w < s_width_count; ++w)
    {
        int item = s_width_items[w];
        char fmt[8];
        snprintf(fmt, sizeof fmt, "%d", item);
        QString combo_text = fmt;
        ui->m_combo_bw->insertItem(w, combo_text);
    }

    int bw_index = s_lookup_bw(m_beat_width);
    ui->m_combo_bw->setCurrentIndex(bw_index);
    connect
    (
        ui->m_combo_bw, SIGNAL(currentIndexChanged(int)),
        this, SLOT(update_beat_width(int))
    );
    set_beat_width(m_seq->get_beat_width());

    /*
     * Pattern Length in Measures. Fill the options for
     * the beats per measure combo-box, and set the default.
     */

    qt_set_icon(length_short_xpm, ui->m_button_length);
#ifdef SEQ64_QSEQEDIT_BUTTON_INCREMENT
    ui->m_button_length->setToolTip
    (
        "Pattern length (bars). Increments to next value."
    );
    connect
    (
        ui->m_button_length, SIGNAL(clicked(bool)),
        this, SLOT(next_measures())
    );
#else
    ui->m_button_length->setToolTip
    (
        "Pattern length (bars). Resets to default value."
    );
    connect
    (
        ui->m_button_length, SIGNAL(clicked(bool)),
        this, SLOT(reset_measures())
    );
#endif
    for (int m = 0; m < s_measures_count; ++m)
    {
        int item = s_measures_items[m];
        char fmt[8];
        snprintf(fmt, sizeof fmt, "%d", item);
        QString combo_text = fmt;
        ui->m_combo_length->insertItem(m, combo_text);
    }

    int len_index = s_lookup_measures(m_measures);
    ui->m_combo_length->setCurrentIndex(len_index);
    connect
    (
        ui->m_combo_length, SIGNAL(currentIndexChanged(int)),
        this, SLOT(update_measures(int))
    );
    m_seq->set_unit_measure();              /* must precede set_measures()  */
    set_measures(get_measures());

#ifdef SEQ64_STAZED_TRANSPOSE

    /*
     *  Transpose button.
     */

    bool cantranspose = m_seq->get_transposable();
    qt_set_icon(transpose_xpm, ui->m_toggle_transpose);
    ui->m_toggle_transpose->setToolTip
    (
        "Sequence is allowed to be transposed if button is highighted/checked."
    );
    ui->m_toggle_transpose->setCheckable(true);
    ui->m_toggle_transpose->setChecked(cantranspose);
    if (! usr().work_around_transpose_image())
        set_transpose_image(cantranspose);

    connect
    (
        ui->m_toggle_transpose, SIGNAL(toggled(bool)),
        this, SLOT(transpose(bool))
    );

#endif

#ifdef SEQ64_STAZED_CHORD_GENERATOR

    /*
     * Chord button and combox-box.  See c_chord_table_text[c_chord_number][]
     * in the scales.h header file.
     */

#ifdef SEQ64_QSEQEDIT_BUTTON_INCREMENT
    qt_set_icon(chord3_inv_xpm, ui->m_button_chord);
    ui->m_button_chord->setToolTip
    (
        "Chord generation. Increments to next value."
    );
    connect
    (
        ui->m_button_chord, SIGNAL(clicked(bool)),
        this, SLOT(increment_chord())
    );
#else
    qt_set_icon(chord3_inv_xpm, ui->m_button_chord);
    ui->m_button_chord->setToolTip
    (
        "Chord generation. Resets chord generation to Off."
    );
    connect
    (
        ui->m_button_chord, SIGNAL(clicked(bool)),
        this, SLOT(reset_chord())
    );
#endif

    for (int chord = 0; chord < c_chord_number; ++chord)
    {
        QString combo_text = c_chord_table_text[chord];
        ui->m_combo_chord->insertItem(chord, combo_text);
    }
    ui->m_combo_chord->setCurrentIndex(m_chord);
    connect
    (
        ui->m_combo_chord, SIGNAL(currentIndexChanged(int)),
        this, SLOT(update_chord(int))
    );
    set_chord(m_chord);

#endif  // SEQ64_STAZED_CHORD_GENERATOR

    /*
     *  MIDI buss items discovered at startup-time.  Not sure if we want to
     *  use the button to reset the buss, or increment to the next buss.
     */

    qt_set_icon(bus_xpm, ui->m_button_bus);
    ui->m_button_bus->setToolTip("Resets output MIDI buss number to 0.");
    connect
    (
        ui->m_button_bus, SIGNAL(clicked(bool)),
        this, SLOT(reset_midi_bus())
    );

    mastermidibus & masterbus = perf().master_bus();
    for (int b = 0; b < masterbus.get_num_out_buses(); ++b)
    {
        ui->m_combo_bus->addItem
        (
            QString::fromStdString(masterbus.get_midi_out_bus_name(b))
        );
    }
    ui->m_combo_bus->setCurrentText
    (
        QString::fromStdString
        (
            masterbus.get_midi_out_bus_name(m_seq->get_midi_bus())
        )
    );
    connect
    (
        ui->m_combo_bus, SIGNAL(currentIndexChanged(int)),
        this, SLOT(update_midi_bus(int))
    );
    set_midi_bus(m_seq->get_midi_bus());

    // Move this call to the Event button zone when that is ready.
    set_data_type(EVENT_NOTE_ON);

    /*
     *  MIDI channels.  Not sure if we want to
     *  use the button to reset the channel, or increment to the next channel.
     */

    qt_set_icon(midi_xpm, ui->m_button_channel);
    ui->m_button_channel->setToolTip("Resets output MIDI channel number to 1.");
    connect
    (
        ui->m_button_channel, SIGNAL(clicked(bool)),
        this, SLOT(reset_midi_channel())
    );

    for (int channel = 0; channel < SEQ64_MIDI_CHANNEL_MAX; ++channel)
    {
        QString combo_text = QString::number(channel + 1);
        ui->m_combo_channel->insertItem(channel, combo_text);
    }
    ui->m_combo_channel->setCurrentIndex(m_seq->get_midi_channel());
    connect
    (
        ui->m_combo_channel, SIGNAL(currentIndexChanged(int)),
        this, SLOT(update_midi_channel(int))
    );
    set_midi_channel(m_seq->get_midi_channel());

    /*
     * Undo and Redo Buttons.
     */

    qt_set_icon(undo_xpm, ui->m_button_undo);
    ui->m_button_undo->setToolTip("Undo.");
    connect(ui->m_button_undo, SIGNAL(clicked(bool)), this, SLOT(undo()));

    qt_set_icon(redo_xpm, ui->m_button_redo);
    ui->m_button_redo->setToolTip("Redo.");
    connect(ui->m_button_redo, SIGNAL(clicked(bool)), this, SLOT(redo()));

    /*
     * Quantize Button.  This is the "Q" button, and indicates to
     * quantize (just?) notes.  Compare it to the Quantize menu entry,
     * which quantizes events.  Note the usage of std::bind()... this feature
     * requires C++11.
     */

    qt_set_icon(quantize_xpm, ui->m_button_quantize);
    ui->m_button_quantize->setToolTip("Quantize.");
    connect
    (
        ui->m_button_quantize, &QPushButton::clicked,
        std::bind(&qseqeditframe64::do_action, this, c_quantize_notes, 0)
    );

    /*
     * Tools Pop-up Menu Button.
     */

    qt_set_icon(tools_xpm, ui->m_button_tools);
    ui->m_button_tools->setToolTip("Tools popup menu.");
    connect(ui->m_button_tools, SIGNAL(clicked(bool)), this, SLOT(tools()));
    popup_tool_menu();

    /*
     * Follow Progress Button.
     */

    qt_set_icon(follow_xpm, ui->m_toggle_follow);

#ifdef SEQ64_FOLLOW_PROGRESS_BAR

    ui->m_toggle_follow->setEnabled(true);
    ui->m_toggle_follow->setCheckable(true);
    ui->m_toggle_follow->setToolTip
    (
        "If active, the piano roll scrolls to "
        "follow the progress bar in playback."
    );

    /*
     * Qt::NoFocus is the default focus policy.
     */

    ui->m_toggle_follow->setAutoDefault(false);
    ui->m_toggle_follow->setChecked(m_seqroll->progress_follow());
    connect(ui->m_toggle_follow, SIGNAL(toggled(bool)), this, SLOT(follow(bool)));
#else

    ui->m_toggle_follow->setEnabled(false);

#endif

    /**
     *  Fill "Snap" and "Note" Combo Boxes:
     *
     *      To reduce the amount of written code, we now use a static array to
     *      initialize some of these menu entries.  0 denotes the separator.
     *      This same setup is used to set up both the snap and note menu, since
     *      they are exactly the same.  Saves a *lot* of code.  This code was
     *      copped from the Gtkmm 2.4 seqedit class and adapted to Qt 5.
     */

    for (int si = 0; si < s_snap_count; ++si)
    {
        int item = s_snap_items[si];
        char fmt[8];
        if (item > 1)
            snprintf(fmt, sizeof fmt, "1/%d", item);
        else
            snprintf(fmt, sizeof fmt, "%d", item);

        QString combo_text = fmt;
        if (item == 0)
        {
            ui->m_combo_snap->insertSeparator(8);   // why 8?
            ui->m_combo_note->insertSeparator(8);   // why 8?
            continue;
        }
        else
        {
            ui->m_combo_snap->insertItem(si, combo_text);
            ui->m_combo_note->insertItem(si, combo_text);
        }
    }
    ui->m_combo_snap->setCurrentIndex(4);               /* 16th-note entry  */
    connect
    (
        ui->m_combo_snap, SIGNAL(currentIndexChanged(int)),
        this, SLOT(update_grid_snap(int))
    );
    ui->m_combo_note->setCurrentIndex(4);               /* ditto            */
    connect
    (
        ui->m_combo_note, SIGNAL(currentIndexChanged(int)),
        this, SLOT(update_note_length(int))
    );

    qt_set_icon(snap_xpm, ui->m_button_snap);
#ifdef SEQ64_QSEQEDIT_BUTTON_INCREMENT
    // No increment code at this time.
#endif
    ui->m_button_snap->setToolTip("Snap size. Resets to default snap size.");
    connect
    (
        ui->m_button_snap, SIGNAL(clicked(bool)),
        this, SLOT(reset_grid_snap())
    );
    set_snap(m_initial_snap * m_ppqn / SEQ64_DEFAULT_PPQN);

    qt_set_icon(note_length_xpm, ui->m_button_note);
#ifdef SEQ64_QSEQEDIT_BUTTON_INCREMENT
    // No increment code at this time.
#endif
    ui->m_button_note->setToolTip("Note length. Resets to default note length.");
    connect
    (
        ui->m_button_note, SIGNAL(clicked(bool)),
        this, SLOT(reset_note_length())
    );
    set_note_length(m_initial_note_length * m_ppqn / SEQ64_DEFAULT_PPQN);

    /*
     *  Zoom In and Zoom Out:  Rather than two buttons, we use one and
     *  a combo-box.
     */

#ifdef SEQ64_QSEQEDIT_BUTTON_INCREMENT
    qt_set_icon(zoom_xpm, ui->m_button_zoom);
    ui->m_button_zoom->setToolTip("Next zoom level. Wraps around.");
    connect
    (
        ui->m_button_zoom, SIGNAL(clicked(bool)),
        this, SLOT(zoom_out())
    );
#else
    qt_set_icon(zoom_xpm, ui->m_button_zoom);
    ui->m_button_zoom->setToolTip("Zoom level. Resets to default zoom.");
    connect
    (
        ui->m_button_zoom, SIGNAL(clicked(bool)),
        this, SLOT(reset_zoom())
    );
#endif

    for (int zi = 0; zi < s_zoom_count; ++zi)
    {
        int zoom = s_zoom_items[zi];
        if (zoom >= usr().min_zoom() && zoom <= usr().max_zoom())
        {
            char fmt[16];
            snprintf(fmt, sizeof fmt, "1px:%dtx", zoom);

            QString combo_text = fmt;
            ui->m_combo_zoom->insertItem(zi, combo_text);
        }
    }
    ui->m_combo_zoom->setCurrentIndex(1);
    connect
    (
        ui->m_combo_zoom, SIGNAL(currentIndexChanged(int)),
        this, SLOT(update_zoom(int))
    );

    int zoom = usr().zoom();
    if (usr().zoom() == SEQ64_USE_ZOOM_POWER_OF_2)      /* i.e. 0 */
        zoom = zoom_power_of_2(m_ppqn);

    set_zoom(zoom);

    /*
     * Musical Keys Button and Combo-Box. See c_key_text[SEQ64_OCTAVE_SIZE][]
     * in the scales.h header file.
     */

#ifdef SEQ64_QSEQEDIT_BUTTON_INCREMENT
    // No key-incrementing functionality at this time
#endif
    qt_set_icon(key_xpm, ui->m_button_key);
    ui->m_button_key->setToolTip
    (
        "Musical key selection. Resets key selection to 'C'."
    );
    connect
    (
        ui->m_button_key, SIGNAL(clicked(bool)),
        this, SLOT(reset_key())
    );

    for (int key = 0; key < SEQ64_OCTAVE_SIZE; ++key)
    {
        QString combo_text = c_key_text[key];
        ui->m_combo_key->insertItem(key, combo_text);
    }
    ui->m_combo_key->setCurrentIndex(m_key);
    connect
    (
        ui->m_combo_key, SIGNAL(currentIndexChanged(int)),
        this, SLOT(update_key(int))
    );
    if (m_seq->musical_key() != SEQ64_KEY_OF_C)
        set_key(m_seq->musical_key());
    else
        set_key(m_key);

    /*
     * Musical Scales Button and Combo-Box. See c_scales_text[c_scale_size][]
     * in the scales.h header file.
     */

#ifdef SEQ64_QSEQEDIT_BUTTON_INCREMENT
    // No scale-incrementing functionality at this time
#endif
    qt_set_icon(scale_xpm, ui->m_button_scale);
    ui->m_button_scale->setToolTip
    (
        "Musical scale selection. Resets scale selection to 'C'."
    );
    connect
    (
        ui->m_button_scale, SIGNAL(clicked(bool)),
        this, SLOT(reset_scale())
    );

    for (int scale = 0; scale < c_scale_size; ++scale)
    {
        QString combo_text = c_scales_text[scale];
        ui->m_combo_scale->insertItem(scale, combo_text);
    }
    ui->m_combo_scale->setCurrentIndex(m_scale);
    connect
    (
        ui->m_combo_scale, SIGNAL(currentIndexChanged(int)),
        this, SLOT(update_scale(int))
    );
    if (m_seq->musical_scale() != int(c_scale_off))
        set_scale(m_seq->musical_scale());
    else
        set_scale(m_scale);

    /*
     * Background Sequence/Pattern Selectors.
     */

    qt_set_icon(sequences_xpm, ui->m_button_sequence);
    ui->m_button_sequence->setToolTip("Background sequence popup menu.");
    connect
    (
        ui->m_button_sequence, SIGNAL(clicked(bool)),
        this, SLOT(sequences())
    );
    popup_sequence_menu();              /* create the initial popup menu    */

    if (SEQ64_IS_VALID_SEQUENCE(m_seq->background_sequence()))
        m_bgsequence = m_seq->background_sequence();

    set_background_sequence(m_bgsequence);

    /*
     * Event Selection Button and Popup Menu for qseqdata.
     */

    ui->m_button_event->setToolTip("Event to show in data panel, popup menu.");
    connect
    (
        ui->m_button_event, SIGNAL(clicked(bool)),
        this, SLOT(events())
    );
    popup_event_menu();                 /* create the initial popup menu    */

    /*
     * Event Data Presence-Indicator Button and Popup Menu.
     */

    /*
     * LFO Button.
     */

    /*
     * Enable (unmute) Play Button.
     */

    /*
     * MIDI Thru Button.
     */

    /*
     * MIDI Record Button.
     */

    /*
     * MIDI Quantized Record Button.
     */

    /*
     * Recording Merge, Replace, Extend Button.
     */

    /*
     * Recording Volume Button (and Combo?)
     */

}

/**
 *  \dtor
 */

qseqeditframe64::~qseqeditframe64 ()
{
    delete ui;
}

/*
 * Play the SLOTS!
 */

/**
 *  Handles edits of the sequence title.
 */

void
qseqeditframe64::update_seq_name ()
{
    m_seq->set_name(ui->m_entry_name->text().toStdString());
}

/**
 *  Handles updates to the beats/measure for only the current sequences.
 *  See the similar function in qsmainwnd.
 */

void
qseqeditframe64::update_beats_per_measure (int index)
{
    ++index;
    if
    (
        index != m_beats_per_bar &&
        index >= SEQ64_MINIMUM_BEATS_PER_MEASURE &&
        index <= SEQ64_MAXIMUM_BEATS_PER_MEASURE
    )
    {
        set_beats_per_measure(index);
        set_dirty();
    }
}

/**
 *  When the BPM (beats-per-measure) button is pushed, we go to the next BPM
 *  entry in the combo-box, wrapping around when the end is reached.
 */

void
qseqeditframe64::increment_beats_per_measure ()
{
    int bpm = m_beats_per_bar + 1;
    if (bpm > SEQ64_MAXIMUM_BEATS_PER_MEASURE)
        bpm = SEQ64_MINIMUM_BEATS_PER_MEASURE;

    ui->m_combo_bpm->setCurrentIndex(bpm - 1);
    set_beats_per_measure(bpm);
}

/**
 *
 */

void
qseqeditframe64::reset_beats_per_measure ()
{
    ui->m_combo_bpm->setCurrentIndex(SEQ64_DEFAULT_BEATS_PER_MEASURE - 1);
    // set_dirty();
}

/**
 *
 */

void
qseqeditframe64::set_beats_per_measure (int bpm)
{
    int measures = get_measures();
    m_seq->set_beats_per_bar(bpm);
    m_beats_per_bar = bpm;
    m_seq->apply_length(bpm, m_ppqn, m_seq->get_beat_width(), measures);
    set_dirty();
}

/**
 *  Set the measures value, using the given parameter, and some internal
 *  values passed to apply_length().
 *
 * \param len
 *      Provides the sequence length, in measures.
 */

void
qseqeditframe64::set_measures (int len)
{
    m_measures = len;
    m_seq->apply_length
    (
        m_seq->get_beats_per_bar(), m_ppqn, m_seq->get_beat_width(), len
    );
    set_dirty();
}

/**
 *
 */

void
qseqeditframe64::reset_measures ()
{
    ui->m_combo_length->setCurrentIndex(0);
    // set_dirty();
}

/**
 *
 */

int
qseqeditframe64::get_measures ()
{
    int units =
    (
        m_seq->get_beats_per_bar() * m_ppqn * 4 / m_seq->get_beat_width()
    );
    int measures = m_seq->get_length() / units;
    if (m_seq->get_length() % units != 0)
        ++measures;

    return measures;
}

/**
 *  Handles updates to the beat width for only the current sequences.
 *  See the similar function in qsmainwnd.
 */

void
qseqeditframe64::update_beat_width (int index)
{
    int bw = s_width_items[index];
    if (bw != m_beat_width)
    {
        set_beat_width(bw);
        set_dirty();
    }
}

/**
 *  When the BW (beat width) button is pushed, we go to the next beat width
 *  entry in the combo-box, wrapping around when the end is reached.
 */

void
qseqeditframe64::next_beat_width ()
{
    int index = s_lookup_bw(m_beat_width);
    if (++index >= s_width_count)
        index = 0;

    ui->m_combo_bw->setCurrentIndex(index);
    int bw = s_width_items[index];
    if (bw != m_beat_width)
        set_beat_width(bw);
}

/**
 *
 */

void
qseqeditframe64::reset_beat_width ()
{
    ui->m_combo_bw->setCurrentIndex(2);     /* i.e. 4, see s_width_items    */
    update_draw_geometry();
}

/**
 *  Sets the beat-width value and then dirties the user-interface so that it
 *  will be repainted.
 */

void
qseqeditframe64::set_beat_width (int bw)
{
    int measures = get_measures();
    m_seq->set_beat_width(bw);
    m_seq->apply_length(m_seq->get_beats_per_bar(), m_ppqn, bw, measures);
    m_beat_width = bw;
    set_dirty();
}

/**
 *  Handles updates to the pattern length.
 */

void
qseqeditframe64::update_measures (int index)
{
    int m = s_measures_items[index];
    if (m != m_measures)
    {
        set_measures(m);
        set_dirty();
    }
}

/**
 *  When the measures-length button is pushed, we go to the next length
 *  entry in the combo-box, wrapping around when the end is reached.
 */

void
qseqeditframe64::next_measures ()
{
    int index = s_lookup_measures(m_measures);
    if (++index >= s_measures_count)
        index = 0;

    ui->m_combo_length->setCurrentIndex(index);
    int m = s_measures_items[index];
    if (m != m_measures)
        set_measures(m);
}

#ifdef SEQ64_STAZED_TRANSPOSE

/**
 *  Passes the transpose status to the sequence object.
 */

void
qseqeditframe64::transpose (bool ischecked)
{
    m_seq->set_transposable(ischecked);
    if (! usr().work_around_transpose_image())
        set_transpose_image(ischecked);
}

/**
 *  Changes the image used for the transpose button.
 *
 * \param istransposable
 *      If true, set the image to the "Transpose" icon.  Otherwise, set it to
 *      the "Drum" (not transposable) icon.
 */

void
qseqeditframe64::set_transpose_image (bool istransposable)
{
    if (istransposable)
    {
        ui->m_toggle_transpose->setToolTip("Sequence is transposable.");
        qt_set_icon(transpose_xpm, ui->m_toggle_transpose);
    }
    else
    {
        ui->m_toggle_transpose->setToolTip("Sequence is not transposable.");
        qt_set_icon(drum_xpm, ui->m_toggle_transpose);
    }
}

#endif

#ifdef SEQ64_STAZED_CHORD_GENERATOR

/**
 *  Handles updates to the beats/measure for only the current sequences.
 *  See the similar function in qsmainwnd.
 */

void
qseqeditframe64::update_chord (int index)
{
    if (index != m_chord && index >= 0 && index < c_chord_number)
    {
        set_chord(index);
        set_dirty();
    }
}

/**
 *  When the chord button is pushed, we can go to the next chord
 *  entry in the combo-box, wrapping around when the end is reached.
 *  Currently, though, we just reset to the default chord.
 */

void
qseqeditframe64::increment_chord ()
{
    int chord = m_chord + 1;
    if (chord >= c_chord_number)
        chord = 0;

    set_chord(chord);
}

/**
 *
 */

void
qseqeditframe64::set_chord (int chord)
{
    if (chord >= 0 && chord < c_chord_number)
    {
        ui->m_combo_chord->setCurrentIndex(chord);
        m_chord = m_initial_chord = chord;
        m_seqroll->set_chord(chord);
    }
}

/**
 *
 */

void
qseqeditframe64::reset_chord ()
{
    ui->m_combo_chord->setCurrentIndex(0);
    m_seqroll->set_chord(0);
}

#endif  // SEQ64_STAZED_CHORD_GENERATOR

/**
 *
 */

void
qseqeditframe64::update_midi_bus (int index)
{
    mastermidibus & masterbus = perf().master_bus();
    if (index >= 0 && index < masterbus.get_num_out_buses())
    {
        m_seq->set_midi_bus(index);
        set_dirty();
    }
}

/**
 *
 */

void
qseqeditframe64::reset_midi_bus ()
{
    ui->m_combo_bus->setCurrentIndex(0);        // update_midi_bus(0)
    update_draw_geometry();
}

/**
 *  Selects the given MIDI buss parameter in the main sequence object,
 *  so that it will use that buss.
 *
 *  Should this change set the is-modified flag?  Where should validation
 *  against the ALSA or JACK buss limits occur?
 *
 *  Also, it would be nice to be able to update this display of the MIDI bus
 *  in the field if we set it from the seqmenu.
 *
 * \param bus
 *      The buss value to set.  If this value changes the selected buss, then
 *      the MIDI channel popup menu is repopulated.
 *
 * \param user_change
 *      True if the user made this change, and thus has potentially modified
 *      the song.
 */

void
qseqeditframe64::set_midi_bus (int bus, bool user_change)
{
    int initialbus = m_seq->get_midi_bus();
    m_seq->set_midi_bus(bus, user_change);          /* user-modified value? */
    ui->m_combo_bus->setCurrentIndex(0);            /* update_midi_bus(0)   */
    if (bus != initialbus)
    {
        // int channel = m_seq->get_midi_channel();
        // TODO
        // repopulate_midich_menu(bus);
        // repopulate_event_menu(bus, channel);
    }
}

/**
 *
 */

void
qseqeditframe64::update_midi_channel (int index)
{
    if (index >= 0 && index < SEQ64_MIDI_CHANNEL_MAX)
    {
        m_seq->set_midi_channel(index);
        set_dirty();
    }
}

/**
 *
 */

void
qseqeditframe64::reset_midi_channel ()
{
    ui->m_combo_channel->setCurrentIndex(0);    // update_midi_channel(0)
    update_draw_geometry();
}

/**
 *  Selects the given MIDI channel parameter in the main sequence object,
 *  so that it will use that channel.
 *
 *  Should this change set the is-modified flag?  Where should validation
 *  occur?
 *
 * \param midichannel
 *      The MIDI channel  value to set.
 *
 * \param user_change
 *      True if the user made this change, and thus has potentially modified
 *      the song.
 */

void
qseqeditframe64::set_midi_channel (int midichannel, bool user_change)
{
    ui->m_combo_channel->setCurrentIndex(midichannel);
    m_seq->set_midi_channel(midichannel, user_change); /* user-modified value? */
}

/**
 *
 */

void
qseqeditframe64::undo ()
{
    m_seq->pop_undo();
    set_dirty();
}

/**
 *
 */

void
qseqeditframe64::redo ()
{
    m_seq->pop_redo();
    set_dirty();
}

/**
 *  Popup menu over button.
 */

void
qseqeditframe64::tools ()
{
    if (not_nullptr(m_tools_popup))
    {
        m_tools_popup->exec
        (
            ui->m_button_tools->mapToGlobal
            (
                QPoint(ui->m_button_tools->width()-2, ui->m_button_tools->height()-2)
            )
        );
    }
}

/**
 *  Builds the Tools popup menu on the fly.
 */

void
qseqeditframe64::popup_tool_menu ()
{
    m_tools_popup = new QMenu(this);
    QMenu * menuselect = new QMenu(tr("&Select..."), m_tools_popup);
    QMenu * menutiming = new QMenu(tr("&Timing..."), m_tools_popup);
    QMenu * menupitch  = new QMenu(tr("&Pitch..."), m_tools_popup);
    QAction * selectall = new QAction(tr("Select all"), m_tools_popup);
    selectall->setShortcut(tr("Ctrl+A"));
    connect
    (
        selectall, SIGNAL(triggered(bool)),
        this, SLOT(select_all_notes())
    );
    menuselect->addAction(selectall);

    QAction * selectinverse = new QAction(tr("Inverse selection"), m_tools_popup);
    selectinverse->setShortcut(tr("Ctrl+Shift+I"));
    connect
    (
        selectinverse, SIGNAL(triggered(bool)),
        this, SLOT(inverse_note_selection())
    );
    menuselect->addAction(selectinverse);

    QAction * quantize = new QAction(tr("Quantize"), m_tools_popup);
    quantize->setShortcut(tr("Ctrl+Q"));
    connect(quantize, SIGNAL(triggered(bool)), this, SLOT(quantize_notes()));
    menutiming->addAction(quantize);

    QAction * tighten = new QAction(tr("Tighten"), m_tools_popup);
    tighten->setShortcut(tr("Ctrl+T"));
    connect(tighten, SIGNAL(triggered(bool)), this, SLOT(tighten_notes()));
    menutiming->addAction(tighten);

    char num[16];
    QAction * transpose[24];     /* fill out note transpositions */
    for (int t = -12; t <= 12; ++t)
    {
        if (t != 0)
        {
            snprintf(num, sizeof num, "%+d [%s]", t, c_interval_text[abs(t)]);
            transpose[t + 12] = new QAction(num, m_tools_popup);
            transpose[t + 12]->setData(t);
            menupitch->addAction(transpose[t + 12]);
            connect
            (
                transpose[t + 12], SIGNAL(triggered(bool)),
                this, SLOT(transpose_notes())
            );
        }
        else
            menupitch->addSeparator();
    }
    m_tools_popup->addMenu(menuselect);
    m_tools_popup->addMenu(menutiming);
    m_tools_popup->addMenu(menupitch);
}

/**
 *  Consider adding Aftertouch events.
 */

void
qseqeditframe64::select_all_notes ()
{
    m_seq->select_events(EVENT_NOTE_ON, 0);
    m_seq->select_events(EVENT_NOTE_OFF, 0);
}

/**
 *  Consider adding Aftertouch events.
 */

void
qseqeditframe64::inverse_note_selection ()
{
    m_seq->select_events(EVENT_NOTE_ON, 0, true);
    m_seq->select_events(EVENT_NOTE_OFF, 0, true);
}

/**
 *  Consider adding Aftertouch events.
 */

void
qseqeditframe64::quantize_notes ()
{
    m_seq->push_undo();
    m_seq->quantize_events(EVENT_NOTE_ON, 0, m_seq->get_snap_tick(), 1, true);
}

/**
 *  Consider adding Aftertouch events.
 */

void
qseqeditframe64::tighten_notes ()
{
    m_seq->push_undo();
    m_seq->quantize_events(EVENT_NOTE_ON, 0, m_seq->get_snap_tick(), 2, true);
}

/**
 *  Consider adding Aftertouch events.
 */

void
qseqeditframe64::transpose_notes ()
{
    QAction * senderAction = (QAction *) sender();
    int transposeval = senderAction->data().toInt();
    m_seq->push_undo();
    m_seq->transpose_notes(transposeval, 0);
}

/**
 *  Popup menu sequences button.
 */

void
qseqeditframe64::sequences ()
{
    if (not_nullptr(m_sequences_popup))
    {
        m_sequences_popup->exec
        (
            ui->m_button_sequence->mapToGlobal
            (
                QPoint
                (
                    ui->m_button_sequence->width()-2,
                    ui->m_button_sequence->height()-2
                )
            )
        );
    }
}

/**
 *  A case where a macro makes the code easier to read.
 */

#define SET_BG_SEQ(seq) \
    std::bind(&qseqeditframe64::set_background_sequence, this, seq)

/**
 *  Builds the Tools popup menu on the fly.  Analogous to seqedit ::
 *  popup_sequence_menu().
 */

void
qseqeditframe64::popup_sequence_menu ()
{
    if (is_nullptr(m_sequences_popup))
    {
        m_sequences_popup = new QMenu(this);
    }

    QAction * off = new QAction(tr("Off"), m_sequences_popup);
    connect(off, &QAction::triggered, SET_BG_SEQ(SEQ64_SEQUENCE_LIMIT));
    (void) m_sequences_popup->addAction(off);
    (void) m_sequences_popup->addSeparator();
    int seqsinset = usr().seqs_in_set();
    for (int sset = 0; sset < c_max_sets; ++sset)
    {
        QMenu * menusset = nullptr;
        if (perf().screenset_is_active(sset))
        {
            char number[8];
            snprintf(number, sizeof number, "[%d]", sset);
            menusset = m_sequences_popup->addMenu(number);
        }
        for (int seq = 0; seq < seqsinset; ++seq)
        {
            char name[32];
            int s = sset * seqsinset + seq;
            sequence * sp = perf().get_sequence(s);
            if (not_nullptr(sp))
            {
                snprintf(name, sizeof name, "[%d] %.13s", s, sp->name().c_str());

                QAction * item = new QAction(tr(name), menusset);
                menusset->addAction(item);
                connect(item, &QAction::triggered, SET_BG_SEQ(s));
            }
        }
    }
}

/**
 *  Sets the given background sequence for the Pattern editor so that the
 *  musician has something to see that can be played against.  As a new
 *  feature, it is also passed to the sequence, so that it can be saved as
 *  part of the sequence data, but only if less or equal to the maximum
 *  single-byte MIDI value, 127.
 *
 *  Note that the "initial value" for this parameter is a static variable that
 *  gets set to the new value, so that opening up another sequence causes the
 *  sequence to take on the new "initial value" as well.  A feature, but
 *  should it be optional?  Now it is, based on the setting of
 *  usr().global_seq_feature().
 *
 *  This function is similar to seqedit::set_background_sequence().
 */

void
qseqeditframe64::set_background_sequence (int seqnum)
{
    m_bgsequence = seqnum;                      /* should check this value!  */
    if (usr().global_seq_feature())
        usr().seqedit_bgsequence(seqnum);

    if (SEQ64_IS_DISABLED_SEQUENCE(seqnum) || ! perf().is_active(seqnum))
    {
        ui->m_entry_sequence->setText("Off");
        m_seqroll->set_background_sequence(false, SEQ64_SEQUENCE_LIMIT);
    }
    sequence * seq = perf().get_sequence(seqnum);
    if (not_nullptr(seq))
    {
        char name[24];
        snprintf(name, sizeof name, "[%d] %.13s", seqnum, seq->name().c_str());
printf("seq name = '%s'\n", name);
        ui->m_entry_sequence->setText(name);
        m_seqroll->set_background_sequence(true, seqnum);
        if (seqnum < usr().max_sequence())      /* even more restrictive */
            m_seq->background_sequence(seqnum);
    }
}

/**
 *  Builds the Event (data) popup menu on the fly.  Analogous to seqedit ::
 *  popup_event_menu().

void
qseqeditframe64::popup_event_menu ()
{
    if (is_nullptr(m_events_popup))
    {
        m_events_popup = new QMenu(this);
    }
}
 */

/**
 *  Sets the data type based on the given parameters.  This function uses the
 *  hardwired array c_controller_names.
 *
 * \param status
 *      The current editing status.
 *
 * \param control
 *      The control value.  However, we really need to validate it!
 */

void
qseqeditframe64::set_data_type (midibyte status, midibyte control)
{
    // TODO:
    // m_editing_status = status;
    // m_editing_cc = control;
    // m_seqevent_wid->set_data_type(status, control);
    // m_seqdata_wid->set_data_type(status, control);
    // m_seqroll_wid->set_data_type(status, control);

    char hex[8];
    char type[80];
    snprintf(hex, sizeof hex, "[0x%02X]", status);
    if (status == EVENT_NOTE_OFF)
        snprintf(type, sizeof type, "Note Off");
    else if (status == EVENT_NOTE_ON)
        snprintf(type, sizeof type, "Note On");
    else if (status == EVENT_AFTERTOUCH)
        snprintf(type, sizeof type, "Aftertouch");
    else if (status == EVENT_CONTROL_CHANGE)
    {
        int bus = m_seq->get_midi_bus();
        int channel = m_seq->get_midi_channel();
        std::string ccname(c_controller_names[control]);
        if (usr().controller_active(bus, channel, control))
            ccname = usr().controller_name(bus, channel, control);

        snprintf(type, sizeof type, "Control Change - %s", ccname.c_str());
    }
    else if (status == EVENT_PROGRAM_CHANGE)
        snprintf(type, sizeof type, "Program Change");
    else if (status == EVENT_CHANNEL_PRESSURE)
        snprintf(type, sizeof type, "Channel Pressure");
    else if (status == EVENT_PITCH_WHEEL)
        snprintf(type, sizeof type, "Pitch Wheel");
    else
        snprintf(type, sizeof type, "Unknown MIDI Event");

    char text[80];
    snprintf(text, sizeof text, "%s %s", hex, type);
    ui->m_entry_data->setText(text);
}

/**
 * Follow-progress callback.
 */

#ifdef SEQ64_FOLLOW_PROGRESS_BAR

/**
 *  Passes the Follow status to the qseqroll object.  When qseqroll has been
 *  upgraded to support follow-progress, then enable this macro in
 *  libseq64/include/seq64_features.h.  Also applies to qperfroll.
 */

void
qseqeditframe64::follow (bool ischecked)
{
    m_seqroll->progress_follow(ischecked);
}

/**
 *  Checks the position of the tick, and, if it is in a different piano-roll
 *  "page" than the last page, moves the page to the next page.
 *
 *  We don't want to do any of this if the length of the sequence fits in the
 *  window, but for now it doesn't hurt; the progress bar just never meets the
 *  criterion for moving to the next page.
 *
 * \todo
 *      -   If playback is disabled (such as by a trigger), then do not update
 *          the page;
 *      -   When it comes back, make sure we're on the correct page;
 *      -   When it stops, put the window back to the beginning, even if the
 *          beginning is not defined as "0".
 */

void
qseqeditframe64::follow_progress ()
{
    int w = m_seqroll->window_width();
    QScrollBar * hadjust = ui->rollScrollArea->h_scroll();
    int scrollx = hadjust->value();
    if (m_seqroll->get_expanded_record() && m_seq->get_recording())
    {
        // double h_max_value = m_seq->get_length() - w * m_zoom;
        // hadjust->setValue(int(h_max_value));
        int newx = scrollx + w;
        hadjust->setValue(newx);
    }
    else                                        /* use for non-recording */
    {
        midipulse progress_tick = m_seq->get_last_tick();
        if (progress_tick > 0 && m_seqroll->progress_follow())
        {
            // int prog_x = progress_tick / m_zoom + SEQ64_PROGRESS_PAGE_OVERLAP;
            int prog_x = progress_tick / m_zoom;
            int page = prog_x / w;
            if (page != m_seqroll->scroll_page() || (page == 0 && scrollx != 0))
            {
                m_seqroll->scroll_page(page);
                hadjust->setValue(prog_x);
                // set_scroll_x();              // not needed
            }
        }
    }
}

#else

void
qseqeditframe64::follow_progress ()
{
    // No code, never follow the progress bar.
}

#endif  // SEQ64_FOLLOW_PROGRESS_BAR

/**
 *  Updates the grid-snap values and control based on the index.  The value is
 *  passed to the set_snap() function for processing.
 *
 * \param index
 *      Provides the index selected from the combo-box.
 */

void
qseqeditframe64::update_grid_snap (int index)
{
    int qnfactor = m_ppqn * 4;
    int item = s_snap_items[index];
    int v = qnfactor / item;
    set_snap(v);
}

/**
 *  Selects the given snap value, which is the number of ticks in a snap-sized
 *  interval.  It is passed to the seqroll, seqevent, and sequence objects, as
 *  well.
 *
 *  The default initial snap is the default PPQN divided by 4, or the
 *  equivalent of a 16th note (48 ticks).  The snap divisor is 192 * 4 / 48 or
 *  16.
 *
 * \param s
 *      The prospective snap value to set.  It is checked only to make sure it
 *      is greater than 0, to avoid a numeric exception.
 */

void
qseqeditframe64::set_snap (int s)
{
    if (s > 0 && s != m_snap)
    {
        m_snap = s;
        m_initial_snap = s;
        m_seqroll->set_snap(s);
        m_seq->set_snap_tick(s);
        m_seqevent->set_snap(s);
    }
}

/**
 *
 */

void
qseqeditframe64::reset_grid_snap ()
{
    ui->m_combo_snap->setCurrentIndex(4);
    update_draw_geometry();
}

/**
 *  Updates the note-length values and control based on the index.  It is passed
 *  to the set_note_length() function for processing.
 *
 * \param index
 *      Provides the index selected from the combo-box.
 */

void
qseqeditframe64::update_note_length (int index)
{
    int qnfactor = m_ppqn * 4;
    int item = s_snap_items[index];
    int v = qnfactor / item;
    set_note_length(v);
}

/**
 *  Selects the given note-length value.
 *
 * \warning
 *      Currently, we don't handle changes in the global PPQN after the
 *      creation of the menu.  The creation of the menu hard-wires the values
 *      of note-length.  To adjust for a new global PQN, we will need to store
 *      the original PPQN (m_original_ppqn = m_ppqn), and then adjust the
 *      notelength based on the new PPQN.  For example if the new PPQN is
 *      twice as high as 192, then the notelength should double, though the
 *      text displayed in the "Note length" field should remain the same.
 *      However, we do adjust for a non-default PPQN at startup time.
 *
 * \param notelength
 *      Provides the note length in units of MIDI pulses.
 */

void
qseqeditframe64::set_note_length (int notelength)
{
#ifdef CAN_MODIFY_GLOBAL_PPQN
    if (m_ppqn != m_original_ppqn)
    {
        double factor = double(m_ppqn) / double(m_original);
        notelength = int(notelength * factor + 0.5);
    }
#endif

    m_note_length = notelength;
    m_initial_note_length = notelength;
    m_seqroll->set_note_length(notelength);
}

/**
 *
 */

void
qseqeditframe64::reset_note_length ()
{
    ui->m_combo_note->setCurrentIndex(4);
    update_draw_geometry();
}

/**
 *  Updates the grid-zoom values and control based on the index.  The value is
 *  passed to the set_zoom() function for processing.
 *
 * \param index
 *      Provides the index selected from the combo-box.
 */

void
qseqeditframe64::update_zoom (int index)
{
    int v = s_zoom_items[index];
    set_zoom(v);
}

/**
 *
 */

void
qseqeditframe64::zoom_in ()
{
    if (m_zoom > 1 && m_zoom > usr().min_zoom())
        m_zoom /= 2;

    int index = s_lookup_zoom(m_zoom);
    ui->m_combo_zoom->setCurrentIndex(index);
    m_seqroll->zoom_in();
    m_seqtime->zoom_in();
    m_seqevent->zoom_in();
    m_seqdata->zoom_in();
    update_draw_geometry();
}

/**
 *
 */

void
qseqeditframe64::zoom_out ()
{
    if (m_zoom < usr().max_zoom())
    {
        m_zoom *= 2;
        m_seqroll->zoom_out();
        m_seqtime->zoom_out();
        m_seqevent->zoom_out();
        m_seqdata->zoom_out();
    }
    else                                /* wrap around to beginning */
    {
        int v = s_zoom_items[0];
        set_zoom(v);
    }

    int index = s_lookup_zoom(m_zoom);
    ui->m_combo_zoom->setCurrentIndex(index);
    update_draw_geometry();
}

/**
 *
 */

void
qseqeditframe64::set_zoom (int z)
{
    if ((z >= usr().min_zoom()) && (z <= usr().max_zoom()))
    {
        int index = s_lookup_zoom(z);
        ui->m_combo_zoom->setCurrentIndex(index);
        m_zoom = z;
        m_seqroll->set_zoom(z);
        m_seqtime->set_zoom(z);
        m_seqdata->set_zoom(z);
        m_seqevent->set_zoom(z);
        set_dirty();
    }
}

/**
 *
 */

void
qseqeditframe64::reset_zoom ()
{
    ui->m_combo_zoom->setCurrentIndex(1);
    // set_dirty();
}

/**
 *  Handles updates to the key selection.
 */

void
qseqeditframe64::update_key (int index)
{
    if (index != m_key && index >= 0 && index < SEQ64_OCTAVE_SIZE)
    {
        set_key(index);
        set_dirty();
    }
}

/**
 *
 */

void
qseqeditframe64::set_key (int key)
{
    if (key >= 0 && key < SEQ64_OCTAVE_SIZE)
    {
        ui->m_combo_key->setCurrentIndex(key);
        m_seqroll->set_key(key);
    }
}

/**
 *
 */

void
qseqeditframe64::reset_key ()
{
    ui->m_combo_key->setCurrentIndex(0);
    m_seqroll->set_key(0);
}

/**
 *  Handles updates to the scale selection.
 */

void
qseqeditframe64::update_scale (int index)
{
    if (index != m_scale && index >= 0 && index < c_scale_size)
    {
        set_scale(index);
        set_dirty();
    }
}

/**
 *
 */

void
qseqeditframe64::set_scale (int scale)
{
    if (scale >= 0 && scale < c_scale_size)
    {
        ui->m_combo_scale->setCurrentIndex(scale);
        m_seqroll->set_scale(scale);
    }
}

/**
 *
 */

void
qseqeditframe64::reset_scale ()
{
    ui->m_combo_scale->setCurrentIndex(0);
    m_seqroll->set_scale(0);
}

/**
 *  Popup menu events button.
 */

void
qseqeditframe64::events ()
{
    if (not_nullptr(m_events_popup))
    {
        m_events_popup->exec
        (
            ui->m_button_event->mapToGlobal
            (
                QPoint
                (
                    ui->m_button_event->width()-2,
                    ui->m_button_event->height()-2
                )
            )
        );
    }
}

/**
 *  Populates the event-selection menu, in necessary, and then pops it up.
 *  Also see the handling of the m_button_data and m_entry_data objects.
 */

void
qseqeditframe64::popup_event_menu ()
{
    if (is_nullptr(m_events_popup))
    {
        m_events_popup = new QMenu(this);
    }

    int buss = m_seq->get_midi_bus();
    int channel = m_seq->get_midi_channel();
    repopulate_event_menu(buss, channel);
    // m_events_popup->popup(0, 0);
}

/**
 *  Sets the menu pixmap depending on the given state, where true is a
 *  full menu (black background), and empty menu (gray background).
 *
 * \param state
 *      If true, the full-menu image will be created.  Otherwise, the
 *      empty-menu image will be created.
 *
 * \return
 *      Returns a pointer to the created image.
 */

QIcon *
qseqeditframe64::create_menu_image (bool state)
{
    QPixmap p(state? menu_full_xpm : menu_empty_xpm);
    return new QIcon(p);
}

/**
 *  A case where a macro makes the code easier to read.
 */

#define SET_DATA_TYPE(status, cc) \
    std::bind(&qseqeditframe64::set_data_type, this, status, cc)

/**
 *  Function to create event menu entries.  Too damn big!
 */

void
qseqeditframe64::set_event_entry
(
    QMenu * menu,
    const std::string & text,
    bool present,
    midibyte status,
    midibyte control    // = 0
)
{
    QAction * item = new QAction(*create_menu_image(present), "XXX");
    /**
     *
    menu->items().push_back
    (
        connect
        (
            menu, *create_menu_image(present),
            SET_DATA_TYPE(status, control)
        )
    );
     */
    if (present && m_first_event == 0x00)
    {
        m_first_event = status;
        m_first_event_name = text;
        set_data_type(status, 0);       // need m_first_control value!
    }
}

/**
 *  Populates the event-selection menu that drops from the "Event" button
 *  in the bottom row of the Pattern editor.
 *
 *  This menu has a large number of items.  They are filled in by
 *  code, but can also be loaded from sequencer64.usr, qpseq64.usr, etc.
 *
 *  This function first loops through all of the existing events in the
 *  sequence in order to determine what events exist in it.  If any of the
 *  following events are found, their entry in the menu is marked by a filled
 *  square, rather than a hollow square:
 *
 *      -   Note On
 *      -   Note off
 *      -   Aftertouch
 *      -   Program Change
 *      -   Channel Pressure
 *      -   Pitch Wheel
 *      -   Control Changes from 0 to 127
 *
 * \param buss
 *      The selected bus number.
 *
 * \param channel
 *      The selected channel number.
 */

void
qseqeditframe64::repopulate_event_menu (int buss, int channel)
{
    bool ccs[SEQ64_MIDI_COUNT_MAX];
    bool note_on = false;
    bool note_off = false;
    bool aftertouch = false;
    bool program_change = false;
    bool channel_pressure = false;
    bool pitch_wheel = false;
    midibyte status, cc;
    memset(ccs, false, sizeof(bool) * SEQ64_MIDI_COUNT_MAX);
    m_seq->reset_draw_marker();
    while (m_seq->get_next_event(status, cc))           /* used only here!  */
    {
        switch (status)
        {
        case EVENT_NOTE_OFF:
            note_off = true;
            break;

        case EVENT_NOTE_ON:
            note_on = true;
            break;

        case EVENT_AFTERTOUCH:
            aftertouch = true;
            break;

        case EVENT_CONTROL_CHANGE:
            ccs[cc] = true;
            break;

        case EVENT_PITCH_WHEEL:
            pitch_wheel = true;
            break;

        case EVENT_PROGRAM_CHANGE:
            program_change = true;
            break;

        case EVENT_CHANNEL_PRESSURE:
            channel_pressure = true;
            break;
        }
    }

    m_events_popup = new QMenu(this);
    set_event_entry(m_events_popup, "Note On Velocity", note_on, EVENT_NOTE_ON);
//  m_events_popup->items().push_back(SeparatorElem());
    set_event_entry(m_events_popup, "Note Off Velocity", note_off, EVENT_NOTE_OFF);
    set_event_entry(m_events_popup, "Aftertouch", aftertouch, EVENT_AFTERTOUCH);
    set_event_entry
    (
        m_events_popup, "Program Change", program_change, EVENT_PROGRAM_CHANGE
    );
    set_event_entry
    (
        m_events_popup, "Channel Pressure", channel_pressure, EVENT_CHANNEL_PRESSURE
    );
    set_event_entry(m_events_popup, "Pitch Wheel", pitch_wheel, EVENT_PITCH_WHEEL);
//  m_events_popup->items().push_back(SeparatorElem());

    /**
     *  Create the 8 sub-menus for the various ranges of controller
     *  changes, shown 16 per sub-menu.
     */

    const int menucount = 8;
    const int itemcount = 16;
    char b[32];
    for (int submenu = 0; submenu < menucount; ++submenu)
    {
        int offset = submenu * itemcount;
        snprintf(b, sizeof b, "Controls %d-%d", offset, offset + itemcount - 1);
        QMenu * menucc = new QMenu(this);
        for (int item = 0; item < itemcount; ++item)
        {
            /*
             * Do we really want the default controller name to start?
             * That's what the legacy Seq24 code does!  We need to document
             * it in the seq24-doc and sequencer64-doc projects.  Also, there
             * was a bug in Seq24 where the instrument number was use re 1
             * to get the proper instrument... it needs to be decremented to
             * be re 0.
             */

            std::string controller_name(c_controller_names[offset + item]);
            const user_midi_bus & umb = usr().bus(buss);
            int inst = umb.instrument(channel);
            const user_instrument & uin = usr().instrument(inst);
            if (uin.is_valid())                             // redundant check
            {
                if (uin.controller_active(offset + item))
                    controller_name = uin.controller_name(offset + item);
            }
            set_event_entry
            (
                menucc, controller_name, ccs[offset+item],
                EVENT_CONTROL_CHANGE, offset + item
            );
        }
//      m_events_popup->items().push_back(MenuElem(std::string(b), *menucc));
    }
}

/**
 *  Populates the event-selection menu, in necessary, and then pops it up.
 *  Also see the handling of the m_button_minidata and m_entry_data objects.
 */

void
qseqeditframe64::popup_mini_event_menu ()
{
    if (is_nullptr(m_minidata_popup))
    {
        m_minidata_popup = new QMenu(this);
    }

    int buss = m_seq->get_midi_bus();
    int channel = m_seq->get_midi_channel();
    repopulate_mini_event_menu(buss, channel);
    // m_minidata_popup->popup(0, 0);
}

/**
 *  Populates the mini event-selection menu that drops from the mini-"Event"
 *  button in the bottom row of the Pattern editor.
 *  This menu has a much smaller number of items, only the ones that actually
 *  exist in the track/pattern/loop/sequence.
 *
 * \param buss
 *      The selected bus number.
 *
 * \param channel
 *      The selected channel number.
 */

void
qseqeditframe64::repopulate_mini_event_menu (int buss, int channel)
{
    bool ccs[SEQ64_MIDI_COUNT_MAX];
    bool note_on = false;
    bool note_off = false;
    bool aftertouch = false;
    bool program_change = false;
    bool channel_pressure = false;
    bool pitch_wheel = false;
    midibyte status, cc;
    memset(ccs, false, sizeof(bool) * SEQ64_MIDI_COUNT_MAX);
    m_seq->reset_draw_marker();
    while (m_seq->get_next_event(status, cc))            /* used only here!  */
    {
        switch (status)
        {
        case EVENT_NOTE_OFF:
            note_off = true;
            break;

        case EVENT_NOTE_ON:
            note_on = true;
            break;

        case EVENT_AFTERTOUCH:
            aftertouch = true;
            break;

        case EVENT_CONTROL_CHANGE:
            ccs[cc] = true;
            break;

        case EVENT_PITCH_WHEEL:
            pitch_wheel = true;
            break;

        case EVENT_PROGRAM_CHANGE:
            program_change = true;
            break;

        case EVENT_CHANNEL_PRESSURE:
            channel_pressure = true;
            break;
        }
    }
    m_minidata_popup = new QMenu(this);
    bool any_events = false;
    if (note_on)
    {
        any_events = true;
        set_event_entry(m_minidata_popup, "Note On Velocity", true, EVENT_NOTE_ON);
    }
    if (note_off)
    {
        any_events = true;
        set_event_entry
        (
            m_minidata_popup, "Note Off Velocity", true, EVENT_NOTE_OFF
        );
    }
    if (aftertouch)
    {
        any_events = true;
        set_event_entry(m_minidata_popup, "Aftertouch", true, EVENT_AFTERTOUCH);
    }
    if (program_change)
    {
        any_events = true;
        set_event_entry
        (
            m_minidata_popup, "Program Change", true, EVENT_PROGRAM_CHANGE
        );
    }
    if (channel_pressure)
    {
        any_events = true;
        set_event_entry
        (
            m_minidata_popup, "Channel Pressure", true, EVENT_CHANNEL_PRESSURE
        );
    }
    if (pitch_wheel)
    {
        any_events = true;
        set_event_entry
        (
            m_minidata_popup, "Pitch Wheel", true, EVENT_PITCH_WHEEL
        );
    }

//  m_minidata_popup->items().push_back(SeparatorElem());

    /**
     *  Create the one menu for the controller changes that actually exist in
     *  the track, if any.
     */

    const int itemcount = SEQ64_MIDI_COUNT_MAX;             /* 128 */
    for (int item = 0; item < itemcount; ++item)
    {
        std::string controller_name(c_controller_names[item]);
        const user_midi_bus & umb = usr().bus(buss);
        int inst = umb.instrument(channel);
        const user_instrument & uin = usr().instrument(inst);
        if (uin.is_valid())                             // redundant check
        {
            if (uin.controller_active(item))
                controller_name = uin.controller_name(item);
        }
        if (ccs[item])
        {
            any_events = true;
            set_event_entry
            (
                m_minidata_popup, controller_name, true,
                EVENT_CONTROL_CHANGE, item
            );
        }
    }
    if (any_events)
    {
        // Here, we would like to pre-select the first kind of event found,
        // somehow.
    }
    else
        set_event_entry(m_minidata_popup, "(no events)", false, 0);

    // Gtk::Image * eventflag = manage(create_menu_image(any_events));
    // if (not_nullptr(eventflag))
        // m_button_minidata->set_image(*eventflag);
}

/**
 *
 */

void
qseqeditframe64::set_dirty ()
{
    m_seqroll->set_dirty();
    m_seqtime->set_dirty();
    m_seqevent->set_dirty();
    m_seqdata->set_dirty();
    update_draw_geometry();
}

/**
 *
 */

void
qseqeditframe64::update_draw_geometry()
{
    /*
     *  QString lentext(QString::number(m_seq->get_num_measures()));
     *  ui->cmbSeqLen->setCurrentText(lentext);
     *  mContainer->adjustSize();
     */

    m_seqtime->updateGeometry();
    m_seqroll->updateGeometry();
    m_seqevent->updateGeometry();
    m_seqdata->updateGeometry();
}

/**
 *
 */

void
qseqeditframe64::set_editor_mode (seq64::edit_mode_t mode)
{
    if (mode != m_edit_mode)
    {
        m_edit_mode = mode;
        perf().seq_edit_mode(*m_seq, mode);
        m_seqroll->update_edit_mode(mode);
    }
}

/**
 *  Implements the actions brought forth from the Tools (hammer) button.
 *
 *  Note that the push_undo() calls push all of the current events (in
 *  sequence::m_events) onto the stack (as a single entry).
 */

void
qseqeditframe64::do_action (edit_action_t action, int var)
{
    switch (action)
    {
    case c_select_all_notes:
        m_seq->select_all_notes();
        break;

    case c_select_inverse_notes:
        m_seq->select_all_notes(true);
        break;

    case c_select_all_events:
        m_seq->select_events(m_editing_status, m_editing_cc);
        break;

    case c_select_inverse_events:
        m_seq->select_events(m_editing_status, m_editing_cc, true);
        break;

#ifdef USE_STAZED_ODD_EVEN_SELECTION

    case c_select_even_notes:
        m_seq->select_even_or_odd_notes(var, true);
        break;

    case c_select_odd_notes:
        m_seq->select_even_or_odd_notes(var, false);
        break;

#endif

#ifdef USE_STAZED_RANDOMIZE_SUPPORT

    case c_randomize_events:
        m_seq->randomize_selected(m_editing_status, m_editing_cc, var);
        break;

#endif

    case c_quantize_notes:

        /*
         * sequence::quantize_events() is used in recording as well, so we do
         * not want to incorporate sequence::push_undo() into it.  So we make
         * a new function to do that.
         */

        m_seq->push_quantize(EVENT_NOTE_ON, 0, m_snap, 1, true);
        break;

    case c_quantize_events:
        m_seq->push_quantize(m_editing_status, m_editing_cc, m_snap, 1);
        break;

    case c_tighten_notes:
        m_seq->push_quantize(EVENT_NOTE_ON, 0, m_snap, 2, true);
        break;

    case c_tighten_events:
        m_seq->push_quantize(m_editing_status, m_editing_cc, m_snap, 2);
        break;

    case c_transpose_notes:                     /* regular transpose    */
        m_seq->transpose_notes(var, 0);
        break;

    case c_transpose_h:                         /* harmonic transpose   */
        m_seq->transpose_notes(var, m_scale);
        break;

#ifdef USE_STAZED_COMPANDING

    case c_expand_pattern:
        m_seq->multiply_pattern(2.0);
        break;

    case c_compress_pattern:
        m_seq->multiply_pattern(0.5);
        break;
#endif

    default:
        break;
    }
    set_dirty();
}

}           // namespace seq64

/*
 * qseqeditframe64.cpp
 *
 * vim: sw=4 ts=4 wm=4 et ft=cpp
 */
