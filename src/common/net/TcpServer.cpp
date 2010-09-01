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
    Author:     Zhur
*/

#include "CommonPCH.h"

#include "net/TcpServer.h"
#include "time/Timer.h"
#include "utils/Log.h"

/*************************************************************************/
/* Net::TcpServerBase                                                    */
/*************************************************************************/
const size_t Net::TcpServerBase::ERRBUF_SIZE = 1024;
const size_t Net::TcpServerBase::LOOP_GRANULARITY = 5;

Net::TcpServerBase::TcpServerBase()
: mSock( NULL ),
  mPort( 0 )
{
}

Net::TcpServerBase::~TcpServerBase()
{
    // Close socket
    Close();

    // Wait until worker thread terminates
    WaitLoop();
}

bool Net::TcpServerBase::IsOpen() const
{
    bool ret;

    mMSock.Lock();
    ret = ( mSock != NULL );
    mMSock.Unlock();

    return ret;
}

bool Net::TcpServerBase::Open( uint16 port, char* errbuf )
{
    if( errbuf != NULL )
        errbuf[0] = 0;

    // mutex lock
    MutexLock lock( mMSock );

    if( IsOpen() )
    {
        if( errbuf != NULL )
            snprintf( errbuf, ERRBUF_SIZE, "Listening socket already open" );
        return false;
    }
    else
    {
        mMSock.Unlock();

        // Wait for thread to terminate
        WaitLoop();

        mMSock.Lock();
    }

    // Setting up TCP port for new TCP connections
    mSock = new Net::Socket( AF_INET, SOCK_STREAM, 0 );

    // Quag: don't think following is good stuff for TCP, good for UDP
    // Mis: SO_REUSEADDR shouldn't be a problem for tcp - allows you to restart
    //      without waiting for conn's in TIME_WAIT to die
    unsigned int reuse_addr = 1;
    mSock->setopt( SOL_SOCKET, SO_REUSEADDR, &reuse_addr, sizeof( reuse_addr ) );

    // Setup internet address information.
    // This is used with the bind() call
    sockaddr_in address;
    memset( &address, 0, sizeof( address ) );

    address.sin_family = AF_INET;
    address.sin_port = htons( port );
    address.sin_addr.s_addr = htonl( INADDR_ANY );

    if( mSock->bind( (sockaddr*)&address, sizeof( address ) ) < 0 )
    {
        if( errbuf != NULL )
            sprintf( errbuf, "bind(): < 0" );

        SafeDelete( mSock );
        return false;
    }

    unsigned int bufsize = 64 * 1024; // 64kbyte receive buffer, up from default of 8k
    mSock->setopt( SOL_SOCKET, SO_RCVBUF, &bufsize, sizeof( bufsize ) );

#ifdef WIN32
    unsigned long nonblocking = 1;
    mSock->ioctl( FIONBIO, &nonblocking );
#else
    mSock->fcntl( F_SETFL, O_NONBLOCK );
#endif

    if( mSock->listen() == SOCKET_ERROR )
    {
        if( errbuf != NULL )
#ifdef WIN32
            snprintf( errbuf, ERRBUF_SIZE, "listen() failed, Error: %u", WSAGetLastError() );
#else
            snprintf( errbuf, ERRBUF_SIZE, "listen() failed, Error: %s", strerror( errno ) );
#endif

        SafeDelete( mSock );
        return false;
    }

    mPort = port;

    // Start processing thread
    StartLoop();

    return true;
}

void Net::TcpServerBase::Close()
{
    MutexLock lock( mMSock );

    SafeDelete( mSock );
    mPort = 0;
}

void Net::TcpServerBase::StartLoop()
{
#ifdef WIN32
    _beginthread( TcpServerLoop, 0, this );
#else
    pthread_t thread;
    pthread_create( &thread, NULL, TcpServerLoop, this );
#endif
}

void Net::TcpServerBase::WaitLoop()
{
    //wait for loop to stop.
    mMLoopRunning.Lock();
    mMLoopRunning.Unlock();
}

bool Net::TcpServerBase::Process()
{
    MutexLock lock( mMSock );

    if( !IsOpen() )
        return false;

    ListenNewConnections();
    return true;
}

void Net::TcpServerBase::ListenNewConnections()
{
    Net::Socket* sock;
    sockaddr_in  from;
    unsigned int fromlen;

    from.sin_family = AF_INET;
    fromlen = sizeof( from );

    MutexLock lock( mMSock );

    // Check for pending connects
    while( ( sock = mSock->accept( (sockaddr*)&from, &fromlen ) ) != NULL )
    {
#ifdef WIN32
        unsigned long nonblocking = 1;
        sock->ioctl( FIONBIO, &nonblocking );
#else
        sock->fcntl( F_SETFL, O_NONBLOCK );
#endif /* !WIN32 */

        unsigned int bufsize = 64 * 1024; // 64kbyte receive buffer, up from default of 8k
        sock->setopt( SOL_SOCKET, SO_RCVBUF, &bufsize, sizeof( bufsize ) );

        // New TCP connection, this must consume the socket.
        CreateNewConnection( sock, from.sin_addr.s_addr, ntohs( from.sin_port ) );
    }
}

thread_return_t Net::TcpServerBase::TcpServerLoop( void* arg )
{
    Net::TcpServerBase* tcps = reinterpret_cast< Net::TcpServerBase* >( arg );
    assert( tcps != NULL );

    THREAD_RETURN( tcps->TcpServerLoop() );
}

thread_return_t Net::TcpServerBase::TcpServerLoop()
{
#ifdef WIN32
    SetThreadPriority( GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL );
#endif

#ifndef WIN32
    sLog.Message( "Threading", "Starting TcpServerLoop with thread ID %d", pthread_self() );
#endif

    mMLoopRunning.Lock();

    Timer timer( LOOP_GRANULARITY );

    timer.Start();
    while( Process() )
        timer.Sleep();

    mMLoopRunning.Unlock();

#ifndef WIN32
    sLog.Message( "Threading", "Ending TcpServerLoop with thread ID %d", pthread_self() );
#endif

    THREAD_RETURN( NULL );
}
