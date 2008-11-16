/*  EVEmu: EVE Online Server Emulator

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.
  
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY except by those people which sell it, which
  are required to give you total support for your newly bought product;
  without even the implied warranty of MERCHANTABILITY or FITNESS FOR
  A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
	
  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "EvemuPCH.h"

#include <signal.h>

static void CatchSignal(int sig_num);
static bool InitSignalHandlers();

// wrapping up db starting
bool _DBStartup(std::string _hostname, uint32 _port, std::string _username, std::string _password, std::string _database);
bool _DBShutdown();

Database* Database_Generic;

static volatile bool RunLoops = true;

int main(int argc, char *argv[]) 
{
	printf("\nCopyright (C) 2006-2008 EVEmu Team. http://evemu.mmoforge.org\n");
	printf("\nThis program is free software; you can redistribute it and/or modify");
	printf("it under the terms of the GNU General Public License as published by");
	printf("the Free Software Foundation; version 2 of the License.");
	printf("For a copy of this license, see the COPYING file provided with this distribution.\n");
	Log.Line();
	
	sLog.String("Revision: %s", EVEMU_REVISION);
	sLog.String("Supported Client: %s, Version %.2f, Build %d, MachoNet %d", EVEProjectVersion, EVEVersionNumber, EVEBuildVersion, MachoNetVersion);

	printf( "The key combination <Ctrl-C> will safely shut down the server at any time.\n" );
	Log.Line();

#ifndef WIN32
	if(geteuid() == 0 || getegid() == 0)
	{
		Log.LargeErrorMessage( LARGERRORMESSAGE_WARNING, "You are running Evemu as root.", "This is not needed, and may be a possible security risk.", "It is advised to hit CTRL+C now and", "start as a non-privileged user.", NULL);
	}
#endif//WIN32

	/* start the 'threadpool', this needs to be in front of things that uses the 'threadpool' */
	ThreadPool.Startup();

	//it is important to do this before doing much of anything, in case they use it.
	Timer::SetCurrentTime();
	
	// Load server configuration
	sLog.String("Loading server configuration..");
	if (!EVEmuServerConfig::LoadConfig()) 
	{
		sLog.Error("EVEmu", "Loading server configuration failed.");
		return 1;
	}
	const EVEmuServerConfig *Config=EVEmuServerConfig::get();

	if(!load_log_settings(Config->LogSettingsFile.c_str()))
	{
		sLog.Warning("Warning: Unable to read %s (this file is optional)", Config->LogSettingsFile.c_str());
	}
	else
	{
		sLog.String("Log settings loaded from %s", Config->LogSettingsFile.c_str());
	}

	//open up the log file if specified.
	if(!Config->LogFile.empty()) {
		if(log_open_logfile(Config->LogFile.c_str())) {
			sLog.String("Opened log file %s", Config->LogFile.c_str());
		} else {
			sLog.String("Unable to open log file '%s', only logging to the screen now.", Config->LogFile.c_str());
		}
	}
	
	if(!PyRepString::LoadStringFile(Config->StringsFile.c_str())) {
		sLog.String("Unable to open %s, i need it to decode string table elements!", Config->StringsFile.c_str());
		return 1;
	}

	/*bool dbret = _DBStartup(Config->DatabaseHost,Config->DatabasePort, Config->DatabaseUsername, Config->DatabasePassword, Config->DatabaseDB);

	if(dbret == false)
	{
		return 1;
	}*/

	//connect to the database...
	DBcore db;
	{
		DBerror err;
		if(!db.Open(err, 
			Config->DatabaseHost.c_str(),
			Config->DatabaseUsername.c_str(),
			Config->DatabasePassword.c_str(),
			Config->DatabaseDB.c_str(),
			Config->DatabasePort)
		) {
			sLog.Error("Unable to connect to the database: %s", err.c_str());
			return 1;
		}
	}

	// Startup banner
	UNIXTIME = time(NULL);
	g_localTime = *localtime(&UNIXTIME);

	time_t curTime;

	uint32 realCurrTime, realPrevTime;
	realCurrTime = realPrevTime = getMSTime();

	// Socket loop timing
	uint32 start;
	uint32 diff;
	uint32 last_time = now();
	uint32 etime;
	uint32 next_printout = getMSTime(), next_send = getMSTime();

	// runnable thread to update server systems and sessions
	SpaceRunnable * sr = new SpaceRunnable();
	ThreadPool.ExecuteTask(sr);

	new SocketMgr;
	new SocketGarbageCollector;
	new Space;
	sSocketMgr.SpawnWorkerThreads();

	/* old 'evemu' spec stuff */
	EntityList entity_list(&db);
	ItemFactory item_factory(&db, &entity_list);

	/* global service manager */
	PyServiceMgr services(888444, &db, &entity_list, &item_factory, Config->CacheDirectory);
	services.RegisterService(new AgentMgrService(&services, &db));
	services.RegisterService(new MissionMgrService(&services, &db));
	services.RegisterService(new AccountService(&services, &db));
	services.RegisterService(new AlertService(&services));
	services.RegisterService(new AuthService(&services));
	services.RegisterService(new BillMgrService(&services, &db));
	services.RegisterService(new BookmarkService(&services));
	services.RegisterService(new CharacterService(&services, &db));
	services.RegisterService(new CharMgrService(&services, &db));
	services.RegisterService(new ConfigService(&services, &db));
	services.RegisterService(new LanguageService(&services, &db));
	services.RegisterService(new CorpMgrService(&services, &db));
	services.RegisterService(new CorpStationMgrService(&services, &db));
	services.RegisterService(new CorporationService(&services, &db));
	services.RegisterService(new CorpRegistryService(&services, &db));
	services.RegisterService(new DogmaIMService(&services, &db));
	services.RegisterService(new InvBrokerService(&services, &db));
	//services.RegisterService(services.lsc_service = new LSCService(&services, &db, &command_dispatcher));
	services.RegisterService(services.lsc_service = new LSCService(&services, &db, NULL));
	services.RegisterService(new LookupService(&services, &db));
	services.RegisterService(new VoiceMgrService(&services));
	services.RegisterService(new ShipService(&services, &db));
	services.RegisterService(new InsuranceService(&services, &db));
	services.RegisterService(new BeyonceService(&services, &db));
	services.RegisterService(new MapService(&services, &db));
	services.RegisterService(new OnlineStatusService(&services));
	services.RegisterService(new Standing2Service(&services, &db));
	services.RegisterService(new WarRegistryService(&services));
	services.RegisterService(new FactionWarMgrService(&services, &db));
	services.RegisterService(new StationService(&services, &db));
	services.RegisterService(new StationSvcService(&services, &db));
	services.RegisterService(new JumpCloneService(&services, &db));
	services.RegisterService(new KeeperService(&services, &db));
	services.RegisterService(new DungeonService(&services, &db));
	services.RegisterService(new SkillMgrService(&services, &db));
	services.RegisterService(new TutorialService(&services, &db));
	services.RegisterService(new PetitionerService(&services));
	//services.RegisterService(new SlashService(&services, &command_dispatcher));
	services.RegisterService(new MarketProxyService(&services, &db));
	services.RegisterService(new ContractMgrService(&services));
	services.RegisterService(new ReprocessingService(&services, &db));
	services.RegisterService(new FactoryService(&services, &db));
	services.RegisterService(new RamProxyService(&services, &db));
	services.RegisterService(new PosMgrService(&services, &db));
	services.RegisterService(new NetService(&services));

	services.GetCache()->PrimeCache();

	services.GetServiceDB()->SetServerOnlineStatus(true);

	/* end old 'evemu' spec stuff */

	static volatile bool m_stopEvent;

	/* these settings are 'hardcoded' for now */
	uint32 wsport = 26000;
	std::string host = "127.0.0.1";

#ifdef WIN32
	HANDLE hThread = GetCurrentThread();
#endif

	uint32 loopcounter = 0;

	// Create listener
	ListenSocket<EveClientSocket> * ls = new ListenSocket<EveClientSocket>(host.c_str(), wsport);
	bool listnersockcreate = ls->IsOpen();
#ifdef WIN32
	if( listnersockcreate == true )
		ThreadPool.ExecuteTask(ls);
#endif

	/* main thread loop */
	while( !m_stopEvent && listnersockcreate )
	{
		start = now();
		diff = start - last_time;

		if(! ((++loopcounter) % 10000) )		// 5mins
		{
			ThreadPool.ShowStats();
			ThreadPool.IntegrityCheck();
		}

		/* since time() is an expensive system call, we only update it once per server loop */
		curTime = time(NULL);
		if( UNIXTIME != curTime )
		{
			UNIXTIME = curTime;
			g_localTime = *localtime(&curTime);
		}

		// check for garbage sockets
		sSocketGarbageCollector.Update();

		/* UPDATE */
		last_time = now();
		etime = last_time - start;

		// do the stuff for thread sleeping
		if( 50 > etime )
		{
#ifdef WIN32
			WaitForSingleObject( hThread, 50 - etime );
#else
			Sleep( 50 - etime );
#endif
		}
	}
	return 0;

	//Start up the TCP server
	EVETCPServer tcps;
	
	char errbuf[TCPConnection_ErrorBufferSize];
	if (tcps.Open(Config->ServerPort, errbuf)) {
		_log(SERVER__INIT,"TCP listener started on port %d.", Config->ServerPort);
	} else {
		_log(SERVER__INIT_ERR,"Failed to start TCP listener on port %d:", Config->ServerPort);
		_log(SERVER__INIT_ERR,"        %s",errbuf);
		return 1;
	}
	
	//EntityList entity_list(&db);
	//ItemFactory item_factory(&db, &entity_list);

	

	//setup the command dispatcher
	//CommandDispatcher command_dispatcher(new CommandDB(&db), &services);
	//RegisterAllCommands(&command_dispatcher);

	/*                                                                              
     * Service creation and registration.
     *
     */
	_log(SERVER__INIT, "Creating services.");
	services.RegisterService(new AgentMgrService(&services, &db));
	services.RegisterService(new MissionMgrService(&services, &db));
	services.RegisterService(new AccountService(&services, &db));
	services.RegisterService(new AlertService(&services));
	services.RegisterService(new AuthService(&services));
	services.RegisterService(new BillMgrService(&services, &db));
	services.RegisterService(new BookmarkService(&services));
	services.RegisterService(new CharacterService(&services, &db));
	services.RegisterService(new CharMgrService(&services, &db));
	services.RegisterService(new ConfigService(&services, &db));
	services.RegisterService(new LanguageService(&services, &db));
	services.RegisterService(new CorpMgrService(&services, &db));
	services.RegisterService(new CorpStationMgrService(&services, &db));
	services.RegisterService(new CorporationService(&services, &db));
	services.RegisterService(new CorpRegistryService(&services, &db));
	services.RegisterService(new DogmaIMService(&services, &db));
	services.RegisterService(new InvBrokerService(&services, &db));
	//services.RegisterService(services.lsc_service = new LSCService(&services, &db, &command_dispatcher));
	services.RegisterService(new LookupService(&services, &db));
	services.RegisterService(new VoiceMgrService(&services));
	services.RegisterService(new ShipService(&services, &db));
	services.RegisterService(new InsuranceService(&services, &db));
	services.RegisterService(new BeyonceService(&services, &db));
	services.RegisterService(new MapService(&services, &db));
	services.RegisterService(new OnlineStatusService(&services));
	services.RegisterService(new Standing2Service(&services, &db));
	services.RegisterService(new WarRegistryService(&services));
	services.RegisterService(new FactionWarMgrService(&services, &db));
	services.RegisterService(new StationService(&services, &db));
	services.RegisterService(new StationSvcService(&services, &db));
	services.RegisterService(new JumpCloneService(&services, &db));
	services.RegisterService(new KeeperService(&services, &db));
	services.RegisterService(new DungeonService(&services, &db));
	services.RegisterService(new SkillMgrService(&services, &db));
	services.RegisterService(new TutorialService(&services, &db));
	services.RegisterService(new PetitionerService(&services));
	//services.RegisterService(new SlashService(&services, &command_dispatcher));
	services.RegisterService(new MarketProxyService(&services, &db));
	services.RegisterService(new ContractMgrService(&services));
	services.RegisterService(new ReprocessingService(&services, &db));
	services.RegisterService(new FactoryService(&services, &db));
	services.RegisterService(new RamProxyService(&services, &db));
	services.RegisterService(new PosMgrService(&services, &db));
	services.RegisterService(new NetService(&services));
	
	_log(SERVER__INIT, "Priming cached objects");
//#ifndef WIN32
//#warning CACHABLES DISABLED!
	services.GetCache()->PrimeCache();
//#endif

	// johnsus - serverStartTime mod
	services.GetServiceDB()->SetServerOnlineStatus(true);

	_log(SERVER__INIT, "Init done.");

	while(argc > 1) {
		uint32 sys = strtoul(argv[1], NULL, 0);
		if(sys > 0) {
			_log(SERVER__INIT, "Command line tells us to boot system %lu", sys);
			entity_list.FindOrBootSystem(sys);
		}
		argc--;
		argv++;
	}

	
	/*
	 * THE MAIN LOOP
	 *
	 * Everything except IO should happen in this loop, in this thread context.
	 *
	 */
	if(!InitSignalHandlers())	//do not set these up until the main loop is about to start, else we cannot abort init.
		return 1;
	EVETCPConnection *tcpc;
	while(RunLoops) {
		Timer::SetCurrentTime();
		
		//check for timeouts in other threads
		//timeout_manager.CheckTimeouts();

		while ((tcpc = tcps.NewQueuePop())) 
		{
			struct in_addr in;
			in.s_addr = tcpc->GetrIP();
			_log(SERVER__CLIENTS, "New TCP connection from %s:%d", inet_ntoa(in),tcpc->GetrPort());
			//Client *c = new Client(&services, &tcpc);
					
			//entity_list.Add(&c);
		}
		
		entity_list.Process();
		services.Process();
		
		Sleep(3);	//this should be a parameter
	}

	_log(SERVER__SHUTDOWN,"main loop stopped");
	_log(SERVER__SHUTDOWN,"TCP listener stopped.");
	tcps.Close();
	
	// johnsus - serverStartTime mod
	services.GetServiceDB()->SetServerOnlineStatus(false);

	//TODO: properly free physics driver

	return 0;
}

static bool InitSignalHandlers() {
	if (signal(SIGINT, CatchSignal) == SIG_ERR)	{
		_log(SERVER__INIT_ERR, "Could not set signal handler");
		return false;
	}
	if (signal(SIGTERM, CatchSignal) == SIG_ERR)	{
		_log(SERVER__INIT_ERR, "Could not set signal handler");
		return false;
	}
	#ifndef WIN32
	if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)	{
		_log(SERVER__INIT_ERR, "Could not set signal handler");
		return false;
	}
	#endif
	return true;
}

static void CatchSignal(int sig_num) {
	_log(SERVER__SHUTDOWN,"Caught signal %d",sig_num);
	RunLoops = false;
}

/* internal db start up wrapper... */
bool _DBStartup(std::string _hostname, uint32 _port, std::string _username, std::string _password, std::string _database)
{
	std::string hostname = _hostname;
	std::string username = _username;
	std::string password = _password;
	std::string database = _database;

	int port = _port;
	int type = 1; // 1 is mysql
	int connectionCount = 3; // make this configurable

	Database_Generic = Database::CreateDatabaseInterface(type);

	// Initialize it
	if( !GenericDatabase.Initialize(hostname.c_str(), (unsigned int)port, username.c_str(),
	   password.c_str(), database.c_str(), connectionCount, 16384 ) )
	{
		Log.Error( "sql","Main database initialization failed. Exiting." );
		return false;
	}

	return true;
}

bool _DBShutdown()
{
	SafeDelete(Database_Generic);
	return true;
}