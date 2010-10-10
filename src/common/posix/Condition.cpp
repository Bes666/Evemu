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
    Author:     Bloody.Rabbit
*/

#include "CommonPCH.h"

#include "posix/Condition.h"
#include "posix/ConditionAttribute.h"

/*************************************************************************/
/* Posix::Condition                                                      */
/*************************************************************************/
const Posix::Condition::Attribute Posix::Condition::DEFAULT_ATTRIBUTE;

Posix::Condition::Condition( const Attribute& attr )
{
    int code;

    code = ::pthread_cond_init( &mCondition, &attr.mAttribute );
    assert( 0 == code );
}

Posix::Condition::~Condition()
{
    int code;

    code = ::pthread_cond_destroy( &mCondition );
    assert( 0 == code );
}

int Posix::Condition::Signal()
{
    return ::pthread_cond_signal( &mCondition );
}

int Posix::Condition::Broadcast()
{
    return ::pthread_cond_broadcast( &mCondition );
}

int Posix::Condition::Wait( Posix::Mutex& mutex )
{
    return ::pthread_cond_wait( &mCondition, &mutex.mMutex );
}

int Posix::Condition::TimedWait( Posix::Mutex& mutex, const Time::Timespec& time )
{
    return ::pthread_cond_timedwait( &mCondition, &mutex.mMutex, &time.ts() );
}