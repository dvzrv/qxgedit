// qxgedit.cpp
//
/****************************************************************************
   Copyright (C) 2005-2019, rncbc aka Rui Nuno Capela. All rights reserved.

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

#include "qxgedit.h"

#include "qxgeditOptions.h"
#include "qxgeditMainForm.h"

#include <QStyleFactory>

#include <QLibraryInfo>
#include <QTranslator>
#include <QLocale>

#include <QDir>

#ifndef CONFIG_PREFIX
#define CONFIG_PREFIX	"/usr/local"
#endif

#ifndef CONFIG_DATADIR
#define CONFIG_DATADIR	CONFIG_PREFIX "/share"
#endif

#ifndef CONFIG_LIBDIR
#if defined(__x86_64__)
#define CONFIG_LIBDIR	CONFIG_PREFIX "/lib64"
#else
#define CONFIG_LIBDIR	CONFIG_PREFIX "/lib"
#endif
#endif

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
#define CONFIG_PLUGINSDIR CONFIG_LIBDIR "/qt4/plugins"
#else
#define CONFIG_PLUGINSDIR CONFIG_LIBDIR "/qt5/plugins"
#endif


//-------------------------------------------------------------------------
// Singleton application instance stuff (Qt/X11 only atm.)
//

#ifdef CONFIG_XUNIQUE

#define QXGEDIT_XUNIQUE "qxgeditApplication"

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
#ifdef CONFIG_X11

#include <unistd.h> /* for gethostname() */

#include <X11/Xatom.h>
#include <X11/Xlib.h>

#endif	// CONFIG_X11
#else
#include <QSharedMemory>
#include <QLocalServer>
#include <QLocalSocket>
#include <QHostInfo>
#endif

#endif	// CONFIG_XUNIQUE


// Constructor.
qxgeditApplication::qxgeditApplication ( int& argc, char **argv )
	: QApplication(argc, argv),
		m_pQtTranslator(nullptr), m_pMyTranslator(nullptr), m_pWidget(nullptr)
{
	// Load translation support.
	QLocale loc;
	if (loc.language() != QLocale::C) {
		// Try own Qt translation...
		m_pQtTranslator = new QTranslator(this);
		QString sLocName = "qt_" + loc.name();
		QString sLocPath = QLibraryInfo::location(QLibraryInfo::TranslationsPath);
		if (m_pQtTranslator->load(sLocName, sLocPath)) {
			QApplication::installTranslator(m_pQtTranslator);
		} else {
			delete m_pQtTranslator;
			m_pQtTranslator = 0;
		#ifdef CONFIG_DEBUG
			qWarning("Warning: no translation found for '%s' locale: %s/%s.qm",
				loc.name().toUtf8().constData(),
				sLocPath.toUtf8().constData(),
				sLocName.toUtf8().constData());
		#endif
		}
		// Try own application translation...
		m_pMyTranslator = new QTranslator(this);
		sLocName = "qxgedit_" + loc.name();
		if (m_pMyTranslator->load(sLocName, sLocPath)) {
			QApplication::installTranslator(m_pMyTranslator);
		} else {
			sLocPath = CONFIG_DATADIR "/QXGEDIT/translations";
			if (m_pMyTranslator->load(sLocName, sLocPath)) {
				QApplication::installTranslator(m_pMyTranslator);
			} else {
				delete m_pMyTranslator;
				m_pMyTranslator = 0;
			#ifdef CONFIG_DEBUG
				qWarning("Warning: no translation found for '%s' locale: %s/%s.qm",
					loc.name().toUtf8().constData(),
					sLocPath.toUtf8().constData(),
					sLocName.toUtf8().constData());
			#endif
			}
		}
	}
#ifdef CONFIG_XUNIQUE
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
#ifdef CONFIG_X11
	m_pDisplay = nullptr;
	m_aUnique = 0;
	m_wOwner = 0;
#endif	// CONFIG_X11
#else
	m_pMemory = nullptr;
	m_pServer = nullptr;
#endif
#endif	// CONFIG_XUNIQUE
}

// Destructor.
qxgeditApplication::~qxgeditApplication (void)
{
#ifdef CONFIG_XUNIQUE
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
	if (m_pServer) {
		m_pServer->close();
		delete m_pServer;
		m_pServer = nullptr;
	}
	if (m_pMemory) {
		delete m_pMemory;
		m_pMemory = nullptr;
	}
#endif
#endif	// CONFIG_XUNIQUE
	if (m_pMyTranslator) delete m_pMyTranslator;
	if (m_pQtTranslator) delete m_pQtTranslator;
}


// Main application widget accessors.
void qxgeditApplication::setMainWidget ( QWidget *pWidget )
{
	m_pWidget = pWidget;
#ifdef CONFIG_XUNIQUE
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
#ifdef CONFIG_X11
	m_wOwner = m_pWidget->winId();
	if (m_pDisplay && m_wOwner) {
		XGrabServer(m_pDisplay);
		XSetSelectionOwner(m_pDisplay, m_aUnique, m_wOwner, CurrentTime);
		XUngrabServer(m_pDisplay);
	}
#endif	// CONFIG_X11
#endif
#endif	// CONFIG_XUNIQUE
}


// Check if another instance is running,
// and raise its proper main widget...
bool qxgeditApplication::setup (void)
{
#ifdef CONFIG_XUNIQUE
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
#ifdef CONFIG_X11
	m_pDisplay = QX11Info::display();
	if (m_pDisplay) {
		QString sUnique = QXGEDIT_XUNIQUE;
		char szHostName[255];
		if (::gethostname(szHostName, sizeof(szHostName)) == 0) {
			sUnique += '@';
			sUnique += szHostName;
		}
		m_aUnique = XInternAtom(m_pDisplay, sUnique.toUtf8().constData(), false);
		XGrabServer(m_pDisplay);
		m_wOwner = XGetSelectionOwner(m_pDisplay, m_aUnique);
		XUngrabServer(m_pDisplay);
		if (m_wOwner != None) {
			// First, notify any freedesktop.org WM
			// that we're about to show the main widget...
			Screen *pScreen = XDefaultScreenOfDisplay(m_pDisplay);
			int iScreen = XScreenNumberOfScreen(pScreen);
			XEvent ev;
			memset(&ev, 0, sizeof(ev));
			ev.xclient.type = ClientMessage;
			ev.xclient.display = m_pDisplay;
			ev.xclient.window = m_wOwner;
			ev.xclient.message_type = XInternAtom(m_pDisplay, "_NET_ACTIVE_WINDOW", false);
			ev.xclient.format = 32;
			ev.xclient.data.l[0] = 0; // Source indication.
			ev.xclient.data.l[1] = 0; // Timestamp.
			ev.xclient.data.l[2] = 0; // Requestor's currently active window (none)
			ev.xclient.data.l[3] = 0;
			ev.xclient.data.l[4] = 0;
			XSelectInput(m_pDisplay, m_wOwner, StructureNotifyMask);
			XSendEvent(m_pDisplay, RootWindow(m_pDisplay, iScreen), false,
					   (SubstructureNotifyMask | SubstructureRedirectMask), &ev);
			XSync(m_pDisplay, false);
			XRaiseWindow(m_pDisplay, m_wOwner);
			// And then, let it get caught on destination
			// by QApplication::native/x11EventFilter...
			const QByteArray value = QSAMPLER_XUNIQUE;
			XChangeProperty(
				m_pDisplay,
				m_wOwner,
				m_aUnique,
				m_aUnique, 8,
				PropModeReplace,
				(unsigned char *) value.data(),
							value.length());
			// Done.
			return true;
		}
	}
#endif	// CONFIG_X11
	return false;
#else
	m_sUnique = QCoreApplication::applicationName();
	m_sUnique += '@';
	m_sUnique += QHostInfo::localHostName();
#ifdef Q_OS_UNIX
	m_pMemory = new QSharedMemory(m_sUnique);
	m_pMemory->attach();
	delete m_pMemory;
#endif
	m_pMemory = new QSharedMemory(m_sUnique);
	bool bServer = false;
	const qint64 pid = QCoreApplication::applicationPid();
	struct Data { qint64 pid; };
	if (m_pMemory->create(sizeof(Data))) {
		m_pMemory->lock();
		Data *pData = static_cast<Data *> (m_pMemory->data());
		if (pData) {
			pData->pid = pid;
			bServer = true;
		}
		m_pMemory->unlock();
	}
	else
	if (m_pMemory->attach()) {
		m_pMemory->lock(); // maybe not necessary?
		Data *pData = static_cast<Data *> (m_pMemory->data());
		if (pData)
			bServer = (pData->pid == pid);
		m_pMemory->unlock();
	}
	if (bServer) {
		QLocalServer::removeServer(m_sUnique);
		m_pServer = new QLocalServer();
		m_pServer->setSocketOptions(QLocalServer::UserAccessOption);
		m_pServer->listen(m_sUnique);
		QObject::connect(m_pServer,
			SIGNAL(newConnection()),
			SLOT(newConnectionSlot()));
	} else {
		QLocalSocket socket;
		socket.connectToServer(m_sUnique);
		if (socket.state() == QLocalSocket::ConnectingState)
			socket.waitForConnected(200);
		if (socket.state() == QLocalSocket::ConnectedState) {
			socket.write(QCoreApplication::arguments().join(' ').toUtf8());
			socket.flush();
			socket.waitForBytesWritten(200);
		}
	}
	return !bServer;
#endif
#else
	return false;
#endif	// !CONFIG_XUNIQUE
}


#ifdef CONFIG_XUNIQUE
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
#ifdef CONFIG_X11

void qxgeditApplication::x11PropertyNotify ( Window w )
{
	if (m_pDisplay && m_pWidget && m_wOwner == w) {
		// Always check whether our property-flag is still around...
		Atom aType;
		int iFormat = 0;
		unsigned long iItems = 0;
		unsigned long iAfter = 0;
		unsigned char *pData = 0;
		if (XGetWindowProperty(
			m_pDisplay,
			m_wOwner,
			m_aUnique,
			0, 1024,
			false,
			m_aUnique,
			&aType,
			&iFormat,
			&iItems,
			&iAfter,
			&pData) == Success
		&& aType == m_aUnique && iItems > 0 && iAfter == 0) {
			// Avoid repeating it-self...
			XDeleteProperty(m_pDisplay, m_wOwner, m_aUnique);
			// Just make it always shows up fine...
			m_pWidget->show();
			m_pWidget->raise();
			m_pWidget->activateWindow();
		}
		// Free any left-overs...
		if (iItems > 0 && pData)
			XFree(pData);
	}
}


bool qxgeditApplication::x11EventFilter ( XEvent *pEv )
{
	if (pEv->type == PropertyNotify
		&& pEv->xproperty.state == PropertyNewValue)
		x11PropertyNotify(pEv->xproperty.window);
	return QApplication::x11EventFilter(pEv);
}

#endif	// CONFIG_X11
#else

// Local server conection slot.
void qxgeditApplication::newConnectionSlot (void)
{
	QLocalSocket *pSocket = m_pServer->nextPendingConnection();
	QObject::connect(pSocket,
		SIGNAL(readyRead()),
		SLOT(readyReadSlot()));
}

// Local server data-ready slot.
void qxgeditApplication::readyReadSlot (void)
{
	QLocalSocket *pSocket = qobject_cast<QLocalSocket *> (sender());
	if (pSocket) {
		const qint64 nread = pSocket->bytesAvailable();
		if (nread > 0) {
			const QByteArray data = pSocket->read(nread);
			// Just make it always shows up fine...
			m_pWidget->hide();
			m_pWidget->show();
			m_pWidget->raise();
			m_pWidget->activateWindow();
		}
	}
}

#endif
#endif	// CONFIG_XUNIQUE


//-------------------------------------------------------------------------
// stacktrace - Signal crash handler.
//

#ifdef CONFIG_STACKTRACE
#if defined(__GNUC__) && defined(Q_OS_LINUX)

#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>

void stacktrace ( int signo )
{
	pid_t pid;
	int rc;
	int status = 0;
	char cmd[80];

	// Reinstall default handler; prevent race conditions...
	::signal(signo, SIG_DFL);

	static const char *shell  = "/bin/sh";
	static const char *format = "gdb -q --batch --pid=%d"
		" --eval-command='thread apply all bt'";

	snprintf(cmd, sizeof(cmd), format, (int) getpid());

	pid = fork();

	// Fork failure!
	if (pid < 0)
		return;

	// Fork child...
	if (pid == 0) {
		execl(shell, shell, "-c", cmd, nullptr);
		_exit(1);
		return;
	}

	// Parent here: wait for child to terminate...
	do { rc = waitpid(pid, &status, 0); }
	while ((rc < 0) && (errno == EINTR));

	// Dispatch any logging, if any...
	QApplication::processEvents(QEventLoop::AllEvents, 3000);

	// Make sure everyone terminates...
	kill(pid, SIGTERM);
	_exit(1);
}

#endif
#endif


//-------------------------------------------------------------------------
// main - The main program trunk.
//

int main ( int argc, char **argv )
{
	Q_INIT_RESOURCE(qxgedit);
#ifdef CONFIG_STACKTRACE
#if defined(__GNUC__) && defined(Q_OS_LINUX)
	::signal(SIGILL,  stacktrace);
	::signal(SIGFPE,  stacktrace);
	::signal(SIGSEGV, stacktrace);
	::signal(SIGABRT, stacktrace);
	::signal(SIGBUS,  stacktrace);
#endif
#endif
	qxgeditApplication app(argc, argv);
#if QT_VERSION >= QT_VERSION_CHECK(5, 6, 0)
	app.setAttribute(Qt::AA_EnableHighDpiScaling);
#endif
	// Construct default settings; override with command line arguments.
	qxgeditOptions options;
	if (!options.parse_args(app.arguments())) {
		app.quit();
		return 1;
	}

	// Have another instance running?
	if (app.setup()) {
		app.quit();
		return 2;
	}

	// Dark themes grayed/disabled color group fix...
	QPalette pal(app.palette());
	if (pal.base().color().value() < 0x7f) {
	#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
		const QColor& color = pal.window().color();
		const int iGroups = int(QPalette::Active | QPalette::Inactive) + 1;
		for (int i = 0; i < iGroups; ++i) {
			const QPalette::ColorGroup group = QPalette::ColorGroup(i);
			pal.setBrush(group, QPalette::Light,    color.lighter(150));
			pal.setBrush(group, QPalette::Midlight, color.lighter(120));
			pal.setBrush(group, QPalette::Dark,     color.darker(150));
			pal.setBrush(group, QPalette::Mid,      color.darker(120));
			pal.setBrush(group, QPalette::Shadow,   color.darker(200));
		}
	//	pal.setColor(QPalette::Disabled, QPalette::ButtonText, pal.mid().color());
	#endif
		pal.setColorGroup(QPalette::Disabled,
			pal.windowText().color().darker(),
			pal.button(),
			pal.light(),
			pal.dark(),
			pal.mid(),
			pal.text().color().darker(),
			pal.text().color().lighter(),
			pal.base(),
			pal.window());
		app.setPalette(pal);
	}

	// Set default base font...
	if (options.iBaseFontSize > 0)
		app.setFont(QFont(app.font().family(), options.iBaseFontSize));

	// Special styles...
	if (QDir(CONFIG_PLUGINSDIR).exists())
		QApplication::addLibraryPath(CONFIG_PLUGINSDIR);
	if (!options.sStyleTheme.isEmpty())
		QApplication::setStyle(QStyleFactory::create(options.sStyleTheme));

	// Construct, setup and show the main form (a pseudo-singleton).
	qxgeditMainForm w;
	w.setup(&options);
	w.show();

	// Settle this one as application main widget...
	app.setMainWidget(&w);

	return app.exec();
}


// end of qxgedit.cpp

