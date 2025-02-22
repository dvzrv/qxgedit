# skulpture.pro
#
NAME = skulpturestyle

TARGET = $${NAME}
TEMPLATE = lib
CONFIG	+= shared plugin

include(skulpture.pri)

HEADERS	+= \
	sk_factory.h \
	skulpture_p.h \
	skulpture.h

SOURCES	+= skulpture.cpp

!lessThan(QT_MAJOR_VERSION, 5) {
	HEADERS	+= sk_plugin.h
	OTHER_FILES += skulpture.json
	QT += widgets
}


unix {

	# variables
	OBJECTS_DIR = .obj
	MOC_DIR     = .moc
	UI_DIR      = .ui

	isEmpty(PREFIX) {
		PREFIX = /usr/local
	}

	isEmpty(LIBDIR) {
		TARGET_ARCH = $$system(uname -m)
		contains(TARGET_ARCH, x86_64) {
			LIBDIR = $${PREFIX}/lib64
		} else {
			LIBDIR = $${PREFIX}/lib
		}
	}

	lessThan(QT_MAJOR_VERSION, 5) {
		TARGET_PATH = $${LIBDIR}/qt4/plugins/styles
	} else {
		TARGET_PATH = $${LIBDIR}/qt5/plugins/styles
	}

	isEmpty(QMAKE_EXTENSION_SHLIB) {
		QMAKE_EXTENSION_SHLIB = so
	}

	TARGET_FILES = lib$${TARGET}.$${QMAKE_EXTENSION_SHLIB}

	INSTALLS += target

	target.path  = $${TARGET_PATH}
	target.files = $${TARGET_FILES}

	QMAKE_CLEAN += $${TARGET_FILES}
}
