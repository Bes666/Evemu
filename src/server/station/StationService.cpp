/*
	------------------------------------------------------------------------------------
	LICENSE:
	------------------------------------------------------------------------------------
	This file is part of EVEmu: EVE Online Server Emulator
	Copyright 2006 - 2008 The EVEmu Team
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
	Author:		Zhur
*/

#include "EvemuPCH.h"

PyCallable_Make_InnerDispatcher(StationService)

StationService::StationService(PyServiceMgr *mgr, DBcore *db)
: PyService(mgr, "station"), m_dispatch(new Dispatcher(this)), m_db(db)
{
	_SetCallDispatcher(m_dispatch);

	PyCallable_REG_CALL(StationService, GetStationItemBits)
	PyCallable_REG_CALL(StationService, GetGuests)
}

StationService::~StationService() {
	delete m_dispatch;
}

PyResult StationService::Handle_GetStationItemBits(PyCallArgs &call) {
	return m_db.GetStationItemBits(call.client->GetStationID());
}

PyResult StationService::Handle_GetGuests(PyCallArgs &call) {
	PyRepList *res = new PyRepList();

	PyRepTuple *t = new PyRepTuple(4);
	t->items[0] = new PyRepInteger(call.client->GetCharacterID());
	t->items[1] = new PyRepInteger(call.client->GetCorporationID());
	t->items[2] = new PyRepInteger(call.client->GetAllianceID());
	t->items[3] = new PyRepInteger(0);	//unknown, might be factionID
	res->add(t);

	return res;
}