/*
	------------------------------------------------------------------------------------
	LICENSE:
	------------------------------------------------------------------------------------
	This file is part of EVEmu: EVE Online Server Emulator
	Copyright 2006 - 2009 The EVEmu Team
	For the latest information visit http://evemu.mmoforge.org
	------------------------------------------------------------------------------------
	This program is free software; you can redistribute it and/or modify it under
	the terms of the GNU Lesser General Public License as published by the Free Software
	Foundation; either version 2 of the License, or (at your option) any later
	version.

	This program is distributed in the hope that it will be useful, but WITHOUT
	ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
	FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more details.

	You should have received a copy of the GNU Lesser General Public License along with
	this program; if not, write to the Free Software Foundation, Inc., 59 Temple
	Place - Suite 330, Boston, MA 02111-1307, USA, or go to
	http://www.gnu.org/copyleft/lesser.txt.
	------------------------------------------------------------------------------------
	Author:		Captnoord
*/

#include "EvemuPCH.h"
#define SPACE_UPDATE_DELAY 50

SpaceRunnable::SpaceRunnable() : ThreadContext()
{

}

bool SpaceRunnable::run()
{
	SetThreadName("SpaceRunnable updater");

	Log.Notice("SpaceRunnable","Booting...");

	uint32 LastSpaceUpdate = getMSTime();
	uint32 LastSessionsUpdate = getMSTime();

	//THREAD_TRY_EXECUTION2

	while(m_threadRunning == true)
	{
		uint32 diff;
		//calculate time passed
		uint32 now,execution_start;
		now = getMSTime();
		execution_start = now;

		if( now < LastSpaceUpdate)//overrun
			diff = SPACE_UPDATE_DELAY;
		else
			diff = now - LastSpaceUpdate;
		
		LastSpaceUpdate = now;
		sSpace.Update( diff );
		
		now = getMSTime();
		
		if( now < LastSessionsUpdate)//overrun
			diff = SPACE_UPDATE_DELAY;
		else
			diff = now-LastSessionsUpdate;
		
		LastSessionsUpdate = now;
		sSpace.UpdateSessions( diff );
		
		now = getMSTime();
		//we have to wait now 
		
		if(execution_start > now)//overrun
			diff = SPACE_UPDATE_DELAY - now;
		else
			diff = now - execution_start;//time used for updating 

		/*This is execution time compensating system
			if execution took more than default delay 
			no need to make this sleep*/
		if(diff < SPACE_UPDATE_DELAY)
			Sleep(SPACE_UPDATE_DELAY - diff);
	}

	//THREAD_HANDLE_CRASH2
	return true;
}