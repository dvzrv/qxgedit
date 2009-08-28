// qxgeditMainForm.cpp
//
/****************************************************************************
   Copyright (C) 2005-2009, rncbc aka Rui Nuno Capela. All rights reserved.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation, Inc.,
   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

*****************************************************************************/

#include "qxgeditMainForm.h"

#include "qxgeditAbout.h"
#include "qxgeditOptions.h"

#include "qxgeditXGParamMap.h"
#include "qxgeditMidiDevice.h"

#include "qxgeditDial.h"
#include "qxgeditCombo.h"

#include "qxgeditOptionsForm.h"

#include <QApplication>
#include <QMessageBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QUrl>

#include <QTreeWidget>
#include <QHeaderView>

#include <QStatusBar>
#include <QLabel>

#include <QDragEnterEvent>
#include <QCloseEvent>
#include <QDropEvent>

#if defined(WIN32)
#undef HAVE_SIGNAL_H
#endif

#ifdef HAVE_SIGNAL_H
#include <signal.h>
#endif

// Specialties for thread-callback comunication.
#define QXGEDIT_SYSEX_EVENT QEvent::Type(QEvent::User + 1)


//----------------------------------------------------------------------------
// qxgeditMainForm -- UI wrapper form.
//
qxgeditMainForm *qxgeditMainForm::g_pMainForm = NULL;

// Constructor.
qxgeditMainForm::qxgeditMainForm (
	QWidget *pParent, Qt::WindowFlags wflags )
	: QMainWindow(pParent, wflags)
{
	// Setup UI struct...
	m_ui.setupUi(this);

	// Pseudo-singleton reference setup.
	g_pMainForm = this;

	// Initialize some pointer references.
	m_pOptions = NULL;
	m_pMidiDevice = NULL;
	m_pParamMap = NULL;

	// We'll start clean.
	m_iUntitled   = 0;
	m_iDirtyCount = 0;

	// Instrument/Normal Voice combo-box view soft-mutex.
	m_iMultipartVoiceUpdate = 0;

#ifdef HAVE_SIGNAL_H
	// Set to ignore any fatal "Broken pipe" signals.
	::signal(SIGPIPE, SIG_IGN);
#endif

	// Create some statusbar labels...
	const QSize pad(4, 0);
	QLabel *pLabel;

	// Track status.
	pLabel = new QLabel(tr("Session"));
	pLabel->setAlignment(Qt::AlignLeft);
	pLabel->setToolTip(tr("Current session name"));
	pLabel->setAutoFillBackground(true);
	m_statusItems[StatusName] = pLabel;
	statusBar()->addWidget(pLabel, 2);

	// Session modification status.
	pLabel = new QLabel(tr("MOD"));
	pLabel->setAlignment(Qt::AlignHCenter);
	pLabel->setMinimumSize(pLabel->sizeHint() + pad);
	pLabel->setToolTip(tr("Session modification state"));
	pLabel->setAutoFillBackground(true);
	m_statusItems[StatusMod] = pLabel;
	statusBar()->addWidget(pLabel);

	// Some actions surely need those
	// shortcuts firmly attached...
	addAction(m_ui.viewMenubarAction);

	// Ah, make it stand right.
	setFocus();

	// UI signal/slot connections...
	QObject::connect(m_ui.fileNewAction,
		SIGNAL(triggered(bool)),
		SLOT(fileNew()));
	QObject::connect(m_ui.fileOpenAction,
		SIGNAL(triggered(bool)),
		SLOT(fileOpen()));
	QObject::connect(m_ui.fileSaveAction,
		SIGNAL(triggered(bool)),
		SLOT(fileSave()));
	QObject::connect(m_ui.fileSaveAsAction,
		SIGNAL(triggered(bool)),
		SLOT(fileSaveAs()));
	QObject::connect(m_ui.fileExitAction,
		SIGNAL(triggered(bool)),
		SLOT(fileExit()));

	QObject::connect(m_ui.viewMenubarAction,
		SIGNAL(triggered(bool)),
		SLOT(viewMenubar(bool)));
	QObject::connect(m_ui.viewStatusbarAction,
		SIGNAL(triggered(bool)),
		SLOT(viewStatusbar(bool)));
	QObject::connect(m_ui.viewToolbarAction,
		SIGNAL(triggered(bool)),
		SLOT(viewToolbar(bool)));
	QObject::connect(m_ui.viewOptionsAction,
		SIGNAL(triggered(bool)),
		SLOT(viewOptions()));

	QObject::connect(m_ui.helpAboutAction,
		SIGNAL(triggered(bool)),
		SLOT(helpAbout()));
	QObject::connect(m_ui.helpAboutQtAction,
		SIGNAL(triggered(bool)),
		SLOT(helpAboutQt()));

	QObject::connect(m_ui.fileMenu,
		SIGNAL(aboutToShow()),
		SLOT(updateRecentFilesMenu()));
}


// Destructor.
qxgeditMainForm::~qxgeditMainForm (void)
{
	// Free designated devices.
	if (m_pMidiDevice)
		delete m_pMidiDevice;
	if (m_pParamMap)
		delete m_pParamMap;

	// Pseudo-singleton reference shut-down.
	g_pMainForm = NULL;
}


// kind of singleton reference.
qxgeditMainForm *qxgeditMainForm::getInstance (void)
{
	return g_pMainForm;
}


// Make and set a proper setup options step.
void qxgeditMainForm::setup ( qxgeditOptions *pOptions )
{
	// We got options?
	m_pOptions = pOptions;

	// Initial decorations toggle state.
	m_ui.viewMenubarAction->setChecked(m_pOptions->bMenubar);
	m_ui.viewStatusbarAction->setChecked(m_pOptions->bStatusbar);
	m_ui.viewToolbarAction->setChecked(m_pOptions->bToolbar);

	// Initial decorations visibility state.
	viewMenubar(m_pOptions->bMenubar);
	viewStatusbar(m_pOptions->bStatusbar);
	viewToolbar(m_pOptions->bToolbar);

	// Try to restore old window positioning.
	m_pOptions->loadWidgetGeometry(this);

	// Primary startup stabilization...
	updateRecentFilesMenu();

	// XG master database...
	m_pParamMap = new qxgeditXGParamMap();

	// Start proper devices...
	m_pMidiDevice = new qxgeditMidiDevice(QXGEDIT_TITLE);
    m_pMidiDevice->setNotifyObject(this);
    m_pMidiDevice->setNotifySysexType(QXGEDIT_SYSEX_EVENT);

	// And respective connections...
	m_pMidiDevice->connectInputs(m_pOptions->midiInputs);
	m_pMidiDevice->connectOutputs(m_pOptions->midiOutputs);

	// Change to last known session dir...
	if (!m_pOptions->sSessionDir.isEmpty())
		QDir::setCurrent(m_pOptions->sSessionDir);

	// Setup UI widgets...
	QWidget *pCentralWidget = centralWidget();
	const QFont& font = pCentralWidget->font();
	pCentralWidget->setFont(QFont(font.family(), font.pointSize() - 2));

	XGParamMap *SYSTEM    = &(m_pParamMap->SYSTEM);
	XGParamMap *REVERB    = &(m_pParamMap->REVERB);
	XGParamMap *CHORUS    = &(m_pParamMap->CHORUS);
	XGParamMap *VARIATION = &(m_pParamMap->VARIATION);
	XGParamMap *MULTIPART = &(m_pParamMap->MULTIPART);
	XGParamMap *DRUMSETUP = &(m_pParamMap->DRUMSETUP);

	// SYSTEM...
	QObject::connect(m_ui.MasterResetButton,
		SIGNAL(clicked()),
		SLOT(masterResetButtonClicked()));

	m_ui.MasterTuneDial            -> set_param_map(SYSTEM, 0x00);
	m_ui.MasterVolumeDial          -> set_param_map(SYSTEM, 0x04);
	m_ui.MasterTransposeDial       -> set_param_map(SYSTEM, 0x06);

	// REVERB...
	m_ui.ReverbTypeCombo           -> set_param_map(REVERB, 0x00);
	m_ui.ReverbParam1Dial          -> set_param_map(REVERB, 0x02);
	m_ui.ReverbParam2Dial          -> set_param_map(REVERB, 0x03);
	m_ui.ReverbParam3Dial          -> set_param_map(REVERB, 0x04);
	m_ui.ReverbParam4Dial          -> set_param_map(REVERB, 0x05);
	m_ui.ReverbParam5Dial          -> set_param_map(REVERB, 0x06);
	m_ui.ReverbParam6Dial          -> set_param_map(REVERB, 0x07);
	m_ui.ReverbParam7Dial          -> set_param_map(REVERB, 0x08);
	m_ui.ReverbParam8Dial          -> set_param_map(REVERB, 0x09);
	m_ui.ReverbParam9Dial          -> set_param_map(REVERB, 0x0a);
	m_ui.ReverbParam10Dial         -> set_param_map(REVERB, 0x0b);
	m_ui.ReverbReturnDial          -> set_param_map(REVERB, 0x0c);
	m_ui.ReverbPanDial             -> set_param_map(REVERB, 0x0d);
	m_ui.ReverbParam11Dial         -> set_param_map(REVERB, 0x10);
	m_ui.ReverbParam12Dial         -> set_param_map(REVERB, 0x11);
	m_ui.ReverbParam13Dial         -> set_param_map(REVERB, 0x12);
	m_ui.ReverbParam14Dial         -> set_param_map(REVERB, 0x13);
	m_ui.ReverbParam15Dial         -> set_param_map(REVERB, 0x14);
	m_ui.ReverbParam16Dial         -> set_param_map(REVERB, 0x15);

	// CHORUS...
	m_ui.ChorusTypeCombo           -> set_param_map(CHORUS, 0x20);
	m_ui.ChorusParam1Dial          -> set_param_map(CHORUS, 0x22);
	m_ui.ChorusParam2Dial          -> set_param_map(CHORUS, 0x23);
	m_ui.ChorusParam3Dial          -> set_param_map(CHORUS, 0x24);
	m_ui.ChorusParam4Dial          -> set_param_map(CHORUS, 0x25);
	m_ui.ChorusParam5Dial          -> set_param_map(CHORUS, 0x26);
	m_ui.ChorusParam6Dial          -> set_param_map(CHORUS, 0x27);
	m_ui.ChorusParam7Dial          -> set_param_map(CHORUS, 0x28);
	m_ui.ChorusParam8Dial          -> set_param_map(CHORUS, 0x29);
	m_ui.ChorusParam9Dial          -> set_param_map(CHORUS, 0x2a);
	m_ui.ChorusParam10Dial         -> set_param_map(CHORUS, 0x2b);
	m_ui.ChorusReturnDial          -> set_param_map(CHORUS, 0x2c);
	m_ui.ChorusPanDial             -> set_param_map(CHORUS, 0x2d);
	m_ui.ChorusReverbDial          -> set_param_map(CHORUS, 0x2e);
	m_ui.ChorusParam11Dial         -> set_param_map(CHORUS, 0x30);
	m_ui.ChorusParam12Dial         -> set_param_map(CHORUS, 0x31);
	m_ui.ChorusParam13Dial         -> set_param_map(CHORUS, 0x32);
	m_ui.ChorusParam14Dial         -> set_param_map(CHORUS, 0x33);
	m_ui.ChorusParam15Dial         -> set_param_map(CHORUS, 0x34);
	m_ui.ChorusParam16Dial         -> set_param_map(CHORUS, 0x35);

	// VARIATION...
	m_ui.VariationTypeCombo        -> set_param_map(VARIATION, 0x40);
	m_ui.VariationParam1Dial       -> set_param_map(VARIATION, 0x42);
	m_ui.VariationParam2Dial       -> set_param_map(VARIATION, 0x44);
	m_ui.VariationParam3Dial       -> set_param_map(VARIATION, 0x46);
	m_ui.VariationParam4Dial       -> set_param_map(VARIATION, 0x48);
	m_ui.VariationParam5Dial       -> set_param_map(VARIATION, 0x4a);
	m_ui.VariationParam6Dial       -> set_param_map(VARIATION, 0x4c);
	m_ui.VariationParam7Dial       -> set_param_map(VARIATION, 0x4e);
	m_ui.VariationParam8Dial       -> set_param_map(VARIATION, 0x50);
	m_ui.VariationParam9Dial       -> set_param_map(VARIATION, 0x52);
	m_ui.VariationParam10Dial      -> set_param_map(VARIATION, 0x54);
	m_ui.VariationReturnDial       -> set_param_map(VARIATION, 0x56);
	m_ui.VariationPanDial          -> set_param_map(VARIATION, 0x57);
	m_ui.VariationReverbDial       -> set_param_map(VARIATION, 0x58);
	m_ui.VariationChorusDial       -> set_param_map(VARIATION, 0x59);
	m_ui.VariationConnectDial      -> set_param_map(VARIATION, 0x5a);
	m_ui.VariationPartDial         -> set_param_map(VARIATION, 0x5b);
	m_ui.VariationWheelDial        -> set_param_map(VARIATION, 0x5c);
	m_ui.VariationBendDial         -> set_param_map(VARIATION, 0x5d);
	m_ui.VariationCATDial          -> set_param_map(VARIATION, 0x5e);
	m_ui.VariationAC1Dial          -> set_param_map(VARIATION, 0x5f);
	m_ui.VariationAC2Dial          -> set_param_map(VARIATION, 0x60);
	m_ui.VariationParam11Dial      -> set_param_map(VARIATION, 0x70);
	m_ui.VariationParam12Dial      -> set_param_map(VARIATION, 0x71);
	m_ui.VariationParam13Dial      -> set_param_map(VARIATION, 0x72);
	m_ui.VariationParam14Dial      -> set_param_map(VARIATION, 0x73);
	m_ui.VariationParam15Dial      -> set_param_map(VARIATION, 0x74);
	m_ui.VariationParam16Dial      -> set_param_map(VARIATION, 0x75);

	// MULTIPART...
	m_ui.MultipartCombo->clear();
	for (int iPart = 0; iPart < 16; ++iPart)
		m_ui.MultipartCombo->addItem(tr("Part %1").arg(iPart + 1));

	QObject::connect(m_ui.MultipartCombo,
		SIGNAL(activated(int)),
		SLOT(multipartComboActivated(int)));

	// Instrument model/view combo-box...
	QTreeWidget *pMultipartVoiceListView = new QTreeWidget();
	pMultipartVoiceListView->header()->hide();
	pMultipartVoiceListView->setRootIsDecorated(true);
	pMultipartVoiceListView->setAllColumnsShowFocus(true);
	pMultipartVoiceListView->setUniformRowHeights(true);
	m_ui.MultipartVoiceCombo->setModel(pMultipartVoiceListView->model());
	m_ui.MultipartVoiceCombo->setView(pMultipartVoiceListView);
	m_ui.MultipartVoiceCombo->clear();

	QList<QTreeWidgetItem *> items;
	for (unsigned short i = 0; i < XGInstrument::count(); ++i) {
		XGInstrument instr(i);
		QTreeWidgetItem *pInstrumentItem
			= new QTreeWidgetItem(pMultipartVoiceListView);
		pInstrumentItem->setFlags(/*Qt::ItemIsUserCheckable | */Qt::ItemIsEnabled);
		pInstrumentItem->setText(0, instr.name());
		for (unsigned short j = 0; j < instr.size(); ++j) {
			XGNormalVoice voice(&instr, j);
			QTreeWidgetItem *pVoiceItem = new QTreeWidgetItem(pInstrumentItem);
			pVoiceItem->setText(0, voice.name());
		}
		items.append(pInstrumentItem);
	}
	pMultipartVoiceListView->addTopLevelItems(items);

	QObject::connect(m_ui.MultipartVoiceCombo,
		SIGNAL(activated(int)),
		SLOT(multipartVoiceComboActivated(int)));

	QObject::connect(m_ui.MultipartBankMSBDial,
		SIGNAL(valueChanged(unsigned short)),
		SLOT(multipartVoiceChanged()));
	QObject::connect(m_ui.MultipartBankLSBDial,
		SIGNAL(valueChanged(unsigned short)),
		SLOT(multipartVoiceChanged()));
	QObject::connect(m_ui.MultipartProgramDial,
		SIGNAL(valueChanged(unsigned short)),
		SLOT(multipartVoiceChanged()));

	QObject::connect(m_ui.MultipartResetButton,
		SIGNAL(clicked()),
		SLOT(multipartResetButtonClicked()));

	// AmpEg...
	QObject::connect(
		m_ui.MultipartAmpEg, SIGNAL(attackChanged(unsigned short)),
		m_ui.MultipartAttackDial, SLOT(setValue(unsigned short)));
	QObject::connect(
		m_ui.MultipartAttackDial, SIGNAL(valueChanged(unsigned short)),
		m_ui.MultipartAmpEg, SLOT(setAttack(unsigned short)));
	QObject::connect(
		m_ui.MultipartAmpEg, SIGNAL(decayChanged(unsigned short)),
		m_ui.MultipartDecayDial, SLOT(setValue(unsigned short)));
	QObject::connect(
		m_ui.MultipartDecayDial, SIGNAL(valueChanged(unsigned short)),
		m_ui.MultipartAmpEg, SLOT(setDecay(unsigned short)));
	QObject::connect(
		m_ui.MultipartAmpEg, SIGNAL(releaseChanged(unsigned short)),
		m_ui.MultipartReleaseDial, SLOT(setValue(unsigned short)));
	QObject::connect(
		m_ui.MultipartReleaseDial, SIGNAL(valueChanged(unsigned short)),
		m_ui.MultipartAmpEg, SLOT(setRelease(unsigned short)));

	// Filter...
	QObject::connect(
		m_ui.MultipartFilter, SIGNAL(cutoffChanged(unsigned short)),
		m_ui.MultipartCutoffDial, SLOT(setValue(unsigned short)));
	QObject::connect(
		m_ui.MultipartCutoffDial, SIGNAL(valueChanged(unsigned short)),
		m_ui.MultipartFilter, SLOT(setCutoff(unsigned short)));
	QObject::connect(
		m_ui.MultipartFilter, SIGNAL(resonanceChanged(unsigned short)),
		m_ui.MultipartResonanceDial, SLOT(setValue(unsigned short)));
	QObject::connect(
		m_ui.MultipartResonanceDial, SIGNAL(valueChanged(unsigned short)),
		m_ui.MultipartFilter, SLOT(setResonance(unsigned short)));

	// Pitch...
	QObject::connect(
		m_ui.MultipartPitch, SIGNAL(attackTimeChanged(unsigned short)),
		m_ui.MultipartAttackTimeDial, SLOT(setValue(unsigned short)));
	QObject::connect(
		m_ui.MultipartAttackTimeDial, SIGNAL(valueChanged(unsigned short)),
		m_ui.MultipartPitch, SLOT(setAttackTime(unsigned short)));
	QObject::connect(
		m_ui.MultipartPitch, SIGNAL(attackLevelChanged(unsigned short)),
		m_ui.MultipartAttackLevelDial, SLOT(setValue(unsigned short)));
	QObject::connect(
		m_ui.MultipartAttackLevelDial, SIGNAL(valueChanged(unsigned short)),
		m_ui.MultipartPitch, SLOT(setAttackLevel(unsigned short)));
	QObject::connect(
		m_ui.MultipartPitch, SIGNAL(releaseTimeChanged(unsigned short)),
		m_ui.MultipartReleaseTimeDial, SLOT(setValue(unsigned short)));
	QObject::connect(
		m_ui.MultipartReleaseTimeDial, SIGNAL(valueChanged(unsigned short)),
		m_ui.MultipartPitch, SLOT(setReleaseTime(unsigned short)));
	QObject::connect(
		m_ui.MultipartPitch, SIGNAL(releaseLevelChanged(unsigned short)),
		m_ui.MultipartReleaseLevelDial, SLOT(setValue(unsigned short)));
	QObject::connect(
		m_ui.MultipartReleaseLevelDial, SIGNAL(valueChanged(unsigned short)),
		m_ui.MultipartPitch, SLOT(setReleaseLevel(unsigned short)));

	// Vibrato...
	QObject::connect(
		m_ui.MultipartVibra, SIGNAL(delayChanged(unsigned short)),
		m_ui.MultipartDelayDial, SLOT(setValue(unsigned short)));
	QObject::connect(
		m_ui.MultipartDelayDial, SIGNAL(valueChanged(unsigned short)),
		m_ui.MultipartVibra, SLOT(setDelay(unsigned short)));
	QObject::connect(
		m_ui.MultipartVibra, SIGNAL(rateChanged(unsigned short)),
		m_ui.MultipartRateDial, SLOT(setValue(unsigned short)));
	QObject::connect(
		m_ui.MultipartRateDial, SIGNAL(valueChanged(unsigned short)),
		m_ui.MultipartVibra, SLOT(setRate(unsigned short)));
	QObject::connect(
		m_ui.MultipartVibra, SIGNAL(depthChanged(unsigned short)),
		m_ui.MultipartDepthDial, SLOT(setValue(unsigned short)));
	QObject::connect(
		m_ui.MultipartDepthDial, SIGNAL(valueChanged(unsigned short)),
		m_ui.MultipartVibra, SLOT(setDepth(unsigned short)));

	m_ui.MultipartElementDial      -> set_param_map(MULTIPART, 0x00);
	m_ui.MultipartBankMSBDial      -> set_param_map(MULTIPART, 0x01);
	m_ui.MultipartBankLSBDial      -> set_param_map(MULTIPART, 0x02);
	m_ui.MultipartProgramDial      -> set_param_map(MULTIPART, 0x03);
	m_ui.MultipartChannelDial      -> set_param_map(MULTIPART, 0x04);
	m_ui.MultipartPolyModeDial     -> set_param_map(MULTIPART, 0x05);
	m_ui.MultipartKeyAssignDial    -> set_param_map(MULTIPART, 0x06);
	m_ui.MultipartPartModeDial     -> set_param_map(MULTIPART, 0x07);
	m_ui.MultipartNoteShiftDial    -> set_param_map(MULTIPART, 0x08);
	m_ui.MultipartDetuneDial       -> set_param_map(MULTIPART, 0x09);
	m_ui.MultipartVolumeDial       -> set_param_map(MULTIPART, 0x0b);
	m_ui.MultipartVelDepthDial     -> set_param_map(MULTIPART, 0x0c);
	m_ui.MultipartVelOffsetDial    -> set_param_map(MULTIPART, 0x0d);
	m_ui.MultipartPanDial          -> set_param_map(MULTIPART, 0x0e);
	m_ui.MultipartNoteLowDial      -> set_param_map(MULTIPART, 0x0f);
	m_ui.MultipartNoteHighDial     -> set_param_map(MULTIPART, 0x10);
	m_ui.MultipartDryWetDial       -> set_param_map(MULTIPART, 0x11);
	m_ui.MultipartChorusDial       -> set_param_map(MULTIPART, 0x12);
	m_ui.MultipartReverbDial       -> set_param_map(MULTIPART, 0x13);
	m_ui.MultipartVariationDial    -> set_param_map(MULTIPART, 0x14);
	m_ui.MultipartRateDial         -> set_param_map(MULTIPART, 0x15);
	m_ui.MultipartDepthDial        -> set_param_map(MULTIPART, 0x16);
	m_ui.MultipartDelayDial        -> set_param_map(MULTIPART, 0x17);
	m_ui.MultipartCutoffDial       -> set_param_map(MULTIPART, 0x18);
	m_ui.MultipartResonanceDial    -> set_param_map(MULTIPART, 0x19);
	m_ui.MultipartAttackDial       -> set_param_map(MULTIPART, 0x1a);
	m_ui.MultipartDecayDial        -> set_param_map(MULTIPART, 0x1b);
	m_ui.MultipartReleaseDial      -> set_param_map(MULTIPART, 0x1c);
	m_ui.MultipartWheelPitchDial   -> set_param_map(MULTIPART, 0x1d);
	m_ui.MultipartWheelFilterDial  -> set_param_map(MULTIPART, 0x1e);
	m_ui.MultipartWheelAmplDial    -> set_param_map(MULTIPART, 0x1f);
	m_ui.MultipartWheelLFOPmodDial -> set_param_map(MULTIPART, 0x20);
	m_ui.MultipartWheelLFOFmodDial -> set_param_map(MULTIPART, 0x21);
	m_ui.MultipartWheelLFOAmodDial -> set_param_map(MULTIPART, 0x22);
	m_ui.MultipartBendPitchDial    -> set_param_map(MULTIPART, 0x23);
	m_ui.MultipartBendFilterDial   -> set_param_map(MULTIPART, 0x24);
	m_ui.MultipartBendAmplDial     -> set_param_map(MULTIPART, 0x25);
	m_ui.MultipartBendLFOPmodDial  -> set_param_map(MULTIPART, 0x26);
	m_ui.MultipartBendLFOFmodDial  -> set_param_map(MULTIPART, 0x27);
	m_ui.MultipartBendLFOAmodDial  -> set_param_map(MULTIPART, 0x28);
	m_ui.MultipartPBCheck          -> set_param_map(MULTIPART, 0x30);
	m_ui.MultipartCATCheck         -> set_param_map(MULTIPART, 0x31);
	m_ui.MultipartPCCheck          -> set_param_map(MULTIPART, 0x32);
	m_ui.MultipartCCCheck          -> set_param_map(MULTIPART, 0x33);
	m_ui.MultipartPATCheck         -> set_param_map(MULTIPART, 0x34);
	m_ui.MultipartNoteCheck        -> set_param_map(MULTIPART, 0x35);
	m_ui.MultipartRPNCheck         -> set_param_map(MULTIPART, 0x36);
	m_ui.MultipartNRPNCheck        -> set_param_map(MULTIPART, 0x37);
	m_ui.MultipartModCheck         -> set_param_map(MULTIPART, 0x38);
	m_ui.MultipartVolCheck         -> set_param_map(MULTIPART, 0x39);
	m_ui.MultipartPanCheck         -> set_param_map(MULTIPART, 0x3a);
	m_ui.MultipartExprCheck        -> set_param_map(MULTIPART, 0x3b);
	m_ui.MultipartHold1Check       -> set_param_map(MULTIPART, 0x3c);
	m_ui.MultipartPortaCheck       -> set_param_map(MULTIPART, 0x3d);
	m_ui.MultipartSostCheck        -> set_param_map(MULTIPART, 0x3e);
	m_ui.MultipartPedalCheck       -> set_param_map(MULTIPART, 0x3f);
	m_ui.MultipartBankCheck        -> set_param_map(MULTIPART, 0x40);
	m_ui.MultipartTuningC_Dial     -> set_param_map(MULTIPART, 0x41);
	m_ui.MultipartTuningCsDial     -> set_param_map(MULTIPART, 0x42);
	m_ui.MultipartTuningD_Dial     -> set_param_map(MULTIPART, 0x43);
	m_ui.MultipartTuningDsDial     -> set_param_map(MULTIPART, 0x44);
	m_ui.MultipartTuningE_Dial     -> set_param_map(MULTIPART, 0x45);
	m_ui.MultipartTuningF_Dial     -> set_param_map(MULTIPART, 0x46);
	m_ui.MultipartTuningFsDial     -> set_param_map(MULTIPART, 0x47);
	m_ui.MultipartTuningG_Dial     -> set_param_map(MULTIPART, 0x48);
	m_ui.MultipartTuningGsDial     -> set_param_map(MULTIPART, 0x49);
	m_ui.MultipartTuningA_Dial     -> set_param_map(MULTIPART, 0x4a);
	m_ui.MultipartTuningAsDial     -> set_param_map(MULTIPART, 0x4b);
	m_ui.MultipartTuningB_Dial     -> set_param_map(MULTIPART, 0x4c);
	m_ui.MultipartCATPitchDial     -> set_param_map(MULTIPART, 0x4d);
	m_ui.MultipartCATFilterDial    -> set_param_map(MULTIPART, 0x4e);
	m_ui.MultipartCATAmplDial      -> set_param_map(MULTIPART, 0x4f);
	m_ui.MultipartCATLFOPmodDial   -> set_param_map(MULTIPART, 0x50);
	m_ui.MultipartCATLFOFmodDial   -> set_param_map(MULTIPART, 0x51);
	m_ui.MultipartCATLFOAmodDial   -> set_param_map(MULTIPART, 0x52);
	m_ui.MultipartPATPitchDial     -> set_param_map(MULTIPART, 0x53);
	m_ui.MultipartPATFilterDial    -> set_param_map(MULTIPART, 0x54);
	m_ui.MultipartPATAmplDial      -> set_param_map(MULTIPART, 0x55);
	m_ui.MultipartPATLFOPmodDial   -> set_param_map(MULTIPART, 0x56);
	m_ui.MultipartPATLFOFmodDial   -> set_param_map(MULTIPART, 0x57);
	m_ui.MultipartPATLFOAmodDial   -> set_param_map(MULTIPART, 0x58);
	m_ui.MultipartAC1ControlDial   -> set_param_map(MULTIPART, 0x59);
	m_ui.MultipartAC1PitchDial     -> set_param_map(MULTIPART, 0x5a);
	m_ui.MultipartAC1FilterDial    -> set_param_map(MULTIPART, 0x5b);
	m_ui.MultipartAC1AmplDial      -> set_param_map(MULTIPART, 0x5c);
	m_ui.MultipartAC1LFOPmodDial   -> set_param_map(MULTIPART, 0x5d);
	m_ui.MultipartAC1LFOFmodDial   -> set_param_map(MULTIPART, 0x5e);
	m_ui.MultipartAC1LFOAmodDial   -> set_param_map(MULTIPART, 0x5f);
	m_ui.MultipartAC2ControlDial   -> set_param_map(MULTIPART, 0x60);
	m_ui.MultipartAC2PitchDial     -> set_param_map(MULTIPART, 0x61);
	m_ui.MultipartAC2FilterDial    -> set_param_map(MULTIPART, 0x62);
	m_ui.MultipartAC2AmplDial      -> set_param_map(MULTIPART, 0x63);
	m_ui.MultipartAC2LFOPmodDial   -> set_param_map(MULTIPART, 0x64);
	m_ui.MultipartAC2LFOFmodDial   -> set_param_map(MULTIPART, 0x65);
	m_ui.MultipartAC2LFOAmodDial   -> set_param_map(MULTIPART, 0x66);
	m_ui.MultipartPortamentoDial   -> set_param_map(MULTIPART, 0x67);
	m_ui.MultipartPortaTimeDial    -> set_param_map(MULTIPART, 0x68);
	m_ui.MultipartAttackLevelDial  -> set_param_map(MULTIPART, 0x69);
	m_ui.MultipartAttackTimeDial   -> set_param_map(MULTIPART, 0x6a);
	m_ui.MultipartReleaseLevelDial -> set_param_map(MULTIPART, 0x6b);
	m_ui.MultipartReleaseTimeDial  -> set_param_map(MULTIPART, 0x6c);
	m_ui.MultipartVelLowDial       -> set_param_map(MULTIPART, 0x6d);
	m_ui.MultipartVelHighDial      -> set_param_map(MULTIPART, 0x6e);

	// DRUMSETUP...
	m_ui.DrumsetupCombo->clear();
	for (int iDrumset = 0; iDrumset < 2; ++iDrumset)
		m_ui.DrumsetupCombo->addItem(tr("Drums %1").arg(iDrumset + 1));

	m_ui.DrumsetupVoiceCombo->clear();
	for (unsigned short k = 0; k < XGDrumKit::count(); ++k) {
		XGDrumKit drumkit(k);
		m_ui.DrumsetupVoiceCombo->addItem(drumkit.name());
	}

	drumsetupVoiceComboActivated(0);
	
	QObject::connect(m_ui.DrumsetupCombo,
		SIGNAL(activated(int)),
		SLOT(drumsetupComboActivated(int)));

	QObject::connect(m_ui.DrumsetupVoiceCombo,
		SIGNAL(activated(int)),
		SLOT(drumsetupVoiceComboActivated(int)));

	QObject::connect(m_ui.DrumsetupNoteCombo,
		SIGNAL(activated(int)),
		SLOT(drumsetupNoteComboActivated(int)));

	QObject::connect(m_ui.DrumsetupResetButton,
		SIGNAL(clicked()),
		SLOT(drumsetupResetButtonClicked()));

	// Filter...
	QObject::connect(
		m_ui.DrumsetupFilter, SIGNAL(cutoffChanged(unsigned short)),
		m_ui.DrumsetupCutoffDial, SLOT(setValue(unsigned short)));
	QObject::connect(
		m_ui.DrumsetupCutoffDial, SIGNAL(valueChanged(unsigned short)),
		m_ui.DrumsetupFilter, SLOT(setCutoff(unsigned short)));
	QObject::connect(
		m_ui.DrumsetupFilter, SIGNAL(resonanceChanged(unsigned short)),
		m_ui.DrumsetupResonanceDial, SLOT(setValue(unsigned short)));
	QObject::connect(
		m_ui.DrumsetupResonanceDial, SIGNAL(valueChanged(unsigned short)),
		m_ui.DrumsetupFilter, SLOT(setResonance(unsigned short)));

	// AmpEg...
	QObject::connect(
		m_ui.DrumsetupAmpEg, SIGNAL(attackChanged(unsigned short)),
		m_ui.DrumsetupAttackDial, SLOT(setValue(unsigned short)));
	QObject::connect(
		m_ui.DrumsetupAttackDial, SIGNAL(valueChanged(unsigned short)),
		m_ui.DrumsetupAmpEg, SLOT(setAttack(unsigned short)));
	QObject::connect(
		m_ui.DrumsetupAmpEg, SIGNAL(decay1Changed(unsigned short)),
		m_ui.DrumsetupDecay1Dial, SLOT(setValue(unsigned short)));
	QObject::connect(
		m_ui.DrumsetupDecay1Dial, SIGNAL(valueChanged(unsigned short)),
		m_ui.DrumsetupAmpEg, SLOT(setDecay1(unsigned short)));
	QObject::connect(
		m_ui.DrumsetupAmpEg, SIGNAL(decay2Changed(unsigned short)),
		m_ui.DrumsetupDecay2Dial, SLOT(setValue(unsigned short)));
	QObject::connect(
		m_ui.DrumsetupDecay2Dial, SIGNAL(valueChanged(unsigned short)),
		m_ui.DrumsetupAmpEg, SLOT(setDecay2(unsigned short)));

	m_ui.DrumsetupCoarseDial       -> set_param_map(DRUMSETUP, 0x00);
	m_ui.DrumsetupFineDial         -> set_param_map(DRUMSETUP, 0x01);
	m_ui.DrumsetupLevelDial        -> set_param_map(DRUMSETUP, 0x02);
	m_ui.DrumsetupGroupDial        -> set_param_map(DRUMSETUP, 0x03);
	m_ui.DrumsetupPanDial          -> set_param_map(DRUMSETUP, 0x04);
	m_ui.DrumsetupReverbDial       -> set_param_map(DRUMSETUP, 0x05);
	m_ui.DrumsetupChorusDial       -> set_param_map(DRUMSETUP, 0x06);
	m_ui.DrumsetupVariationDial    -> set_param_map(DRUMSETUP, 0x07);
	m_ui.DrumsetupKeyAssignDial    -> set_param_map(DRUMSETUP, 0x08);
	m_ui.DrumsetupNoteOffCheck     -> set_param_map(DRUMSETUP, 0x09);
	m_ui.DrumsetupNoteOnCheck      -> set_param_map(DRUMSETUP, 0x0a);
	m_ui.DrumsetupCutoffDial       -> set_param_map(DRUMSETUP, 0x0b);
	m_ui.DrumsetupResonanceDial    -> set_param_map(DRUMSETUP, 0x0c);
	m_ui.DrumsetupAttackDial       -> set_param_map(DRUMSETUP, 0x0d);
	m_ui.DrumsetupDecay1Dial       -> set_param_map(DRUMSETUP, 0x0e);
	m_ui.DrumsetupDecay2Dial       -> set_param_map(DRUMSETUP, 0x0f);

	// Is any session pending to be loaded?
	if (!m_pOptions->sSessionFile.isEmpty()) {
		// Just load the prabably startup session...
		if (loadSessionFile(m_pOptions->sSessionFile))
			m_pOptions->sSessionFile.clear();
	} else {
		// Open up with a new empty session...
		newSession();
	}

	// Make it ready :-)
	statusBar()->showMessage(tr("Ready"), 3000);
}


// Window close event handlers.
bool qxgeditMainForm::queryClose (void)
{
	bool bQueryClose = closeSession();

	// Try to save current general state...
	if (m_pOptions) {
		// Try to save current positioning.
		if (bQueryClose) {
			// Save decorations state.
			m_pOptions->bMenubar = m_ui.MenuBar->isVisible();
			m_pOptions->bStatusbar = statusBar()->isVisible();
			m_pOptions->bToolbar = m_ui.ToolBar->isVisible();
			// Save main windows state.
			m_pOptions->saveWidgetGeometry(this);
		}
	}

	return bQueryClose;
}


void qxgeditMainForm::closeEvent ( QCloseEvent *pCloseEvent )
{
	// Let's be sure about that...
	if (queryClose()) {
		pCloseEvent->accept();
		QApplication::quit();
	} else {
		pCloseEvent->ignore();
	}
}


// Window drag-n-drop event handlers.
void qxgeditMainForm::dragEnterEvent ( QDragEnterEvent* pDragEnterEvent )
{
	// Accept external drags only...
	if (pDragEnterEvent->source() == NULL
		&& pDragEnterEvent->mimeData()->hasUrls()) {
		pDragEnterEvent->accept();
	} else {
		pDragEnterEvent->ignore();
	}
}


void qxgeditMainForm::dropEvent ( QDropEvent* pDropEvent )
{
	// Accept externally originated drops only...
	if (pDropEvent->source())
		return;

	const QMimeData *pMimeData = pDropEvent->mimeData();
	if (pMimeData->hasUrls()) {
		QString sFilename
			= QDir::toNativeSeparators(pMimeData->urls().first().toLocalFile());
		// Close current session and try to load the new one...
		if (!sFilename.isEmpty() && closeSession())
			loadSessionFile(sFilename);
	}
}


// Custom event handler.
void qxgeditMainForm::customEvent ( QEvent *pEvent )
{
	switch (pEvent->type()) {
	case QXGEDIT_SYSEX_EVENT:
		sysexEvent(static_cast<qxgeditMidiSysexEvent *> (pEvent));
		// Fall thru...
	default:
		break;
	}
}


// SYSEX Event handler.
bool qxgeditMainForm::sysexEvent ( qxgeditMidiSysexEvent *pSysexEvent )
{
	return sysexData(pSysexEvent->data(), pSysexEvent->len());
}

bool qxgeditMainForm::sysexData ( unsigned char *data, unsigned short len )
{
	bool ret = false;
	unsigned short i;

	if (data[0] == 0xf0 && data[len - 1] == 0xf7) {
		 // SysEx (actually)...
		if (data[1] == 0x43) {
			// Yamaha ID...
			unsigned char mode  = (data[2] & 0x70);
		//	unsigned char devno = (data[2] & 0x0f);
			if (data[3] == 0x4c) {
				// XG Model ID...
				if (mode == 0x00) {
					// Native Bulk Dump...
					unsigned short size = (data[4] << 7) + data[5];
					unsigned char cksum = 0;
					for (i = 0; i < size + 5; ++i) {
						cksum += data[4 + i];
						cksum &= 0x7f;
					}
					if (data[9 + size] == 0x80 - cksum) {
						unsigned char high = data[6];
						unsigned char mid  = data[7];
						unsigned char low  = data[8];
						for (i = 0; i < size; ++i) {
							// Parameter Change...
							unsigned short n
								= sysexXGParam(high, mid, low + i, &data[9 + i]);
							if (n < 1)
								break;
							if (n > 1)
								i += (n - 1);
						}
						ret = (i == (size - 6));
					}
				}
				else
				if (mode == 0x10) {
					// Parameter Change...
					unsigned char high = data[4];
					unsigned char mid  = data[5];
					unsigned char low  = data[6];
					ret = (sysexXGParam(high, mid, low, &data[7]) > 0);
				}
			}
		}
	}
	
	return ret;
}

unsigned short qxgeditMainForm::sysexXGParam (
	unsigned char high, unsigned char mid, unsigned char low,
	unsigned char *data )
{
	return m_pParamMap->set_param_data(high, mid, low, data);
}


//-------------------------------------------------------------------------
// qxgeditMainForm -- Session file stuff.

// Format the displayable session filename.
QString qxgeditMainForm::sessionName ( const QString& sFilename )
{
	bool bCompletePath = (m_pOptions && m_pOptions->bCompletePath);
	QString sSessionName = sFilename;
	if (sSessionName.isEmpty())
		sSessionName = tr("Untitled") + QString::number(m_iUntitled);
	else if (!bCompletePath)
		sSessionName = QFileInfo(sSessionName).baseName();
	return sSessionName;
}


// Create a new session file from scratch.
bool qxgeditMainForm::newSession (void)
{
	// Check if we can do it.
	if (!closeSession())
		return false;

	// Reset it all.
	if (m_pParamMap)
		m_pParamMap->reset_all();

	// Ok, increment untitled count.
	m_iUntitled++;

	// Stabilize form.
	m_sFilename.clear();
	m_iDirtyCount = 0;

	stabilizeForm();

	return true;
}


// Open an existing sampler session.
bool qxgeditMainForm::openSession (void)
{
	if (m_pOptions == NULL)
		return false;

	// Ask for the filename to open...
	QString sFilename;

	QString sExt("syx");
	const QString& sTitle  = tr("Open Session") + " - " QXGEDIT_TITLE;
	const QString& sFilter = tr("Session files (*.%1)").arg(sExt);
#if QT_VERSION < 0x040400
	sFilename = QFileDialog::getOpenFileName(this,
		sTitle, m_pOptions->sSessionDir, sFilter);
#else
	// Construct open-file session/template dialog...
	QFileDialog fileDialog(this,
		sTitle, m_pOptions->sSessionDir, sFilter);
	// Set proper open-file modes...
	fileDialog.setAcceptMode(QFileDialog::AcceptOpen);
	fileDialog.setFileMode(QFileDialog::ExistingFile);
	fileDialog.setHistory(m_pOptions->recentFiles);
	fileDialog.setDefaultSuffix(sExt);
	// Stuff sidebar...
	QList<QUrl> urls(fileDialog.sidebarUrls());
	urls.append(QUrl::fromLocalFile(m_pOptions->sSessionDir));
	fileDialog.setSidebarUrls(urls);
	// Show dialog...
	if (!fileDialog.exec())
		return false;
	// Have the open-file name...
	sFilename = fileDialog.selectedFiles().first();
#endif

	// Have we cancelled?
	if (sFilename.isEmpty())
		return false;

	// Check if we're going to discard safely the current one...
	if (!closeSession())
		return false;

	// Load it right away.
	return loadSessionFile(sFilename);
}


// Save current sampler session with another name.
bool qxgeditMainForm::saveSession ( bool bPrompt )
{
	if (m_pOptions == NULL)
		return false;

	// Suggest a filename, if there's none...
	QString sFilename = m_sFilename;
	if (sFilename.isEmpty())
		bPrompt = true;

	// Ask for the file to save...
	if (bPrompt) {
		// Prompt the guy...
		QString sExt("syx");
		const QString& sTitle  = tr("Save Session") + " - " QXGEDIT_TITLE;
		const QString& sFilter = tr("Session files (*.%1)").arg(sExt);
	#if QT_VERSION < 0x040400
		sFilename = QFileDialog::getSaveFileName(this,
			sTitle, sFilename, sFilter);
	#else
		// Construct save-file session/template dialog...
		QFileDialog fileDialog(this,
			sTitle, sFilename, sFilter);
		// Set proper save-file modes...
		fileDialog.setAcceptMode(QFileDialog::AcceptSave);
		fileDialog.setFileMode(QFileDialog::AnyFile);
		fileDialog.setHistory(m_pOptions->recentFiles);
		fileDialog.setDefaultSuffix(sExt);
		// Stuff sidebar...
		QList<QUrl> urls(fileDialog.sidebarUrls());
		urls.append(QUrl::fromLocalFile(m_pOptions->sSessionDir));
		fileDialog.setSidebarUrls(urls);
		// Show save-file dialog...
		if (!fileDialog.exec())
			return false;
		// Have the save-file name...
		sFilename = fileDialog.selectedFiles().first();
	#endif
		// Have we cancelled it?
		if (sFilename.isEmpty())
			return false;
		// Enforce extension...
		if (QFileInfo(sFilename).suffix() != sExt) {
			sFilename += '.' + sExt;
			// Check if already exists...
			if (sFilename != m_sFilename && QFileInfo(sFilename).exists()) {
				if (QMessageBox::warning(this,
					tr("Warning") + " - " QXGEDIT_TITLE,
					tr("The file already exists:\n\n"
					"\"%1\"\n\n"
					"Do you want to replace it?")
					.arg(sessionName(sFilename)),
					QMessageBox::Yes | QMessageBox::No) == QMessageBox::No)
					return false;
			}
		}
	}

	// Save it right away.
	return saveSessionFile(sFilename);
}


// Close current session.
bool qxgeditMainForm::closeSession (void)
{
	bool bClose = true;

	// Are we dirty enough to prompt it?
	if (m_iDirtyCount > 0) {
		switch (QMessageBox::warning(this,
			tr("Warning") + " - " QXGEDIT_TITLE,
			tr("The current session has been changed:\n\n"
			"\"%1\"\n\n"
			"Do you want to save the changes?")
			.arg(sessionName(m_sFilename)),
			QMessageBox::Save |
			QMessageBox::Discard |
			QMessageBox::Cancel)) {
		case QMessageBox::Save:
			bClose = saveSession(false);
			// Fall thru....
		case QMessageBox::Discard:
			break;
		default:    // Cancel.
			bClose = false;
			break;
		}
	}

	// If we may close it, dot it.
	if (bClose) {
		// TODO: Actually close session...
		// We're now clean, for sure.
		m_iDirtyCount = 0;
	}

	return bClose;
}


// Load a session from specific file path.
bool qxgeditMainForm::loadSessionFile ( const QString& sFilename )
{
	// open the source file...
	QFile file(sFilename);
	if (!file.open(QIODevice::ReadOnly))
		return false;

	// Tell the world we'll take some time...
	QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

	int iSysex = 0;
	unsigned short iBuff = 0;
	unsigned char *pBuff = NULL;
	unsigned short i = 0;

	// Read the file....
	while (!file.atEnd()) {
		// (Re)allocate buffer...
		if (i >= iBuff) {
			unsigned char *pTemp = pBuff;
			iBuff += 1024;
			pBuff  = new unsigned char [iBuff];
			if (pTemp) {
				::memcpy(pBuff, pTemp, i);
				delete [] pTemp;
			}
		}
		// Read the next chunk...
		unsigned short iRead = file.read((char *) pBuff + i, iBuff - i) + i;
		while (i < iRead) {
			if (pBuff[i++] == 0xf7) {
				sysexData(pBuff, i);
				if (i < iRead) {
					::memmove(pBuff, pBuff + i, iRead -= i);
					i = 0;
				}
			}
		}
	}

	// Cleanup...
	if (pBuff)
		delete [] pBuff;	
	file.close();

	// We're formerly done.
	QApplication::restoreOverrideCursor();

	// Reset the official session title.
	m_sFilename = sFilename;
	updateRecentFiles(sFilename);
	m_iDirtyCount = 0;

	// Save as default session directory.
	if (m_pOptions)
		m_pOptions->sSessionDir = QFileInfo(sFilename).absolutePath();

	stabilizeForm();

	return (iSysex > 0);
}


// Save current session to specific file path.
bool qxgeditMainForm::saveSessionFile (	const QString& sFilename )
{
	// Open the target file...
	QFile file(sFilename);
	if (!file.open(QIODevice::ReadWrite | QIODevice::Truncate))
		return false;

	// Tell the world we'll take some time...
	QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

	XGParamMasterMap::const_iterator iter = m_pParamMap->constBegin();
	for (; iter != m_pParamMap->constEnd(); ++iter) {
		XGParam *pParam = iter.value();
		if (pParam->value() == pParam->def())
			continue;
		unsigned char aSysex[]  = { 0xf0, 0x43, 0x10, 0x4c,
			0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 0xf7 };
		unsigned short iSysex = 8 + pParam->size();
		aSysex[4] = pParam->high();
		aSysex[5] = pParam->mid();
		aSysex[6] = pParam->low();
		pParam->set_data_value(&aSysex[7], pParam->value());
		aSysex[iSysex - 1] = 0xf7;
		file.write((const char *) aSysex, iSysex);
	}

	file.close();

	// We're formerly done.
	QApplication::restoreOverrideCursor();

	// Reset the official session title.
	m_sFilename = sFilename;
	updateRecentFiles(sFilename);
	m_iDirtyCount = 0;

	// Save as default session directory.
	if (m_pOptions)
		m_pOptions->sSessionDir = QFileInfo(sFilename).absolutePath();

	stabilizeForm();

	return true;
}


//-------------------------------------------------------------------------
// qxgeditMainForm -- File Action slots.

// Create a new sampler session.
void qxgeditMainForm::fileNew (void)
{
	// Of course we'll start clean new.
	newSession();
}


// Open an existing sampler session.
void qxgeditMainForm::fileOpen (void)
{
	// Open it right away.
	openSession();
}


// Open a recent file session.
void qxgeditMainForm::fileOpenRecent (void)
{
	// Retrive filename index from action data...
	QAction *pAction = qobject_cast<QAction *> (sender());
	if (pAction && m_pOptions) {
		int iIndex = pAction->data().toInt();
		if (iIndex >= 0 && iIndex < m_pOptions->recentFiles.count()) {
			QString sFilename = m_pOptions->recentFiles[iIndex];
			// Check if we can safely close the current session...
			if (!sFilename.isEmpty() && closeSession())
				loadSessionFile(sFilename);
		}
	}
}


// Save current sampler session.
void qxgeditMainForm::fileSave (void)
{
	// Save it right away.
	saveSession(false);
}


// Save current sampler session with another name.
void qxgeditMainForm::fileSaveAs (void)
{
	// Save it right away, maybe with another name.
	saveSession(true);
}


// Exit application program.
void qxgeditMainForm::fileExit (void)
{
	// Go for close the whole thing.
	close();
}


//-------------------------------------------------------------------------
// qxgeditMainForm -- View Action slots.

// Show/hide the main program window menubar.
void qxgeditMainForm::viewMenubar ( bool bOn )
{
	m_ui.MenuBar->setVisible(bOn);
}


// Show/hide the main program window statusbar.
void qxgeditMainForm::viewStatusbar ( bool bOn )
{
	statusBar()->setVisible(bOn);
}


// Show/hide the file-toolbar.
void qxgeditMainForm::viewToolbar ( bool bOn )
{
	m_ui.ToolBar->setVisible(bOn);
}


// Show options dialog.
void qxgeditMainForm::viewOptions (void)
{
	if (m_pOptions == NULL)
		return;

	// Load the current setup settings.
	bool bOldCompletePath   = m_pOptions->bCompletePath;
	int  iOldMaxRecentFiles = m_pOptions->iMaxRecentFiles;
	int  iOldBaseFontSize   = m_pOptions->iBaseFontSize;
	// Load the current setup settings.
	qxgeditOptionsForm optionsForm(this);
	optionsForm.setOptions(m_pOptions);
	if (optionsForm.exec()) {
		enum { RestartSession = 1, RestartProgram = 2, RestartAny = 3 };
		// Check wheather something immediate has changed.
		int iNeedRestart = 0;
		if (iOldBaseFontSize != m_pOptions->iBaseFontSize)
			iNeedRestart |= RestartProgram;
		if (( bOldCompletePath && !m_pOptions->bCompletePath) ||
			(!bOldCompletePath &&  m_pOptions->bCompletePath) ||
			(iOldMaxRecentFiles != m_pOptions->iMaxRecentFiles))
			updateRecentFilesMenu();
		// Warn if something will be only effective on next time.
		if (iNeedRestart & RestartAny) {
			QString sNeedRestart;
			if (iNeedRestart & RestartSession)
				sNeedRestart += tr("session");
			if (iNeedRestart & RestartProgram) {
				if (!sNeedRestart.isEmpty())
					sNeedRestart += tr(" or ");
				sNeedRestart += tr("program");
			}
			// Show restart needed message...
			QMessageBox::information(this,
				tr("Information") + " - " QXGEDIT_TITLE,
				tr("Some settings may be only effective\n"
				"next time you start this %1.")
				.arg(sNeedRestart));
		}
	}

	// This makes it.
	stabilizeForm();
}


//-------------------------------------------------------------------------
// qxgeditMainForm -- Help Action slots.

// Show information about application program.
void qxgeditMainForm::helpAbout (void)
{
	// Stuff the about box text...
	QString sText = "<p>\n";
	sText += "<b>" QXGEDIT_TITLE " - " + tr(QXGEDIT_SUBTITLE) + "</b><br />\n";
	sText += "<br />\n";
	sText += tr("Version") + ": <b>" QXGEDIT_VERSION "</b><br />\n";
	sText += "<small>" + tr("Build") + ": " __DATE__ " " __TIME__ "</small><br />\n";
#ifdef CONFIG_DEBUG
	sText += "<small><font color=\"red\">";
	sText += tr("Debugging option enabled.");
	sText += "</font></small><br />";
#endif
	sText += "<br />\n";
	sText += tr("Website") + ": <a href=\"" QXGEDIT_WEBSITE "\">" QXGEDIT_WEBSITE "</a><br />\n";
	sText += "<br />\n";
	sText += "<small>";
	sText += QXGEDIT_COPYRIGHT "<br />\n";
	sText += "<br />\n";
	sText += tr("This program is free software; you can redistribute it and/or modify it") + "<br />\n";
	sText += tr("under the terms of the GNU General Public License version 2 or later.");
	sText += "</small>";
	sText += "</p>\n";

	QMessageBox::about(this, tr("About") + " " QXGEDIT_TITLE, sText);
}


// Show information about the Qt toolkit.
void qxgeditMainForm::helpAboutQt (void)
{
	QMessageBox::aboutQt(this);
}


//-------------------------------------------------------------------------
// qxgeditMainForm -- Main window stabilization.

void qxgeditMainForm::stabilizeForm (void)
{
	// Update the main application caption...
	QString sSessionName = sessionName(m_sFilename);
	if (m_iDirtyCount > 0)
		sSessionName += ' ' + tr("[modified]");
	setWindowTitle(sSessionName + " - " QXGEDIT_TITLE);

	// Update the main menu state...
	m_ui.fileSaveAction->setEnabled(m_iDirtyCount > 0);

	// Recent files menu.
	m_ui.fileOpenRecentMenu->setEnabled(m_pOptions->recentFiles.count() > 0);

	m_statusItems[StatusName]->setText(sessionName(m_sFilename));

	if (m_iDirtyCount > 0)
		m_statusItems[StatusMod]->setText(tr("MOD"));
	else
		m_statusItems[StatusMod]->clear();
}


// Update the recent files list and menu.
void qxgeditMainForm::updateRecentFiles ( const QString& sFilename )
{
	if (m_pOptions == NULL)
		return;

	// Remove from list if already there (avoid duplicates)
	int iIndex = m_pOptions->recentFiles.indexOf(sFilename);
	if (iIndex >= 0)
		m_pOptions->recentFiles.removeAt(iIndex);
	// Put it to front...
	m_pOptions->recentFiles.push_front(sFilename);
}


// Update the recent files list and menu.
void qxgeditMainForm::updateRecentFilesMenu (void)
{
	if (m_pOptions == NULL)
		return;

	// Time to keep the list under limits.
	int iRecentFiles = m_pOptions->recentFiles.count();
	while (iRecentFiles > m_pOptions->iMaxRecentFiles) {
		m_pOptions->recentFiles.pop_back();
		iRecentFiles--;
	}

	// Rebuild the recent files menu...
	m_ui.fileOpenRecentMenu->clear();
	for (int i = 0; i < iRecentFiles; i++) {
		const QString& sFilename = m_pOptions->recentFiles[i];
		if (QFileInfo(sFilename).exists()) {
			QAction *pAction = m_ui.fileOpenRecentMenu->addAction(
				QString("&%1 %2").arg(i + 1).arg(sessionName(sFilename)),
				this, SLOT(fileOpenRecent()));
			pAction->setData(i);
		}
	}
}


// XG System Reset...
void qxgeditMainForm::masterResetButtonClicked (void)
{
	if (m_pParamMap) {
		XGParam *pParam = m_pParamMap->find_param(0x00, 0x00, 0x7e);
		if (pParam)
			pParam->set_value_update(0);
	}
}


// Switch the current MULTIPART section...
void qxgeditMainForm::multipartComboActivated ( int iPart )
{
	if (m_pParamMap)
		m_pParamMap->MULTIPART.set_current_key(iPart);
}


// Switch the current MULTIPART Instrument Normal Voice...
void qxgeditMainForm::multipartVoiceComboActivated ( int iVoice )
{
	if (m_iMultipartVoiceUpdate > 0)
		return;

	m_iMultipartVoiceUpdate++;

	const QModelIndex& parent
		= m_ui.MultipartVoiceCombo->view()->currentIndex().parent();

	XGInstrument instr(parent.row());
	if (instr.group()) {
		XGNormalVoice voice(&instr, iVoice);
	#ifdef CONFIG_DEBUG
		qDebug("qxgeditMainForm::multipartVoiceComboActivated(%d)"
			" parent=(%d) [%s/%s]",
			iVoice, parent.row(),
			instr.name(), voice.name());
	#endif
		m_ui.MultipartBankMSBDial->setValue(voice.bank() >> 7);
		m_ui.MultipartBankLSBDial->setValue(voice.bank() & 0x7f);
		m_ui.MultipartProgramDial->setValue(voice.prog());
	}

	m_iMultipartVoiceUpdate--;
}

void qxgeditMainForm::multipartVoiceChanged (void)
{
	if (m_iMultipartVoiceUpdate > 0)
		return;

	m_iMultipartVoiceUpdate++;

	unsigned short iBank
		= (m_ui.MultipartBankMSBDial->value() << 7)
		| m_ui.MultipartBankLSBDial->value();
	unsigned char iProg = m_ui.MultipartProgramDial->value();

	for (unsigned short i = 0; i < XGInstrument::count(); ++i) {
		XGInstrument instr(i);
		int j = instr.find_voice(iBank, iProg);
		if (j >= 0) {
			XGNormalVoice voice(&instr, j);
		//	m_ui.MultipartVoiceCombo->showPopup();
			const QModelIndex& parent
				= m_ui.MultipartVoiceCombo->model()->index(i, 0);
			const QModelIndex& index
				= m_ui.MultipartVoiceCombo->model()->index(j, 0, parent);
		#ifdef CONFIG_DEBUG
			qDebug("qxgeditMainForm::multipartVoiceChanged(%d)"
				" parent=%d bank=%u prog=%u [%s/%s]",
				index.row(), parent.row(),
				iBank, iProg, instr.name(), voice.name());
		#endif
		//	m_ui.MultipartVoiceCombo->view()->setCurrentIndex(index);
			QModelIndex oldroot = m_ui.MultipartVoiceCombo->rootModelIndex();
			m_ui.MultipartVoiceCombo->setRootModelIndex(parent);
			m_ui.MultipartVoiceCombo->setCurrentIndex(index.row());
			m_ui.MultipartVoiceCombo->setRootModelIndex(oldroot);
		}
	}

	m_iMultipartVoiceUpdate--;
}


void qxgeditMainForm::multipartResetButtonClicked (void)
{
	if (m_pParamMap)
		m_pParamMap->reset_part(m_ui.MultipartCombo->currentIndex());

	multipartVoiceChanged();
}


// Switch the current DRUMSETUP section...
void qxgeditMainForm::drumsetupComboActivated ( int iDrumset )
{
	if (m_pParamMap) {
		int iNote = m_ui.DrumsetupNoteCombo->currentIndex();
		unsigned short key = (unsigned short) (iDrumset << 7)
			+ m_ui.DrumsetupNoteCombo->itemData(iNote).toUInt();
		m_pParamMap->DRUMSETUP.set_current_key(key);
	}
}

// Switch the current DRUMSETUP Drum Kit Voice...
void qxgeditMainForm::drumsetupVoiceComboActivated ( int iDrumKit )
{
	// Diatonic note map...
	static const char *notes[] =
		{ "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

	XGDrumKit drumkit(iDrumKit);
	if (drumkit.item()) {
		XGDrumKit stdkit(0); // Standard Kit (default)
		m_ui.DrumsetupNoteCombo->clear();
		for (unsigned short k = 13; k < 85; ++k) {
			QString sName;
			int i = drumkit.find_voice(k);
			if (i >= 0)  {
				sName = XGDrumVoice(&drumkit, i).name();
			} else if (stdkit.item()) {
				i = stdkit.find_voice(k);
				if (i >= 0)
					sName = XGDrumVoice(&stdkit, i).name();
			}
			if (!sName.isEmpty())
				sName += ' ';
			sName += QString("(%1%2)").arg(notes[k % 12]).arg((k / 12) - 2);
			m_ui.DrumsetupNoteCombo->addItem(sName, k);
		}
		int iNote = m_ui.DrumsetupNoteCombo->findData(
			m_pParamMap->DRUMSETUP.current_key());
		if (iNote >= 0)
			m_ui.DrumsetupNoteCombo->setCurrentIndex(iNote);
	}
}


void qxgeditMainForm::drumsetupNoteComboActivated ( int iNote )
{
	if (m_pParamMap) {
		int iDrumset = m_ui.DrumsetupCombo->currentIndex();
		unsigned short key = (unsigned short) (iDrumset << 7)
			+ m_ui.DrumsetupNoteCombo->itemData(iNote).toUInt();
		m_pParamMap->DRUMSETUP.set_current_key(key);
	}
}


void qxgeditMainForm::drumsetupResetButtonClicked (void)
{
	if (m_pParamMap) {
		int iDrumset = m_ui.DrumsetupCombo->currentIndex();
		XGParam *pParam = m_pParamMap->find_param(0x00, 0x00, 0x7d);
		if (pParam)
			pParam->set_value_update(iDrumset);
	}
}


// Main dirty flag raiser.
void qxgeditMainForm::contentsChanged (void)
{
	m_iDirtyCount++;
	stabilizeForm();
}


//-------------------------------------------------------------------------
// qxgeditMainForm -- Messages window form handlers.

// Messages output methods.
void qxgeditMainForm::showMessage( const QString& s )
{
	statusBar()->showMessage(s, 3000);
}

void qxgeditMainForm::showMessageError( const QString& s )
{
	QMessageBox::critical(this,
		tr("Error") + " - " QXGEDIT_TITLE, s, QMessageBox::Ok);
}


// end of qxgeditMainForm.cpp
