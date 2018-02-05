// qxgeditMidiRpn.h
//
/****************************************************************************
   Copyright (C) 2005-2018, rncbc aka Rui Nuno Capela. All rights reserved.

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

#ifndef __qxgeditMidiRpn_h
#define __qxgeditMidiRpn_h


//---------------------------------------------------------------------
// qxgeditMidiRpn - decl.

class qxgeditMidiRpn
{
public:

	qxgeditMidiRpn();

	~qxgeditMidiRpn();

	enum Type { None = 0, CC = 0x10, RPN = 0x20, NRPN = 0x30, CC14 = 0x40 };

	struct Event
	{
		unsigned long  time;
		int            port;
		unsigned char  status;
		unsigned short param;
		unsigned short value;
	};

	bool isPending() const;

	bool process(const Event& event);
	bool dequeue(Event& event);

	void flush();

private:

	class Impl;

	Impl *m_pImpl;
};


#endif	//  __qxgeditMidiRpn_h

// end of qxgeditMidiRpn.h
