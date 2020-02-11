//==========================================================================
// Name:            hamlib.cpp
//
// Purpose:         Hamlib integration for FreeDV
// Created:         May 2013
// Authors:         Joel Stanley
//
// License:
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License version 2.1,
//  as published by the Free Software Foundation.  This program is
//  distributed in the hope that it will be useful, but WITHOUT ANY
//  WARRANTY; without even the implied warranty of MERCHANTABILITY or
//  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
//  License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, see <http://www.gnu.org/licenses/>.
//
//==========================================================================
#include <hamlib.h>

#include <vector>
#include <algorithm>

using namespace std;

typedef std::vector<const struct rig_caps *> riglist_t;

static bool rig_cmp(const struct rig_caps *rig1, const struct rig_caps *rig2);
static int build_list(const struct rig_caps *rig, rig_ptr_t);

Hamlib::Hamlib() : m_rig(NULL) {
    /* Stop hamlib from spewing info to stderr. */
    rig_set_debug(RIG_DEBUG_NONE);

    /* Create sorted list of rigs. */
    rig_load_all_backends();
    rig_list_foreach(build_list, &m_rigList);
    sort(m_rigList.begin(), m_rigList.end(), rig_cmp);

    /* Reset debug output. */
    rig_set_debug(RIG_DEBUG_VERBOSE);

    m_rig = NULL;
}

Hamlib::~Hamlib() {
	if(m_rig)
		close();
}

static int build_list(const struct rig_caps *rig, rig_ptr_t rigList) {
    ((riglist_t *)rigList)->push_back(rig); 
    return 1;
}

static bool rig_cmp(const struct rig_caps *rig1, const struct rig_caps *rig2) {
    /* Compare manufacturer. */
    int r = strcasecmp(rig1->mfg_name, rig2->mfg_name);
    if (r != 0)
        return r < 0;

    /* Compare model. */
    r = strcasecmp(rig1->model_name, rig2->model_name);
    if (r != 0)
        return r < 0;

    /* Compare rig ID. */
    return rig1->rig_model < rig2->rig_model;
}

void Hamlib::populateComboBox(wxComboBox *cb) {

    riglist_t::const_iterator rig = m_rigList.begin();
    for (; rig !=m_rigList.end(); rig++) {
        char name[128];
        snprintf(name, 128, "%s %s", (*rig)->mfg_name, (*rig)->model_name); 
        cb->Append(name);
    }
}

bool Hamlib::connect(unsigned int rig_index, const char *serial_port, const int serial_rate) {

    /* Look up model from index. */

    if (rig_index >= m_rigList.size()) {
        fprintf(stderr, "rig_index invalid, returning\n");
        return false;
    }

    fprintf(stderr, "rig: %s %s (%d)\n", m_rigList[rig_index]->mfg_name,
            m_rigList[rig_index]->model_name, m_rigList[rig_index]->rig_model);

    if(m_rig) {
        fprintf(stderr, "Closing old hamlib instance!\n");
        close();
    }

    /* Initialise, configure and open. */

    m_rig = rig_init(m_rigList[rig_index]->rig_model);

    if (!m_rig) {
        fprintf(stderr, "rig_init() failed, returning\n");
        return false;
    }
    fprintf(stderr, "rig_init() OK ....\n");

    /* TODO we may also need civaddr for Icom */

    strncpy(m_rig->state.rigport.pathname, serial_port, FILPATHLEN - 1);
    if (serial_rate) {
        fprintf(stderr, "hamlib: setting serial rate: %d\n", serial_rate);
        m_rig->state.rigport.parm.serial.rate = serial_rate;
    }
    fprintf(stderr, "hamlib: serial rate: %d\n", m_rig->state.rigport.parm.serial.rate);
    fprintf(stderr, "hamlib: data_bits..: %d\n", m_rig->state.rigport.parm.serial.data_bits);
    fprintf(stderr, "hamlib: stop_bits..: %d\n", m_rig->state.rigport.parm.serial.stop_bits);

    if (rig_open(m_rig) == RIG_OK) {
        fprintf(stderr, "hamlib: rig_open() OK\n");
        return true;
    }
    fprintf(stderr, "hamlib: rig_open() failed ...\n");

    return false;
}

int Hamlib::get_serial_rate(void) {
    return m_rig->state.rigport.parm.serial.rate;
}

int Hamlib::get_data_bits(void) {
    return m_rig->state.rigport.parm.serial.data_bits;
}

int Hamlib::get_stop_bits(void) {
    return m_rig->state.rigport.parm.serial.stop_bits;
}

bool Hamlib::ptt(bool press, wxString &hamlibError) {
    fprintf(stderr,"Hamlib::ptt: %d\n", press);
    hamlibError = "";

    if(!m_rig)
        return false;

    /* TODO(Joel): make ON_DATA and ON configurable. */

    ptt_t on = press ? RIG_PTT_ON : RIG_PTT_OFF;

    /* TODO(Joel): what should the VFO option be? */

    int retcode = rig_set_ptt(m_rig, RIG_VFO_CURR, on);
    fprintf(stderr,"Hamlib::ptt: rig_set_ptt returned: %d\n", retcode);
    if (retcode != RIG_OK ) {
        fprintf(stderr, "rig_set_ptt: error = %s \n", rigerror(retcode));
        hamlibError = rigerror(retcode);
    }

    return retcode == RIG_OK;
}

bool Hamlib::is_correct_sideband(wxString &hamlibError) {
    bool retVal = false;

    fprintf(stderr,"Hamlib::is_correct_sideband\n");
    hamlibError = "";

    if(!m_rig)
        return false;

    rmode_t mode = RIG_MODE_NONE;
    pbwidth_t passband = 0;
    int result = rig_get_mode(m_rig, RIG_VFO_CURR, &mode, &passband);
    if (result != RIG_OK)
    {
        fprintf(stderr, "rig_get_mode: error = %s \n", rigerror(result));
        hamlibError = rigerror(result);
    }
    else
    {
        freq_t freq = 0;
        result = rig_get_freq(m_rig, RIG_VFO_CURR, &freq);
        if (result != RIG_OK)
        {
            fprintf(stderr, "rig_get_freq: error = %s \n", rigerror(result));
            hamlibError = rigerror(result);
        }
        else
        {
            fprintf(stderr, "is_correct_sideband detected sideband %s, freq %f\n", rig_strrmode(mode), freq);
            retVal = 
                (freq >= 10000000 && (mode == RIG_MODE_USB || mode == RIG_MODE_PKTUSB)) ||
                (freq < 10000000 && (mode == RIG_MODE_LSB || mode == RIG_MODE_PKTLSB));
            if (!retVal)
            {
                hamlibError = "Your radio may be set to the incorrect sideband for FreeDV (LSB under 10MHz, USB >= 10MHz). Please confirm settings on your radio.";
            }
        }
    }

    return retVal;
}

void Hamlib::close(void) {
    if(m_rig) {
        rig_close(m_rig);
        rig_cleanup(m_rig);
        m_rig = NULL;
    }
}
