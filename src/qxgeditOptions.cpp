// qxgeditOptions.cpp
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

#include "qxgeditAbout.h"
#include "qxgeditOptions.h"

#include <QWidget>
#include <QTextStream>


//-------------------------------------------------------------------------
// qxgeditOptions - Prototype settings structure (pseudo-singleton).
//

// Singleton instance pointer.
qxgeditOptions *qxgeditOptions::g_pOptions = NULL;

// Singleton instance accessor (static).
qxgeditOptions *qxgeditOptions::getInstance (void)
{
	return g_pOptions;
}


// Constructor.
qxgeditOptions::qxgeditOptions (void)
	: m_settings(QXGEDIT_DOMAIN, QXGEDIT_TITLE)
{
	// Pseudo-singleton reference setup.
	g_pOptions = this;

	// And go into general options group.
	m_settings.beginGroup("/Options");

	// MIDI specific options...
	m_settings.beginGroup("/Midi");
	midiInputs  = m_settings.value("/Inputs").toStringList();
	midiOutputs = m_settings.value("/Outputs").toStringList();
	m_settings.endGroup();

	// Load display options...
	m_settings.beginGroup("/Display");
	bCompletePath   = m_settings.value("/CompletePath", true).toBool();
	iMaxRecentFiles = m_settings.value("/MaxRecentFiles", 5).toInt();
	iBaseFontSize   = m_settings.value("/BaseFontSize", 0).toInt();
	m_settings.endGroup();

	// And go into view options group.
	m_settings.beginGroup("/View");
	bMenubar   = m_settings.value("/Menubar", true).toBool();
	bStatusbar = m_settings.value("/Statusbar", true).toBool();
	bToolbar   = m_settings.value("/Toolbar", true).toBool();
	m_settings.endGroup();

	m_settings.endGroup(); // End of options group.

	// Last but not least, get the defaults.
	m_settings.beginGroup("/Default");
	sSessionDir = m_settings.value("/SessionDir").toString();
	recentFiles = m_settings.value("/RecentFiles").toStringList();
	m_settings.endGroup();

	// (QS300) USER VOICE Specific options.
	m_settings.beginGroup("/Uservoice");
	bUservoiceAutoSend = m_settings.value("/AutoSend", false).toBool();
	m_settings.endGroup();
}


// Default Destructor.
qxgeditOptions::~qxgeditOptions (void)
{
	// Make program version available in the future.
	m_settings.beginGroup("/Program");
	m_settings.setValue("/Version", QXGEDIT_VERSION);
	m_settings.endGroup();

	// And go into general options group.
	m_settings.beginGroup("/Options");

	// MIDI specific options...
	m_settings.beginGroup("/Midi");
	m_settings.setValue("/Inputs", midiInputs);
	m_settings.setValue("/Outputs", midiOutputs);
	m_settings.endGroup();

	// Save display options.
	m_settings.beginGroup("/Display");
	m_settings.setValue("/CompletePath", bCompletePath);
	m_settings.setValue("/MaxRecentFiles", iMaxRecentFiles);
	m_settings.setValue("/BaseFontSize", iBaseFontSize);
	m_settings.endGroup();

	// View options group.
	m_settings.beginGroup("/View");
	m_settings.setValue("/Menubar", bMenubar);
	m_settings.setValue("/Statusbar", bStatusbar);
	m_settings.setValue("/Toolbar", bToolbar);
	m_settings.endGroup();

	m_settings.endGroup(); // End of options group.

	// Default directories.
	m_settings.beginGroup("/Default");
	m_settings.setValue("/SessionDir", sSessionDir);
	m_settings.setValue("/RecentFiles", recentFiles);
	m_settings.endGroup();

	// (QS300) USER VOICE Specific options.
	m_settings.beginGroup("/Uservoice");
	m_settings.setValue("/AutoSend", bUservoiceAutoSend);
	m_settings.endGroup();

	// Pseudo-singleton reference shut-down.
	g_pOptions = NULL;
}


//-------------------------------------------------------------------------
// Settings accessor.
//

QSettings& qxgeditOptions::settings (void)
{
	return m_settings;
}


//-------------------------------------------------------------------------
// Command-line argument stuff.
//

// Help about command line options.
void qxgeditOptions::print_usage ( const QString& arg0 )
{
	QTextStream out(stderr);
	out << QObject::tr(
		"Usage: %1 [options] [syx-file]\n\n"
		QXGEDIT_TITLE " - " QXGEDIT_SUBTITLE "\n\n"
		"Options:\n\n"
		"  -h, --help\n\tShow help about command line options\n\n"
		"  -v, --version\n\tShow version information\n\n")
		.arg(arg0);
}


// Parse command line arguments into m_settings.
bool qxgeditOptions::parse_args ( const QStringList& args )
{
	QTextStream out(stderr);
	const QString sEol = "\n\n";
	int iCmdArgs = 0;
	int argc = args.count();

	for (int i = 1; i < argc; ++i) {

		if (iCmdArgs > 0) {
			sSessionFile += ' ';
			sSessionFile += args.at(i);
			iCmdArgs++;
			continue;
		}

		QString sArg = args.at(i);

		if (sArg == "-h" || sArg == "--help") {
			print_usage(args.at(0));
			return false;
		}
		else if (sArg == "-v" || sArg == "--version") {
			out << QObject::tr("Qt: %1\n").arg(qVersion());
			out << QObject::tr(QXGEDIT_TITLE ": %1\n").arg(QXGEDIT_VERSION);
			return false;
		}
		else {
			// If we don't have one by now,
			// this will be the startup session file...
			sSessionFile += sArg;
			iCmdArgs++;
		}
	}

	// Alright with argument parsing.
	return true;
}


//---------------------------------------------------------------------------
// Widget geometry persistence helper methods.

void qxgeditOptions::loadWidgetGeometry ( QWidget *pWidget )
{
	// Try to restore old form window positioning.
	if (pWidget) {
		QPoint fpos;
		QSize  fsize;
		bool bVisible;
		m_settings.beginGroup("/Geometry/" + pWidget->objectName());
		fpos.setX(m_settings.value("/x", -1).toInt());
		fpos.setY(m_settings.value("/y", -1).toInt());
		fsize.setWidth(m_settings.value("/width", -1).toInt());
		fsize.setHeight(m_settings.value("/height", -1).toInt());
		bVisible = m_settings.value("/visible", false).toBool();
		m_settings.endGroup();
		if (fpos.x() > 0 && fpos.y() > 0)
			pWidget->move(fpos);
		if (fsize.width() > 0 && fsize.height() > 0)
			pWidget->resize(fsize);
		else
			pWidget->adjustSize();
		if (bVisible)
			pWidget->show();
		else
			pWidget->hide();
	}
}


void qxgeditOptions::saveWidgetGeometry ( QWidget *pWidget )
{
	// Try to save form window position...
	// (due to X11 window managers ideossincrasies, we better
	// only save the form geometry while its up and visible)
	if (pWidget) {
		m_settings.beginGroup("/Geometry/" + pWidget->objectName());
		bool bVisible = pWidget->isVisible();
		const QPoint& fpos  = pWidget->pos();
		const QSize&  fsize = pWidget->size();
		m_settings.setValue("/x", fpos.x());
		m_settings.setValue("/y", fpos.y());
		m_settings.setValue("/width", fsize.width());
		m_settings.setValue("/height", fsize.height());
		m_settings.setValue("/visible", bVisible);
		m_settings.endGroup();
	}
}


// end of qxgeditOptions.cpp
