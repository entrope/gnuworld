/* 
 *  cservice.cc 
 *  Overall control client, general dumping ground for 'cservice'
 *  related methods.
 */

#warning --
#warning --
#warning -- If you have not yet ran "update.delete.sql" in doc/ on your database,
#warning -- do so now.
#warning -- 
#warning --	

#include	<vector.h>
#include	<iostream.h>
#include	<strstream.h>
#include	<string.h>

#include	<ctime>
#include	<cstdlib>
 
#include	"client.h" 
#include  	"cservice.h"
#include	"EConfig.h"
#include	"events.h"
#include	"ip.h"
#include	"Network.h"
#include	"StringTokenizer.h"
#include	"misc.h"
#include	"ELog.h" 
#include	"libpq++.h"
#include	"constants.h"
#include	"networkData.h"
#include	"levels.h"
#include	"cservice_config.h"
#include	"match.h"

namespace gnuworld
{

using std::vector ;
using std::endl ;
using std::strstream ;
using std::ends ;
using std::string ; 

/*
 *  Exported function used by moduleLoader to gain an
 *  instance of this module.
 */

extern "C"
{
  xClient* _gnuwinit(const string& args)
  { 
    return new cservice( args );
  }

} 
 
bool cservice::RegisterCommand( Command* newComm )
{ 
UnRegisterCommand( newComm->getName() ) ;
return commandMap.insert( pairType( newComm->getName(), newComm ) ).second ;
}

bool cservice::UnRegisterCommand( const string& commName )
{
commandMapType::iterator ptr = commandMap.find( commName ) ;
if( ptr == commandMap.end() )
        {
        return false ;
        }
delete ptr->second ;
commandMap.erase( ptr ) ;
return true ;
}

void cservice::ImplementServer( xServer* theServer )
{
for( commandMapType::iterator ptr = commandMap.begin() ;
	ptr != commandMap.end() ; ++ptr )
        {
        ptr->second->setServer( theServer ) ;
        }
	 
/* Attempt to register our interest in recieving NOTIFY events. */
if (SQLDb->ExecCommandOk("LISTEN channels_u; LISTEN users_u; LISTEN levels_u;"))
	{
	elog	<< "cmaster::ImplementServer> Successfully registered "
		<< "LISTEN event for Db updates."
		<< endl;

	// Start the Db checker timer rolling.
	time_t theTime = time(NULL) + connectCheckFreq;
	dBconnection_timerID = theServer->RegisterTimer(theTime, this, NULL);

	// Start the Db update/Reop timer rolling.
	theTime = time(NULL) + updateInterval;
	update_timerID = theServer->RegisterTimer(theTime, this, NULL);

	// Start the ban suspend/expire timer rolling.
	theTime = time(NULL) + expireInterval;
	expire_timerID = theServer->RegisterTimer(theTime, this, NULL);

	// Start the cache expire timer rolling.
	theTime = time(NULL) + cacheInterval;
	cache_timerID = theServer->RegisterTimer(theTime, this, NULL); 

	// Start the pending chan timer rolling.
	theTime = time(NULL) + pendingChanPeriod;
	pending_timerID = theServer->RegisterTimer(theTime, this, NULL); 

	}
else 
	{
	elog	<< "cmaster::ImplementServer> PostgreSQL error while "
		<< "attempting to register LISTEN event: "
		<< SQLDb->ErrorMessage()
		<< endl;
	}

if (SQLDb->Exec("SELECT now()::abstime::int4;") == PGRES_TUPLES_OK) 
	{ 
	// Set our "Last Refresh" timers to the current database system time.
	time_t serverTime = atoi(SQLDb->GetValue(0,0));
	lastChannelRefresh = serverTime;
	lastUserRefresh = serverTime;
	lastLevelRefresh = serverTime;
	lastBanRefresh = serverTime;

	/* 
	 * Calculate the current time offset from the DB server.
	 * We always want to talk in DB server time. 
	 */ 

	dbTimeOffset = serverTime - ::time(NULL);
	elog	<< "cservice::ImplementServer> Current DB server time: "
		<< currentTime()
		<< endl; 
	}
else
	{
 	elog	<< "Unable to retrieve time from postgres server!"
		<< endl;
	::exit(0);
	}
 
// Register our interest in recieving some Network events from gnuworld.

theServer->RegisterEvent( EVT_KILL, this );
theServer->RegisterEvent( EVT_QUIT, this );
theServer->RegisterEvent( EVT_NICK, this );

xClient::ImplementServer( theServer ) ;
}

cservice::cservice(const string& args)
 : xClient( args )
{ 
/*
 *  Register command handlers.
 */
 
RegisterCommand(new SHOWCOMMANDSCommand(this, "SHOWCOMMANDS", "[#channel]", 3));
RegisterCommand(new LOGINCommand(this, "LOGIN", "<username> <password>", 10)); 
RegisterCommand(new ACCESSCommand(this, "ACCESS", "[channel] [username] [-min n] [-max n] [-op] [-voice] [-none] [-modif [mask]]", 5));
RegisterCommand(new CHANINFOCommand(this, "CHANINFO", "<#channel>", 3)); 
RegisterCommand(new ISREGCommand(this, "ISREG", "<#channel>", 4)); 
RegisterCommand(new VERIFYCommand(this, "VERIFY", "<nick>", 3));
RegisterCommand(new SEARCHCommand(this, "SEARCH", "<keywords>", 5));
RegisterCommand(new MOTDCommand(this, "MOTD", "", 4));
RegisterCommand(new HELPCommand(this, "HELP", "[command]", 4));
RegisterCommand(new RANDOMCommand(this, "RANDOM", "", 4));
RegisterCommand(new SHOWIGNORECommand(this, "SHOWIGNORE", "", 3));

RegisterCommand(new OPCommand(this, "OP", "<#channel> [nick] [nick] ..", 3));
RegisterCommand(new DEOPCommand(this, "DEOP", "<#channel> [nick] [nick] ..", 3));
RegisterCommand(new VOICECommand(this, "VOICE", "<#channel> [nick] [nick] ..", 3));
RegisterCommand(new DEVOICECommand(this, "DEVOICE", "<#channel> [nick] [nick] ..", 3));
RegisterCommand(new ADDUSERCommand(this, "ADDUSER", "<#channel> <username> <access>", 8));
RegisterCommand(new REMUSERCommand(this, "REMUSER", "<#channel> <username>", 4));
RegisterCommand(new MODINFOCommand(this, "MODINFO", "<#channel> [ACCESS <username> <level>] [AUTOMODE <username> <NONE|OP|VOICE>]", 6));
RegisterCommand(new SETCommand(this, "SET", "[#channel] <variable> <value> or, SET <invisible> <ON|OFF>", 6));
RegisterCommand(new INVITECommand(this, "INVITE", "<#channel>", 2));
RegisterCommand(new TOPICCommand(this, "TOPIC", "<#channel> <topic>", 4));
RegisterCommand(new CHANINFOCommand(this, "CHANINFO", "<#channel>", 3));
RegisterCommand(new CHANINFOCommand(this, "INFO", "<username>", 3));
RegisterCommand(new BANLISTCommand(this, "BANLIST", "<#channel>", 3));
RegisterCommand(new KICKCommand(this, "KICK", "<#channel> <nick> <reason>", 4));
RegisterCommand(new STATUSCommand(this, "STATUS", "<#channel>", 4));
RegisterCommand(new SUSPENDCommand(this, "SUSPEND", "<#channel> <nick> [duration]", 5));
RegisterCommand(new UNSUSPENDCommand(this, "UNSUSPEND", "<#channel> <nick>", 5));
RegisterCommand(new BANCommand(this, "BAN", "<#channel> <nick | *!*user@*.host> [duration] [level] [reason]", 5));
RegisterCommand(new UNBANCommand(this, "UNBAN", "<#channel> <*!*user@*.host>", 5));
RegisterCommand(new LBANLISTCommand(this, "LBANLIST", "<#channel> <banmask>", 5));
RegisterCommand(new NEWPASSCommand(this, "NEWPASS", "<new passphrase>", 8)); 
RegisterCommand(new JOINCommand(this, "JOIN", "<#channel>", 8));
RegisterCommand(new PARTCommand(this, "PART", "<#channel>", 8));
RegisterCommand(new OPERJOINCommand(this, "OPERJOIN", "<#channel>", 8));
RegisterCommand(new OPERPARTCommand(this, "OPERPART", "<#channel>", 8));
RegisterCommand(new CLEARMODECommand(this, "CLEARMODE", "<#channel>", 4));

RegisterCommand(new REMIGNORECommand(this, "REMIGNORE", "<mask>", 5));
RegisterCommand(new REGISTERCommand(this, "REGISTER", "<#channel> <username>", 8));
RegisterCommand(new REMOVEALLCommand(this, "REMOVEALL", "<#channel>", 15));
RegisterCommand(new PURGECommand(this, "PURGE", "<#channel> <reason>", 8));
RegisterCommand(new FORCECommand(this, "FORCE", "<#channel>", 8));
RegisterCommand(new UNFORCECommand(this, "UNFORCE", "<#channel>", 8));
RegisterCommand(new SERVNOTICECommand(this, "SERVNOTICE", "<#channel> <text>", 5));
RegisterCommand(new SAYCommand(this, "SAY", "<#channel> <text>", 5));
RegisterCommand(new QUOTECommand(this, "QUOTE", "<text>", 5)); 
RegisterCommand(new REHASHCommand(this, "REHASH", "[translations]", 5));
 
//-- Load in our cservice configuration file. 
cserviceConfig = new (nothrow) EConfig( args ) ;
assert( cserviceConfig != 0 ) ;

confSqlHost = cserviceConfig->Require( "sql_host" )->second;
confSqlDb = cserviceConfig->Require( "sql_db" )->second;
confSqlPort = cserviceConfig->Require( "sql_port" )->second;
confSqlUser = cserviceConfig->Require( "sql_user" )->second;

string Query = "host=" + confSqlHost + " dbname=" + confSqlDb + " port=" + confSqlPort + " user=" + confSqlUser;

elog	<< "cmaster::cmaster> Attempting to connect to "
	<< confSqlHost
	<< "; Database: "
	<< confSqlDb
	<< endl;
 
SQLDb = new (nothrow) cmDatabase( Query.c_str() ) ;
assert( SQLDb != 0 ) ;

//-- Make sure we connected to the SQL database; if we didn't we exit entirely.
if (SQLDb->ConnectionBad ())
	{
	elog	<< "cmaster::cmaster> Unable to connect to SQL server."
		<< endl 
		<< "cmaster::cmaster> PostgreSQL error message: "
		<< SQLDb->ErrorMessage()
		<< endl ;

	::exit( 0 ) ;
	}
else
	{
	elog	<< "cmaster::cmaster> Connection established to SQL server. "
		<< "Backend PID: " << SQLDb->getPID()
		<< endl ;
	}

// The program will exit if these variables are not defined in the
// configuration file.
relayChan = cserviceConfig->Require( "relay_channel" )->second ;
debugChan = cserviceConfig->Require( "debug_channel" )->second ;
pendingPageURL = cserviceConfig->Require( "pending_page_url" )->second ;
updateInterval = atoi((cserviceConfig->Require( "update_interval" )->second).c_str());
expireInterval = atoi((cserviceConfig->Require( "expire_interval" )->second).c_str());
cacheInterval = atoi((cserviceConfig->Require( "cache_interval" )->second).c_str());
input_flood = atoi((cserviceConfig->Require( "input_flood" )->second).c_str());
output_flood = atoi((cserviceConfig->Require( "output_flood" )->second).c_str());
flood_duration = atoi((cserviceConfig->Require( "flood_duration" )->second).c_str());
topic_duration = atoi((cserviceConfig->Require( "topic_duration" )->second).c_str());
pendingChanPeriod = atoi((cserviceConfig->Require( "pending_duration" )->second).c_str());
connectCheckFreq = atoi((cserviceConfig->Require( "connection_check_frequency" )->second).c_str());
connectRetry = atoi((cserviceConfig->Require( "connection_retry_total" )->second).c_str());
 
userHits = 0;
userCacheHits = 0;
channelHits = 0;
channelCacheHits = 0;
levelHits = 0;
levelCacheHits = 0;
banHits = 0;
banCacheHits = 0; 
dbErrors = 0;
joinCount = 0;
connectRetries = 0;

// Load our translation tables.
loadTranslationTable(); 

}

cservice::~cservice()
{
delete cserviceConfig ;	cserviceConfig = 0 ;
delete SQLDb ; SQLDb = 0 ;

for( commandMapType::iterator ptr = commandMap.begin() ;
	ptr != commandMap.end() ; ++ptr )
	{
	delete ptr->second ;
	}
commandMap.clear() ;
}

int cservice::BurstChannels()
{
/*
 *   Need to join every channel with AUTOJOIN set. (But not * ;))
 *   Various other things must be done, such as setting the topic if AutoTopic
 *   is on, gaining ops if AlwaysOp is on, and so forth.
 */ 
strstream theQuery;
theQuery	<< "SELECT " << sql::channel_fields
		<< " FROM channels WHERE lower(name) <> '*' AND "
		<< "registered_ts <> 0"
		<< ends;

#ifdef LOG_SQL
	elog	<< "cmaster::BurstChannels> "
		<< theQuery.str()
		<< endl; 
#endif

ExecStatusType status = SQLDb->Exec(theQuery.str()) ;
delete[] theQuery.str() ;

if( PGRES_TUPLES_OK == status )
	{
	for (int i = 0 ; i < SQLDb->Tuples(); i++)
		{ 
 		/* Add this information to the channel cache. */

		sqlChannel* newChan = new (nothrow) sqlChannel(SQLDb);
		assert( newChan != 0 ) ;

		newChan->setAllMembers(i);
		sqlChannelCache.insert(sqlChannelHashType::value_type(newChan->getName(), newChan));
		sqlChannelIDCache.insert(sqlChannelIDHashType::value_type(newChan->getID(), newChan));

		/*
		 *  Check the auto-join flag is set, if so - join. :)
		 */ 

		if (newChan->getFlag(sqlChannel::F_AUTOJOIN))
			{
			MyUplink->JoinChannel( this, 
				newChan->getName(),
				newChan->getChannelMode(),
				newChan->getChannelTS(),
				true ); 
			MyUplink->RegisterChannelEvent( newChan->getName(),
				this ) ;
			newChan->setInChan(true);
			joinCount++;

			/* If neccessary, set the auto topic. */
			if (newChan->getFlag(sqlChannel::F_AUTOTOPIC))
			{
				doAutoTopic(newChan);
			}
 
			}

 
		} // for()
	} // if()

	loadPendingChannelList(); 

return xClient::BurstChannels();
}
	
int cservice::OnConnect()
{
return 0;
}

unsigned short cservice::getFloodPoints(iClient* theClient)
{
/*
 * This function gets an iClient's flood points. 
 */

networkData* tmpData =
	static_cast< networkData* >( theClient->getCustomData(this) ) ;

if(!tmpData)
	{
	return 0;
	}
//assert(tmpData != NULL);
 
return tmpData->flood_points;
}

void cservice::setFloodPoints(iClient* theClient, unsigned short amount)
{ 
networkData* tmpData =
	static_cast< networkData* >( theClient->getCustomData(this) ) ;

if (!tmpData)
	{
	return;
	}
//assert(tmpData != NULL);

tmpData->flood_points = amount; 
}	

/**
 * This method sets a timestamp for when we last recieved
 * a message from this iClient.
 */
void cservice::setLastRecieved(iClient* theClient, time_t last_recieved)
{
networkData* tmpData =
	static_cast< networkData* >( theClient->getCustomData(this) ) ;

if(!tmpData)
	{
	return;
	}
//assert(tmpData != NULL);

tmpData->messageTime = last_recieved;
}	

/**
 * This method gets a timestamp from this iClient
 * for flood control.
 */
time_t cservice::getLastRecieved(iClient* theClient)
{
networkData* tmpData =
	static_cast< networkData* >( theClient->getCustomData(this) ) ;

if(!tmpData)
	{
	return 0;
	}
//assert(tmpData != NULL);

return tmpData->messageTime;
}	

bool cservice::isIgnored(iClient* theClient)
{
 
networkData* tmpData =
	static_cast< networkData* >( theClient->getCustomData(this) ) ;

if(!tmpData)
	{
	return 0;
	} 
 
return tmpData->ignored;
}

void cservice::setIgnored(iClient* theClient, bool _ignored)
{
 
networkData* tmpData =
	static_cast< networkData* >( theClient->getCustomData(this) ) ;

if(!tmpData)
	{
	return;
	} 
 
tmpData->ignored = _ignored;
}
 
bool cservice::hasFlooded(iClient* theClient, const string& type)
{
if( (getLastRecieved(theClient) + flood_duration) <= ::time(NULL) )
	{
	/*
	 *  Reset a few things, they're out of the flood period now.
	 *  Or, this is the first message from them.
	 */

	setFloodPoints(theClient, 0);
	setOutputTotal(theClient, 0);
	setLastRecieved(theClient, ::time(NULL));
	}
else 
	{
	/*
	 *  Inside the flood period, check their points..
	 */

	if(getFloodPoints(theClient) > input_flood)
		{
		/*
		 *  Check admin access, if present then
		 *  don't trigger.
		 */

		sqlUser* theUser = isAuthed(theClient, false);
		if (theUser && getAdminAccessLevel(theUser))
			{
			return false;
			}

		// Bad boy!
		setFloodPoints(theClient, 0);
		setLastRecieved(theClient, ::time(NULL)); 
		Notice(theClient,
			"Flood me will you? I'm not going to listen to "
			"you anymore.");
		
		// Send a silence numeric target, and mask to ignore
		// messages from this user.
		string silenceMask = string( "*!*" )
			+ theClient->getUserName()
			+ "@"
			+ theClient->getInsecureHost();

		strstream s;
		s	<< getCharYYXXX() 
			<< " SILENCE " 
			<< theClient->getCharYYXXX() 
			<< " " 
			<< silenceMask
			<< ends; 
		Write( s );
		delete[] s.str();

		time_t expireTime = currentTime() + 3600; 
		silenceList.insert(silenceListType::value_type(silenceMask,
			make_pair(expireTime, theClient->getCharYYXXX())));

		setIgnored(theClient, true);

		string floodComment; 
		StringTokenizer st(type) ;

		if( st.size() >= 2 ) 
		{
			floodComment = st[0] + ' ' + st[1];
		} else {
			floodComment = st[0];
		}
 
		logAdminMessage("MSG-FLOOD from %s (%s)",
			theClient->getNickUserHost().c_str(),
			floodComment.c_str());
		return true;
		} // if()
	} // else()

return false;
} 

void cservice::setOutputTotal(const iClient* theClient, unsigned int count)
{
/*
 * This function sets the current output total in bytes
 * for this client.
 */

networkData* tmpData =
	static_cast< networkData* >( theClient->getCustomData(this) );

if (!tmpData)
	{
	return;
	}
//assert(tmpData != NULL);

tmpData->outputCount = count;
} 

unsigned int cservice::getOutputTotal(const iClient* theClient)
{
networkData* tmpData =
	static_cast< networkData* >( theClient->getCustomData(this) );

if (!tmpData)
	{
	return 0;
	}
//assert(tmpData != NULL);

return tmpData->outputCount;
}	

bool cservice::hasOutputFlooded(iClient* theClient)
{

if( (getLastRecieved(theClient) + flood_duration) <= ::time(NULL) )
	{
	/*
	 *  Reset a few things, they're out of the flood period now.
	 *  Or, this is the first message from them.
	 */

	setOutputTotal(theClient, 0);
	setFloodPoints(theClient, 0);
	setLastRecieved(theClient, ::time(NULL));
	}
else 
	{
	/*
	 *  Inside the flood period, check their output count.
	 */

	if(getOutputTotal(theClient) > output_flood)
		{
		/*
		 *  Check admin access, if present then
		 *  don't trigger.
		 */

		sqlUser* theUser = isAuthed(theClient, false);
		if (theUser && getAdminAccessLevel(theUser))
			{
			return false;
 			}

		setOutputTotal(theClient, 0);
		setLastRecieved(theClient, ::time(NULL)); 
		Notice(theClient, "I think I've sent you a little "
			"too much data, I'm going to ignore you "
			"for a while."); 

		// Send a silence numeric target, and mask to ignore
		// messages from this user.
		string silenceMask = string( "*!*" )
			+ theClient->getUserName()
			+ "@"
			+ theClient->getInsecureHost();

		strstream s;
		s	<< getCharYYXXX()
			<< " SILENCE "
			<< theClient->getCharYYXXX()
			<< " "
			<< silenceMask
			<< ends; 
		Write( s );
		delete[] s.str();

		time_t expireTime = currentTime() + 3600;

		silenceList.insert(silenceListType::value_type(silenceMask,
			make_pair(expireTime, theClient->getCharYYXXX()))); 

		setIgnored(theClient, true);

		logAdminMessage("OUTPUT-FLOOD from %s", theClient->getNickUserHost().c_str());
		return true;
		}
	}

return false;
}	
 
int cservice::OnPrivateMessage( iClient* theClient, const string& Message,
	bool secure )
{ 
/*
 * Private message handler. Pass off the command to the relevant
 * handler.
 */

if (isIgnored(theClient)) return 0;

StringTokenizer st( Message ) ;
if( st.empty() )
	{
	Notice( theClient, "Incomplete command");
	return 0 ;
	}

/*
 * Do flood checking - admins at 750 or above are excempt.
 * N.B: Only check that *after* someone has flooded ;)
 */
const string Command = string_upper( st[ 0 ] ) ;

/*
 *  Just quickly, abort if someone tries to LOGIN or NEWPASS
 *  unsecurely.
 */

if (!secure && ((Command == "LOGIN") || (Command == "NEWPASS")) )
	{
	Notice(theClient, "To use %s, you must /msg %s@%s",
		Command.c_str(), nickName.c_str(), getUplinkName().c_str());
	return false;
	}
 
/* Attempt to find a handler for this method. */

commandMapType::iterator commHandler = commandMap.find( Command ) ;
if( commHandler == commandMap.end() )
	{
	/* Don't reply to unknown commands, but add to their flood 
	 * total :)
	 */
	if (hasFlooded(theClient, "PRIVMSG"))
		{
		return false;
		}

	if (hasOutputFlooded(theClient))
		{
		return false;
		}
 
	// Why use 3 here?  Should be in config file
	// (Violation of "rule of numbers")
	setFloodPoints(theClient, getFloodPoints(theClient) + 3); 
	}
else
	{
	/*
	 *  Check users flood limit, if exceeded..
	 */

	if (hasFlooded(theClient, Message))
		{
		return false;
		}

	if (hasOutputFlooded(theClient))
		{
		return false;
		}

	setFloodPoints(theClient, getFloodPoints(theClient)
		+ commHandler->second->getFloodPoints() ); 

	commHandler->second->Exec( theClient, Message ) ;
	}

return xClient::OnPrivateMessage( theClient, Message ) ;
}
 
int cservice::OnCTCP( iClient* theClient, const string& CTCP,
                    const string& Message, bool Secure)
{ 
/**
 * CTCP hander. Deal with PING, GENDER and VERSION. 
 * Hit users with a '5' flood score for CTCP's.
 * This should be in the config file.
 */

if (isIgnored(theClient)) return 0;

if (hasFlooded(theClient, "CTCP"))
	{
	return false;
	}

setFloodPoints(theClient, getFloodPoints(theClient) + 5 ); 

StringTokenizer st( CTCP ) ;
if( st.empty() )
	{
	return 0;
	}

const string Command = string_upper(st[0]);

if(Command == "PING" || Command=="ECHO")
	{
	xClient::DoCTCP(theClient, CTCP, Message);
	}
else if(Command == "GENDER")
	{
	xClient::DoCTCP(theClient, CTCP,
		"Tried to be a man again - there was a slip - now I'm an IT");
	}
else if(Command == "VERSION")
	{
	xClient::DoCTCP(theClient, CTCP,
		"Undernet P10 Channel Services Version 2 ["
		__DATE__ " " __TIME__
		"] Release 1.0pl7a-devel");
	}
else if(Command == "PROBLEM?")
	{
	xClient::DoCTCP(theClient, CTCP.c_str(), "Blame Gte!");
	} 
else if(Command == "WHAT_YOU_SAY?")
	{
	xClient::DoCTCP(theClient, CTCP.c_str(), "Move 'Zig'!");
	} 
else if(Command == "SOUND")
	{
	xClient::DoCTCP(theClient, CTCP.c_str(), "I'm deaf, remember?");
	}
else if(Command == "DCC")
	{
	xClient::DoCTCP(theClient, CTCP.c_str(), "REJECT");
	}
else if(Command == "FLOOD")
	/* Ignored */;
else if(Command == "PAGE")
	{
	xClient::DoCTCP(theClient, CTCP.c_str(), "I'm always here, no need to page");
	}
else if(Command == "USERINFO")
	{
	xClient::DoCTCP(theClient, CTCP.c_str(), "I'm a user, not an abuser");
	}
else if(Command == "TIME")
	{
	xClient::DoCTCP(theClient, CTCP.c_str(), "Time you got a watch?");
	}
else 
	{
	xClient::DoCTCP(theClient, "ERRMSG", CTCP.c_str());
	}

return true;
} 

/**
 * Check to see if this user is 'forced' onto this channel.
 */
unsigned short cservice::isForced(sqlChannel* theChan, sqlUser* theUser)
{
if (!theChan->forceMap.empty())
	{ 
	sqlChannel::forceMapType::iterator ptr = theChan->forceMap.find(theUser->getID()); 
	if(ptr != theChan->forceMap.end()) 
		{
		/* So they do, return their forced level. */
		return ptr->second.first;
		}
	}

return 0;
}
 
/**
 *  Confirms a user is logged in by returning a pointer to
 *  the sqlUser record. 
 *  If 'alert' is true, send a notice to the user informing
 *  them that they must be logged in.
 */ 
sqlUser* cservice::isAuthed(iClient* theClient, bool alert)
{
networkData* tmpData =
	static_cast< networkData* >( theClient->getCustomData(this) ) ;

if(!tmpData)
	{
	return 0;
	}

//assert( tmpData != 0 ) ;

sqlUser* theUser = tmpData->currentUser;

if( theUser )
	{
	return theUser;
	}

if( alert )
	{
	Notice(theClient,
		"Sorry, You must be logged in to use this command.");
	}
return 0;
}

/**
 *  Locates a cservice user record by 'id', the username of this user. 
 */
sqlUser* cservice::getUserRecord(const string& id)
{
/*
 *  Check if this is a lookup by nick
 */
if (id[0]=='=') 
	{
	const char* theNick = id.c_str();
	// Skip the '=' 
	++theNick; 

	iClient *client = Network->findNick(theNick);
	if (client) return isAuthed(client,false);

	return 0;
	}
/*
 *  Check if this record is already in the cache.
 */

sqlUserHashType::iterator ptr = sqlUserCache.find(id);
if(ptr != sqlUserCache.end())
	{
	// Found something!
	#ifdef LOG_CACHE_HITS
		elog	<< "cmaster::getUserRecord> Cache hit for "
			<< id
			<< endl;
	#endif

	ptr->second->setLastUsed(currentTime());
	userCacheHits++;
	return ptr->second ;
	}

/*
 *  We didn't find anything in the cache, fetch the data from
 *  the backend and create a new sqlUser object.
 */

sqlUser* theUser = new (nothrow) sqlUser(SQLDb);
assert( theUser != 0 ) ;

if (theUser->loadData(id)) 
	{ 
 	sqlUserCache.insert(sqlUserHashType::value_type(id, theUser));

	#ifdef LOG_SQL
		elog	<< "cmaster::getUserRecord> There are "
			<< sqlUserCache.size()
			<< " elements in the cache."
		<< endl;
	#endif

	userHits++;

	// Return the new user to the caller
	theUser->setLastUsed(currentTime());
	return theUser;
	}
else
	{
	delete theUser ;
	}

return 0;
}	

/**
 *  Returns a vector of sqlBan's for a given channel.
 *  If not found in the cache, load from the database.
 */
vector<sqlBan*>* cservice::getBanRecords(sqlChannel* theChan)
{

sqlBanHashType::iterator ptr = sqlBanCache.find(theChan->getID());
if(ptr != sqlBanCache.end())
	{
	// Found something!
	#ifdef LOG_CACHE_HITS
		elog	<< "cmaster::getBanRecords> Cache hit for "
			<< theChan->getID()
			<< endl;
	#endif

	banCacheHits++;
	return ptr->second ;
	}

/*
 *  We didn't find anything in the cache, fetch the data from
 *  the backend and create a new vector<sqlban*> object.
 *  If we find no bans.. return a new blank container.
 */ 

vector<sqlBan*>* banList = new (nothrow) vector<sqlBan*>;
assert( banList != 0 ) ;

/*
 * Execute some SQL to get all bans relating to this channel.
 */

strstream theQuery;
theQuery	<< "SELECT " << sql::ban_fields
		<< " FROM bans WHERE channel_id = "
		<< theChan->getID()
		<< ends;

#ifdef LOG_SQL
	elog		<< "cmaster::getBanRecords> "
			<< theQuery.str()
			<< endl; 
#endif

ExecStatusType status = SQLDb->Exec(theQuery.str()) ;
delete[] theQuery.str() ;

if( PGRES_TUPLES_OK == status )
	{ 
	for (int i = 0 ; i < SQLDb->Tuples(); i++)
		{ 
		sqlBan* newBan = new (nothrow) sqlBan(SQLDb);
		assert( newBan != 0 ) ;

		newBan->setAllMembers(i);
		banList->push_back(newBan);
		elog	<< "cservice::getBanRecords> Loaded ban for : "
			<< newBan->getBanMask()
			<< endl;
		}
	}

 /* Insert into the cache - even if its empty */

sqlBanCache.insert(sqlBanHashType::value_type(theChan->getID(), banList));

#ifdef LOG_CACHE_HITS
	elog	<< "cmaster::getBanRecords> There are "
		<< sqlBanCache.size()
		<< " elements in the cache."
		<< endl;
#endif

banHits++;

return banList;
}	
 
/**
 *  Locates a cservice user record by 'id', the channel name. 
 */
sqlChannel* cservice::getChannelRecord(const string& id)
{ 

/*
 *  Check if this record is already in the cache.
 */
 
sqlChannelHashType::iterator ptr = sqlChannelCache.find(id);
if(ptr != sqlChannelCache.end())
	{
	// Found something!
	#ifdef LOG_CACHE_HITS
		elog	<< "cmaster::getChannelRecord> Cache hit for "
			<< id
			<< endl;
	#endif

	channelCacheHits++; 
	ptr->second->setLastUsed(currentTime());

	// Return the channel to the caller
	return ptr->second ;
	} 

/*
 *  We didn't find anything in the cache, fetch the data from
 *  the backend and create a new sqlUser object.
 */

sqlChannel* theChan = new (nothrow) sqlChannel(SQLDb);
assert( theChan != 0 ) ;

if (theChan->loadData(id))
	{
 	sqlChannelCache.insert(sqlChannelHashType::value_type(id, theChan));

	#ifdef LOG_CACHE_HITS
		elog	<< "cmaster::getChannelRecord> There are "
			<< sqlChannelCache.size()
			<< " elements in the cache."
			<< endl;
	#endif

	channelHits++;
	theChan->setLastUsed(currentTime());

	// Return the channel to the caller
	return theChan;
	}

delete theChan;
return 0;
} 

/**
 *  Loads a channel from the cache by 'id'.
 */
sqlChannel* cservice::getChannelRecord(int id)
{ 

/*
 *  Check if this record is already in the cache.
 */
 
sqlChannelIDHashType::iterator ptr = sqlChannelIDCache.find(id);
if(ptr != sqlChannelIDCache.end())
	{
	// Found something!
	#ifdef LOG_CACHE_HITS
		elog	<< "cmaster::getChannelRecord(ID)> Cache hit for "
			<< id
			<< endl; 
	#endif

	// Return the channel to the caller
	return ptr->second ;
	} 

/*
 *  We didn't find anything in the cache, fetch the data from
 *  the backend and create a new sqlUser object.
 */

sqlChannel* theChan = new (nothrow) sqlChannel(SQLDb);
assert( theChan != 0 ) ;

if (theChan->loadData(id))
	{
 	sqlChannelIDCache.insert(sqlChannelIDHashType::value_type(id, theChan));

	#ifdef LOG_CACHE_HITS
		elog	<< "cmaster::getChannelRecord(ID)> There are "
			<< sqlChannelIDCache.size()
			<< " elements in the ID cache."
			<< endl; 
	#endif

	// Return the channel to the caller
	return theChan;
	}

delete theChan;
return 0;
} 


sqlLevel* cservice::getLevelRecord( sqlUser* theUser, sqlChannel* theChan )
{
// Check if the record is already in the cache.
pair<int, int> thePair( theUser->getID(), theChan->getID() );

sqlLevelHashType::iterator ptr = sqlLevelCache.find(thePair);
if(ptr != sqlLevelCache.end())
	{ 
	// Found something!
	#ifdef LOG_CACHE_HITS
		elog	<< "cmaster::getLevelRecord> Cache hit for "
			<< "user-id:chan-id "
			<< theUser->getID() << ":"
			<< theChan->getID()
			<< endl;
	#endif

	levelCacheHits++;
	ptr->second->setLastUsed(currentTime());
	return ptr->second ;
	} 

/*
 *  We didn't find anything in the cache, fetch the data from
 *  the backend and create a new sqlUser object.
 */

sqlLevel* theLevel = new (nothrow) sqlLevel(SQLDb);
assert( theLevel != 0 ) ;

if (theLevel->loadData(theUser->getID(), theChan->getID()))
	{
 	sqlLevelCache.insert(sqlLevelHashType::value_type(thePair, theLevel));

	#ifdef LOG_SQL
		elog	<< "cmaster::getLevelRecord> There are "
			<< sqlLevelCache.size()
			<< " elements in the cache."
			<< endl;
	#endif

	levelHits++;
	theLevel->setLastUsed(currentTime());
	return theLevel;
	}

delete theLevel;
return 0;
}	
 
/**
 *  Returns the admin access level a particular user has. 
 */
short int cservice::getAdminAccessLevel( sqlUser* theUser )
{
	
/*
 *  First thing, check if this ACCOUNT has been globally 
 *  suspended.
 */

if (theUser->getFlag(sqlUser::F_GLOBAL_SUSPEND))
	{
	return 0;
	}

sqlChannel* theChan = getChannelRecord("*");
if (!theChan)
	{
	elog	<< "cservice::getAdminAccessLevel> Unable to "
		<< "locate channel '*'! Sorry, I can't continue.."
		<< endl;
	::exit(0);
	}

sqlLevel* theLevel = getLevelRecord(theUser, theChan);
if(theLevel)
	{
	return theLevel->getAccess();
	}

// By default, users have level 0 admin access.
return 0;
}

/**
 * Returns the access level a particular user has on a particular
 * channel taking into account channel & user level suspensions.
 * Also used to return the level of access granted to a forced access.
 *
 * Usage: When determining if we should grant a permission to a user
 * to access a particular command/function.
 * To determine the effect access level of a target user.
 */
short int cservice::getEffectiveAccessLevel( sqlUser* theUser,
	sqlChannel* theChan, bool notify )
{
	
/*
 *  Have a look to see if this user has forced some access.
 */

unsigned short forcedAccess = isForced(theChan, theUser);
if (forcedAccess)
	{
	return forcedAccess;
	}
 
sqlLevel* theLevel = getLevelRecord(theUser, theChan);
if( !theLevel )
	{
	return 0 ;
	} 

/*
 *  First thing, check if this ACCOUNT has been globally 
 *  suspended.
 */

if (theUser->getFlag(sqlUser::F_GLOBAL_SUSPEND))
	{ 
	iClient* theClient = theUser->isAuthed();
	if (theClient && notify)
		{
		Notice(theClient, "Your account has been suspended.");
		}
	return 0; 
	}

/* Then, check to see if the channel has been suspended. */ 

if (theChan->getFlag(sqlChannel::F_SUSPEND))
	{
	/* Send them a notice to let them know they've been bad? */
	iClient* theClient = theUser->isAuthed();
	if (theClient && notify)
		{
		Notice(theClient, "The channel %s has been suspended by a cservice administrator.",
			theChan->getName().c_str());
		}
	return 0;
	}

/*
 *  Check to see if this particular access record has been
 *  suspended too.
 */

if (theLevel->getSuspendExpire() != 0)
	{
	// Send them a notice.
	iClient* theClient = theUser->isAuthed();
	if (theClient && notify)
		{
		Notice(theClient, "Your access on %s has been suspended.",
			theChan->getName().c_str());
		}
	return 0;
	}
 
return theLevel->getAccess();
}

/**
 *  Returns the access level a particular user has on a particular
 *  channel. Plain and simple. If the user has 500 in the channel
 *  record, this function returns 500. 
 */
short int cservice::getAccessLevel( sqlUser* theUser,
	sqlChannel* theChan )
{ 
sqlLevel* theLevel = getLevelRecord(theUser, theChan);
if(theLevel)
	{
	return theLevel->getAccess();
 	}

/* By default, users have level 0 access on a channel. */
return 0;
}
 
/**
 * Returns response id "response_id" for this user's prefered
 * language. 
 */
const string cservice::getResponse( sqlUser* theUser, int response_id,
	string msg )
{ 

// Language defaults to English
int lang_id = 1;

if (theUser)
	{
	lang_id = theUser->getLanguageId();
	}
	
pair<int, int> thePair( lang_id, response_id );

translationTableType::iterator ptr = translationTable.find(thePair);
if(ptr != translationTable.end())
	{ 
	/* Found something! */
	return ptr->second ;
	} 

/* 
 * Can't find this response Id within a valid language.
 * Realistically we should bomb here, however it might be wise
 * to 'fallback' to a lower language ID and try again, only bombing if we
 * can't find an english variant. (Carrying on here could corrupt
 * numerous varg lists, and will most likely segfault anyway).
 */
if (lang_id != 1)
	{
	// TODO: Set the language for this user to english.
	// return getResponse(theUser,1);
	}

if( !msg.empty() )
	{
	return msg;
	}

return string( "Unable to retrieve response. Please contact a cservice "
	"administrator." ) ;
}

/**
 *  Execute an SQL query to retrieve all the translation data.
 */
void cservice::loadTranslationTable()
{
ExecStatusType status;

status = SQLDb->Exec("SELECT id,code,name FROM languages");

if (PGRES_TUPLES_OK == status)
	for (int i = 0; i < SQLDb->Tuples(); i++)
		languageTable.insert(languageTableType::value_type(SQLDb->GetValue(i, 1),
			make_pair(atoi(SQLDb->GetValue(i, 0)), SQLDb->GetValue(i, 2))));

#ifdef LOG_SQL
	elog	<< "cmaster::loadTranslationTable> Loaded "
			<< languageTable.size()
			<< " language entries."
			<< endl;
#endif

status = SQLDb->Exec(
	"SELECT language_id,response_id,text FROM translations" ) ;

if( PGRES_TUPLES_OK == status )
	{
	for (int i = 0 ; i < SQLDb->Tuples(); i++)
		{
		/*
		 *  Add to our translations table.
		 */

		int lang_id = atoi(SQLDb->GetValue( i, 0 ));
		int resp_id = atoi(SQLDb->GetValue( i, 1 ));

		pair<int, int> thePair( lang_id, resp_id ) ;

		translationTable.insert(
			translationTableType::value_type(
				thePair, SQLDb->GetValue( i, 2 )) );
		}
	}

#ifdef LOG_SQL
	elog	<< "cmaster::loadTranslationTable> Loaded "
		<< translationTable.size()
		<< " entries."
		<< endl;
#endif

} 
 
bool cservice::isOnChannel( const string& chanName ) const
{
return true;
}

/**
 * This member function executes an SQL query to return all
 * suspended access level records, and unsuspend them. 
 * TODO
 */
void cservice::expireSuspends()
{
elog	<< "cservice::expireSuspends> Checking for expired Suspensions.."
	<< endl;

strstream expireQuery;
expireQuery	<< "SELECT user_id,channel_id FROM levels "
		<< "WHERE suspend_expires <= "
		<< currentTime() 
		<< " AND suspend_expires <> 0"
		<< ends;

#ifdef LOG_SQL
	elog	<< "expireSuspends::sqlQuery> "
		<< expireQuery.str()
		<< endl;
#endif

ExecStatusType status = SQLDb->Exec(expireQuery.str()) ;
delete[] expireQuery.str() ;

if( PGRES_TUPLES_OK != status )
	{
	elog	<< "cservice::expireSuspends> SQL Error: "
		<< SQLDb->ErrorMessage()
		<< endl ;
	return ;
	}

/*
 *  Loops over the results set, and attempt to locate
 *  this level record in the cache.
 */ 

#ifdef LOG_SQL
	elog	<< "cservice::expireSuspends> Found " 
		<< SQLDb->Tuples()
		<< " expired suspensions."
		<< endl; 
#endif

/*
 *  Place our query results into temporary storage, because
 *  we might have to execute other queries inside the
 *  loop which will invalidate our results set.
 */

typedef vector < pair < string, string > > expireVectorType;
expireVectorType expireVector;

for (int i = 0 ; i < SQLDb->Tuples(); i++)
	{ 
	expireVector.push_back(expireVectorType::value_type(
		SQLDb->GetValue(i, 0), 
		SQLDb->GetValue(i, 1) )
		);
	}
 
for (expireVectorType::const_iterator resultPtr = expireVector.begin();
	resultPtr != expireVector.end(); ++resultPtr)
		{
		/*
		 * Attempt to find this level record in the cache.
		 */
		pair<int, int> thePair( atoi(resultPtr->first.c_str()),
			atoi(resultPtr->second.c_str()) );

		sqlLevelHashType::iterator Lptr
			= sqlLevelCache.find(thePair);
		if(Lptr != sqlLevelCache.end())
			{
			/* Found it in the cache, remove suspend. */
			elog	<< "cservice::expireSuspends> "
				<< "Found level record in cache: "
				<< resultPtr->first
				<< ":"
				<< resultPtr->second
				<< endl;
			(Lptr->second)->setSuspendExpire(0);
			(Lptr->second)->setSuspendBy(string()); 
			}

		/*
		 *  Execute a query to update the status in the db.
		 */

		string updateQuery = "UPDATE levels SET suspend_expires = "
			"0, suspend_by = '' WHERE user_id = " 
			+ resultPtr->first
			+ " AND channel_id = "
			+ resultPtr->second;

		#ifdef LOG_SQL
			elog	<< "expireSuspends::sqlQuery> "
				<< updateQuery
				<< endl; 
		#endif

		status = SQLDb->Exec(updateQuery.c_str() ) ;
		if( status != PGRES_COMMAND_OK)
			{
			elog	<< "cservice::expireSuspends> Unable to "
				<< "update record while unsuspending."
				<< endl;
			}

		} // for()
}

/**
 * This function removes any ignores that have expired. 
 */
void cservice::expireSilence()
{
	
silenceListType::iterator ptr = silenceList.begin();
while (ptr != silenceList.end())
	{
	if ( ptr->second.first < currentTime() )
		{
		string theMask = ptr->first;
		strstream s;
		s	<< getCharYYXXX()
			<< " SILENCE "
			<< ptr->second.second
			<< " -"
			<< theMask
			<< ends; 
		Write( s );
		delete[] s.str(); 

		/* 
		 * Locate this user by numeric.
		 * If the numeric is still in use, clear the ignored flag.
		 * If someone else has inherited this numeric, no prob,
		 * its cleared anyway. 
		 */

		iClient* theClient = Network->findClient(ptr->second.second);
		if (theClient)
			{
				setIgnored(theClient, false);
			}

		++ptr;
		silenceList.erase(theMask);
		} else {
			++ptr;
		}

	} // while()
	
} 

/**
 * This member function executes an SQL query to return all
 * bans that have expired. It removes them from the internal
 * cache as well as the database.
 */ 
void cservice::expireBans()
{
elog	<< "cservice::expireBans> Checking for expired bans.."
	<< endl;

strstream expireQuery;
expireQuery	<< "SELECT channel_id,id FROM bans "
		<< "WHERE expires <= "
		<< currentTime() 
		<< ends;

#ifdef LOG_SQL
	elog	<< "sqlQuery> "
		<< expireQuery.str()
		<< endl;
#endif

ExecStatusType status = SQLDb->Exec(expireQuery.str()) ;
delete[] expireQuery.str() ;

if( PGRES_TUPLES_OK != status )
	{ 
	elog	<< "cservice::expireBans> SQL Error: "
		<< SQLDb->ErrorMessage()
		<< endl ;
	return ;
	}

/*
 *  Loops over the results set, and attempt to locate
 *  this ban in the cache.
 */ 

#ifdef LOG_SQL
	elog	<< "cservice::expireBans> Found " 
		<< SQLDb->Tuples()
		<< " expired bans."
		<< endl;
#endif

/*
 *  Place our query results into temporary storage, because
 *  we might have to execute other queries inside the
 *  loop which will invalidate our results set.
 */
typedef vector < pair < unsigned int, unsigned int > > expireVectorType;
expireVectorType expireVector;
 
for (int i = 0 ; i < SQLDb->Tuples(); i++)
	{ 
	expireVector.push_back(expireVectorType::value_type(
		atoi(SQLDb->GetValue(i, 0)), 
		atoi(SQLDb->GetValue(i, 1)) )
		);
	}

for (expireVectorType::const_iterator resultPtr = expireVector.begin();
	resultPtr != expireVector.end(); ++resultPtr)
	{ 
	sqlChannel* theChan = getChannelRecord( resultPtr->first );
	if (!theChan)
		{
		// TODO: Debuglog.
		continue;
		}

	#ifdef LOG_DEBUG
		elog	<< "Checking bans for "
			<< theChan->getName()
			<< endl;
	#endif
	 
	/* Then hunt it out in the cache.. */
	vector<sqlBan*>* chanBans = getBanRecords(theChan);

	/* Loop over all bans cached in this channel, match ID.. */
	vector< sqlBan* >::iterator ptr = chanBans->begin(); 

	while (ptr != chanBans->end())
		{
		sqlBan* theBan = *ptr;
		if ( theBan->getID() == resultPtr->second )
			{
			ptr = chanBans->erase(ptr);

			Channel* tmpChan = Network->findChannel(
				theChan->getName());
			if (tmpChan)
				{
				UnBan(tmpChan, theBan->getBanMask()); 
				}

			#ifdef LOG_DEBUG
				elog	<< "Cleared Ban "
					<< theBan->getBanMask()
					<< " from cache"
					<< endl;
			#endif

			theBan->deleteRecord();
			delete(theBan);
			}
		else 
			{
			++ptr;
			}
		} /* While looking at bans */

	} /* Forall results in set */

}

/**
 * This function iterates over the usercache, and removes
 * accounts not accessed in a certain timeframe. 
 * N.B: do *NOT* expire those with a networkClient set.
 * (Ie: those currently logged in).
 */
void cservice::cacheExpireUsers()
{ 
	logDebugMessage("Beginning User cache cleanup:");
	sqlUserHashType::iterator ptr = sqlUserCache.begin();
	sqlUser* tmpUser;
	clock_t startTime = ::clock();
	clock_t endTime = 0;
	int purgeCount = 0;
	string removeKey;

	while (ptr != sqlUserCache.end())
	{
		tmpUser = ptr->second;
		/*
	 	 *  If this user has been idle one hour, and currently
		 *  isn't logged in, boot him out the window.
		 */
		if ( ((tmpUser->getLastUsed() + 3600) < currentTime()) &&
			!tmpUser->isAuthed() ) 
		{ 
#ifdef LOG_DEBUG
			elog << "cservice::cacheExpireUsers> "
			<< tmpUser->getUserName() 
			<< "; last used: " 
			<< tmpUser->getLastUsed() 
			<< endl;
#endif
			purgeCount++;
			removeKey = ptr->first;
			delete(ptr->second);
			/* Advance the iterator past the soon to be
			 * removed element. */
			++ptr; 
			sqlUserCache.erase(removeKey); 
		}  
			else
		{ 
			++ptr;
		}
	}
	endTime = ::clock();
	logDebugMessage("User cache cleanup complete; Removed %i user records in %i ms.",
		purgeCount, (endTime - startTime) /  CLOCKS_PER_SEC); 
} 

void cservice::cacheExpireLevels()
{
	logDebugMessage("Beginning Channel Level-cache cleanup:");

	/*
	 *  While we are at this, we'll clear out any FORCE'd access's
	 *  in channels.
	 */

	sqlChannelHashType::iterator ptr = sqlChannelCache.begin();
	while (ptr != sqlChannelCache.end())
	{
		sqlChannel* theChan = (ptr)->second;
		if(theChan->forceMap.size() > 0)
		{ 
			logDebugMessage("Clearing out %i FORCE(s) from channel %s",
						theChan->forceMap.size(), theChan->getName().c_str());
			theChan->forceMap.clear(); 
		}
 
		++ptr;
	} 
	logDebugMessage("Channel Level cache-cleanup complete.");
}

/**
 * This member function checks the reop buffer for any
 * pending reop's, performing them if neccessary.
 * TODO: Update internal state for 'me'.
 */ 
void cservice::performReops()
{
/* TODO: Rewrite this bit --Gte */

if( reopQ.empty() )
	{
	return;
	}

reopQType::iterator ptr = reopQ.begin();
while ( ptr != reopQ.end() )
{
if (ptr->second <= currentTime()) 
	{
	Channel* tmpChan = Network->findChannel(ptr->first);
	if (tmpChan)
		{ 
		ChannelUser* tmpChanUser;
		tmpChanUser = tmpChan->findUser(me);

		/* Don't op ourself if we're already opped.. */
		if (tmpChanUser && !tmpChanUser->getMode(ChannelUser::MODE_O))
			{
			strstream s;
			s	<< MyUplink->getCharYY()
				<< " M "
				<< tmpChan->getName()
				<< " +o "
				<< getCharYYXXX()
				<< ends;
			
			Write( s );
			delete[] s.str();

			/*
			 *  Update the channel state.
			 */

			tmpChanUser->setMode(ChannelUser::MODE_O);

			#ifdef LOG_DEBUG
				elog	<< "cservice::OnTimer> REOP "
					<< tmpChan->getName()
					<< endl;
			#endif
			}

			/*
			 *  If STRICTOP or NOOP is set, do the 'right thing.
			 */

			sqlChannel* theChan = getChannelRecord(tmpChan->getName());
			if (theChan)
			{
				if(theChan->getFlag(sqlChannel::F_NOOP))
					{
					deopAllOnChan(tmpChan);
					}
				if(theChan->getFlag(sqlChannel::F_STRICTOP))
					{
					deopAllUnAuthedOnChan(tmpChan);
					}
			}

		} 
		reopQ.erase(ptr->first); 
	} /* If channel exists */
	++ptr;
} /* While */
 
}

/**
 * This member function processes and pending database
 * update notifications, reloading any records as
 * neccessary.
 * TODO: Notification update for Ban records.
 * TODO: Add logging of username/records updated.
 */
void cservice::processDBUpdates()
{
PGnotify* notify = SQLDb->Notifies();

/*
 *  Check to see if the PGSQL connection is broken..
 */
 
/* 'We get signal..' */
if (!notify)
	{
	return;
	} 

#ifdef LOG_DEBUG
	elog	<< "cmaster::OnTimer> Recieved a notification event for '"
		<< notify->relname 
		<< "' from Backend PID '"
		<< notify->be_pid
		<< "'"
		<< endl;
#endif
 
// Check we aren't getting our own updates.
if (notify->be_pid == SQLDb->getPID())
	{
	#ifdef LOG_DEBUG
		elog	<< "cmaster::OnTimer> Notification from our Backend "
			<< "PID, ignoring update."
			<< endl; 
	#endif

	// TODO: Be absolutely certain that we should be using free()
	// here.
	free(notify); 
	return;
	}

logDebugMessage("Recieved a notification event for '%s' from backend PID %i.",
	notify->relname, notify->be_pid);

assert( notify->relname != 0 ) ;
string relname( notify->relname ) ;

// Free memory allocated by postgres API object.
// TODO: Be absolutely certain that we should be using free()
// here.
free(notify); 

// 1 = channel, 2 = user, 3 = level, 4 = ban. 
unsigned short updateType = 0;
strstream theQuery ;

if (relname == "channels_u") 
	{
	updateType = 1;

	// Fetch updated channel information.
	theQuery	<< "SELECT "
			<< sql::channel_fields
			<< ",now()::abstime::int4 as db_unixtime FROM "
			<< "channels WHERE last_updated >= "
			<< lastChannelRefresh;
	}

if( relname == "users_u" )
	{
	updateType = 2;

	// Fetch updated user information.
	theQuery	<< "SELECT "
			<< sql::user_fields
			<< ",now()::abstime::int4 as db_unixtime FROM "
			<< "users WHERE "
			<< "users.last_updated >= "
			<< lastUserRefresh;
	} 

if ( relname == "levels_u")
	{
	updateType = 3;

	// Fetch updated level information.
	theQuery	<< "SELECT "
			<< sql::level_fields
			<< ",now()::abstime::int4 as db_unixtime FROM "
			<< "levels WHERE last_updated >= "
			<< lastLevelRefresh;
	} 

theQuery << ends;

#ifdef LOG_SQL
	elog	<< "cservice::OnTimer> sqlQuery: "
		<< theQuery.str()
		<< endl;
#endif

// Execute query, parse results.
ExecStatusType status = SQLDb->Exec(theQuery.str());
delete[] theQuery.str() ;

if (status != PGRES_TUPLES_OK)
	{
	elog	<< "cmaster::OnTimer> SQL error: "
		<< SQLDb->ErrorMessage()
		<< endl;

	// TODO: Log to debugchan here.
	return;
	}

#ifdef LOG_DEBUG
	elog	<< "cmaster::OnTimer> Found "
		<< SQLDb->Tuples()
		<< " updated records."
		<< endl;
#endif

logDebugMessage("Found %i updated records.",
	SQLDb->Tuples());

/*
 *  Now, update the cache with information in this results set.
 */

if (SQLDb->Tuples() <= 0)
	{ 
	// We could get no results back if the last_update
	// field wasn't set properly.
	return;
	}

/* Update our time offset incase things drift.. */
dbTimeOffset = atoi(SQLDb->GetValue(0,"db_unixtime")) - ::time(NULL);

switch(updateType)
	{
	case 1: // Channel update.
		{
		for (int i = 0 ; i < SQLDb->Tuples(); i++)
			{ 
			sqlChannelHashType::iterator ptr =
				sqlChannelCache.find(SQLDb->GetValue(i, 1)); 
			if(ptr != sqlChannelCache.end())
				{
				/* Found something! */
				(ptr->second)->setAllMembers(i);
				}
			else 
				{
				/* Not in the cache.. must be a new channel. */
				/* Create new channel record, insert in cache. */
				sqlChannel* newChan =
					new (nothrow) sqlChannel(SQLDb);
				assert( newChan != 0 ) ;

				newChan->setAllMembers(i);
			 	sqlChannelCache.insert(sqlChannelHashType::value_type(newChan->getName(), newChan));

				/* Join the newly registered channel. */
				//Join(newChan->getName(), newChan->getChannelMode(), 0, true);
				}
			
			}
		
		// Set the "Last refreshed from channels table" timestamp.
		lastChannelRefresh = atoi(SQLDb->GetValue(0,"db_unixtime"));
		break;
		} // case 1

	case 2: // User updates.
		{
		for (int i = 0 ; i < SQLDb->Tuples(); i++)
			{ 
			sqlUserHashType::iterator ptr =
				sqlUserCache.find(SQLDb->GetValue(i, 1)); 

			if(ptr != sqlUserCache.end())
				{
				// Found something..
				(ptr->second)->setAllMembers(i); 
				}
			} // for()
		
		// Set the "Last refreshed from channels table" timestamp.
		lastUserRefresh = atoi(SQLDb->GetValue(0,"db_unixtime"));
		break;
		} // case 2

	case 3: // Level updates.
		{
		for (int i = 0 ; i < SQLDb->Tuples(); i++)
			{ 
			sqlLevelHashType::iterator ptr =
				sqlLevelCache.find(make_pair(atoi(SQLDb->GetValue(i, 1)), atoi(SQLDb->GetValue(i, 0)))); 

			if(ptr != sqlLevelCache.end())
				{
				// Found something..
				(ptr->second)->setAllMembers(i); 
				}
			}
		
		// Set the "Last refreshed from channels table" timestamp.
		lastLevelRefresh = atoi(SQLDb->GetValue(0,"db_unixtime"));
		break;
		} // case 3
 
	} // switch()

}

/**
 * Timer handler.
 * This member handles a number of timers, dispatching
 * control to the relevant member for the timer
 * triggered.
 */
int cservice::OnTimer(xServer::timerID timer_id, void*)
{

if (timer_id == dBconnection_timerID)
	{ 
	checkDbConnectionStatus();
	performReops();

	/* Refresh Timers */
	time_t theTime = time(NULL) + connectCheckFreq;
	dBconnection_timerID = MyUplink->RegisterTimer(theTime, this, NULL);
	}
 
if (timer_id ==  update_timerID)
	{ 
	processDBUpdates();

	/* Refresh Timer */
	time_t theTime = time(NULL) + updateInterval;
	update_timerID = MyUplink->RegisterTimer(theTime, this, NULL); 
	}

if (timer_id == expire_timerID)
	{ 
	expireBans();
	expireSuspends();
	expireSilence();

	/* Refresh Timer */
	time_t theTime = time(NULL) + expireInterval;
	expire_timerID = MyUplink->RegisterTimer(theTime, this, NULL); 
	} 

if (timer_id == cache_timerID)
	{ 
	cacheExpireUsers(); 
	cacheExpireLevels();

	/* Refresh Timer */
	time_t theTime = time(NULL) + cacheInterval;
	cache_timerID = MyUplink->RegisterTimer(theTime, this, NULL); 
	} 

if (timer_id == pending_timerID)
	{ 
	/* 
	 * Load the list of pending channels and calculate/save some stats.
	 */

	loadPendingChannelList(); 
 
	/*
	 * Load a list of channels in NOTIFICATION stage and send them
	 * a notice. 
	 */

	strstream theQuery;
	theQuery	<<  "SELECT channels.name,channels.id,pending.created_ts"
				<< " FROM pending,channels" 
				<< " WHERE channels.id = pending.channel_id"
				<< " AND pending.status = 2;"
				<< ends;

#ifdef LOG_SQL
		elog	<< "cmaster::loadPendingChannelList> "
			<< theQuery.str()
			<< endl; 
#endif

	ExecStatusType status = SQLDb->Exec(theQuery.str()) ;
	delete[] theQuery.str() ;
	unsigned int noticeCount = 0;

	if( PGRES_TUPLES_OK == status )
		{
		for (int i = 0 ; i < SQLDb->Tuples(); i++)
			{ 
			noticeCount++;
			string channelName = SQLDb->GetValue(i,0);
			unsigned int channel_id = atoi(SQLDb->GetValue(i, 1));
			unsigned int created_ts = atoi(SQLDb->GetValue(i, 2));
			Channel* tmpChan = Network->findChannel(channelName);
 
			if (tmpChan)
				{
				serverNotice(tmpChan, "This channel is currently being processed for registration.");
				serverNotice(tmpChan, "If you wish to view the details of the application or to object, please visit:");
				serverNotice(tmpChan, "%s?id=%i-%i", pendingPageURL.c_str(), created_ts, channel_id);
				} 
			} 
		}

	logDebugMessage("Loaded Pending Channels notification list, I have just notified %i channels that they are under registration.",
		noticeCount);
	

	/* Refresh Timer */

	time_t theTime = time(NULL) +pendingChanPeriod;
	pending_timerID = MyUplink->RegisterTimer(theTime, this, NULL);
	}


return 0 ;
}
 
/**
 * Send a notice to a channel from the server.
 * TODO: Move this method to xServer.
 */
bool cservice::serverNotice( Channel* theChannel, const char* format, ... )
{

char buf[ 1024 ] = { 0 } ;
va_list _list ;
	
va_start( _list, format ) ;
vsprintf( buf, format, _list ) ;
va_end( _list ) ;
	
strstream s;
s	<< MyUplink->getCharYY()
	<< " O "
	<< theChannel->getName()
	<< " :"
	<< buf
	<< ends;

Write( s );
delete[] s.str();
	
return false;
} 

/**
 * Send a notice to a channel from the server.
 * TODO: Move this method to xServer.
 */
bool cservice::serverNotice( Channel* theChannel, const string& Message)
{
 
strstream s;
s	<< MyUplink->getCharYY()
	<< " O "
	<< theChannel->getName()
	<< " :"
	<< Message
	<< ends;

Write( s );
delete[] s.str();
	
return false;
} 

/**
 *  Log a message to the admin channel and the logfile.
 */
bool cservice::logAdminMessage(const char* format, ... )
{

char buf[ 1024 ] = { 0 } ;
va_list _list ;
	
va_start( _list, format ) ;
vsprintf( buf, format, _list ) ;
va_end( _list ) ;
	
// Try and locate the relay channel.
Channel* tmpChan = Network->findChannel(relayChan);
if (!tmpChan) 
	{
	elog	<< "cservice::logAdminMessage> Unable to locate relay "
		<< "channel on network!"
		<< endl;
	return false;
	}

string message = string( "[" ) + nickName + "] " + buf ;
serverNotice(tmpChan, message);
return true;
}

bool cservice::logDebugMessage(const char* format, ... )
{

char buf[ 1024 ] = { 0 } ;
va_list _list ;
	
va_start( _list, format ) ;
vsprintf( buf, format, _list ) ;
va_end( _list ) ;
	
// Try and locate the debug channel.
Channel* tmpChan = Network->findChannel(debugChan);
if (!tmpChan) 
	{
	elog	<< "cservice::logAdminMessage> Unable to locate debug "
		<< "channel on network!"
		<< endl;
	return false;
	}

string message = string( "[" ) + nickName + "] " + buf ;
serverNotice(tmpChan, message);
return true;
}

 
string cservice::userStatusFlags( const string& theUser ) 
{

string flagString = "";

sqlUserHashType::iterator ptr = sqlUserCache.find(theUser);

if(ptr != sqlUserCache.end())
	{
	sqlUser* tmpUser = ptr->second;
	flagString = 'L';
	if(tmpUser->getFlag(sqlUser::F_LOGGEDIN)) flagString += 'U';
	}

return flagString;
}

const string cservice::prettyDuration( int duration ) const
{ 

// Pretty format a 'duration' in seconds to
// x day(s), xx:xx:xx.

char tmpBuf[ 64 ] = {0};

int	res = currentTime() - duration,
	secs = res % 60,
	mins = (res / 60) % 60,
	hours = (res / 3600) % 24,
	days = (res / 86400) ;

sprintf(tmpBuf, "%i day%s, %02d:%02d:%02d",
	days,
	(days == 1 ? "" : "s"),
	hours,
	mins,
	secs );

return string( tmpBuf ) ;
}  

bool cservice::validUserMask(const string& userMask) const
{

// Check that a '!' exists, and that the nickname
// is no more than 9 characters
StringTokenizer st1( userMask, '!' ) ;
if( (st1.size() != 2) || (st1[ 0 ].size() > 9) )
	{
	return false ;
	}

// Check that a '@' exists and that the username is
// no more than 12 characters
StringTokenizer st2( st1[ 1 ], '@' ) ;

if( (st2.size() != 2) || (st2[ 0 ].size() > 12) )
	{
	return false ;
	}

// Be sure that the hostname is no more than 128 characters
if( st2[ 1 ].size() > 128 )
	{
	return false ;
	}

// Tests have passed
return true ;
}

void cservice::OnChannelModeO( Channel* theChan, ChannelUser* theChanUser,
	const xServer::opVectorType& theTargets)
{
/*
 * There are a number of things to do when we recieve a mode O for
 * a channel/targets.
 * Firstly, we check the status of certain channel flags is it set
 * NOOP? is it set STRICTOP?
 * We will only ever recieve events for channels that are registered.
 */ 

sqlChannel* reggedChan = getChannelRecord(theChan->getName());
if(!reggedChan)
	{
	elog	<< "cservice::OnChannelModeO> WARNING, unable to "
		<< "locate channel record"
		<< " for registered channel event: "
		<< theChan->getName()
		<< endl;
	return;
	}

// List of clients to deop.
vector< iClient* > deopList;
// Channel bans.
vector< sqlBan*>* banList;
banList = getBanRecords(reggedChan);

// If we find a situation where we need to deop the person who has
// performed the mode, do so.
bool sourceHasBeenBad = false;
 
int deopCounter = 0;

for( xServer::opVectorType::const_iterator ptr = theTargets.begin() ;
	ptr != theTargets.end() ; ++ptr )
	{
	ChannelUser* tmpUser = ptr->second;
	bool polarity = ptr->first;

	if (polarity)
		{
		// If somebody is being opped.

		// If the channel is NOOP, deop everyone who tries to
		// get opped!
		if (reggedChan->getFlag(sqlChannel::F_NOOP))
			{
			if ( !tmpUser->getClient()->getMode(iClient::MODE_SERVICES) )
			deopList.push_back(tmpUser->getClient());
			}

		sqlUser* authUser = isAuthed(tmpUser->getClient(), false);

		// Has the target user's account been suspended?
		if (authUser && authUser->getFlag(sqlUser::F_GLOBAL_SUSPEND))
		{ 
			Notice(theChanUser->getClient(), "The user %s (%s) has been suspended by a CService Administrator.",
				authUser->getUserName().c_str(), tmpUser->getClient()->getNickName().c_str()); 
			deopList.push_back(tmpUser->getClient()); 
			sourceHasBeenBad = true;
		}

		// If the channel is STRICTOP, deop everyone who isn't
		// authenticated or and doesn't have access on the
		// channel.

		if (reggedChan->getFlag(sqlChannel::F_STRICTOP))
			{
			if (!authUser)
				{
				// Not authed, deop.
				if ( !tmpUser->getClient()->getMode(iClient::MODE_SERVICES) )
				deopList.push_back(tmpUser->getClient());
				sourceHasBeenBad = true;
				// Authed but doesn't have access... deop.
				}
			else if (!(getEffectiveAccessLevel(authUser,reggedChan, false) >= level::op))
				{
				if ( !tmpUser->getClient()->getMode(iClient::MODE_SERVICES) )					
				deopList.push_back(tmpUser->getClient());
				sourceHasBeenBad = true;
				}
			}	

		/*
		 *  The 'Fun' Part. Scan through channel bans to see if this hostmask
		 *  is 'banned' at 75 or below.
		 */

		sqlBan* theBan = isBannedOnChan(reggedChan, tmpUser->getClient()); 
		if( theBan && (theBan->getLevel() <= 75) ) 
			{
				if ( !tmpUser->getClient()->getMode(iClient::MODE_SERVICES) )
					{
					deopList.push_back(tmpUser->getClient());
					sourceHasBeenBad = true;

					/* Tell the person bein op'd that they can't */
					Notice(tmpUser->getClient(), 
						"You are not allowed to be opped on %s",
						reggedChan->getName().c_str());
	
					/* Tell the person doing the op'ing this is bad */
					if (theChanUser) 
						{
						Notice(theChanUser->getClient(),
							"%s isn't allowed to be opped on %s",
							tmpUser->getClient()->getNickName().c_str(), 
							reggedChan->getName().c_str());
						} 
					}
			}

		} // if()
	else
		{
		/* Somebody is being deopped? */
		deopCounter++;

		/* What if someone deop'd us?! */
		if (tmpUser->getClient() == me)
			{ 
			logAdminMessage("I've been deopped on %s!",
				reggedChan->getName().c_str());
			/* Add this chan to the reop queue, ready to op itself in 15 seconds. */
			reopQ.insert(cservice::reopQType::value_type(reggedChan->getName(),
				currentTime() + 15) ); 
			}
		}
	} // for()

/*
 *  Send notices and perform the deop's.
 */

if (theChanUser && sourceHasBeenBad)
	deopList.push_back(theChanUser->getClient());

if( !deopList.empty() )
	{
	if ((theChanUser) && (reggedChan->getFlag(sqlChannel::F_NOOP)) ) 
		{
		Notice( theChanUser->getClient(),
			"The NOOP flag is set on %s",
			reggedChan->getName().c_str());
		}

	if ((theChanUser) && (reggedChan->getFlag(sqlChannel::F_STRICTOP)) ) 
		{
		Notice( theChanUser->getClient(),
			"The STRICTOP flag is set on %s",
			reggedChan->getName().c_str());
		}

	DeOp(theChan, deopList);
	}

/*
 *  Have more than 'maxdeoppro' been deopped?
 *  If so, suspend and kick 'em.
 */

if ((theChanUser) && (deopCounter >= reggedChan->getMassDeopPro()) 
	&& (reggedChan->getMassDeopPro() > 0))
	{
		doInternalBanAndKick(reggedChan, theChanUser->getClient(),
			"### MASSDEOPPRO TRIGGERED! ###");
	} 
}

int cservice::OnEvent( const eventType& theEvent,
	void* data1, void* data2, void* data3, void* data4 )
{
switch( theEvent )
	{
	case EVT_QUIT:
	case EVT_KILL:
		{
		/*
		 *  We need to deauth this user if they're authed.
		 *  Also, clean up their custom data memory.
		 */
	
		iClient* tmpUser = (theEvent == EVT_QUIT) ?
			static_cast< iClient* >( data1 ) :
			static_cast< iClient* >( data2 ) ;

		sqlUser* tmpSqlUser = isAuthed(tmpUser, false);
		if (tmpSqlUser)
			{
			tmpSqlUser->networkClient = NULL;
			tmpSqlUser->removeFlag(sqlUser::F_LOGGEDIN);
			elog	<< "cservice::OnEvent> Deauthenticated "
				<< "user "
				<< tmpSqlUser->getUserName()
				<< endl;
			}

		// Clear up the custom data structure we appended to
		// this iClient.
		networkData* tmpData = static_cast< networkData* >(
			tmpUser->getCustomData(this) ) ; 
		tmpUser->removeCustomData(this);

		delete(tmpData); 
		customDataAlloc--; 

		break ;
		} // case EVT_KILL/case EVT_QUIT

	case EVT_NICK:
		{
		/*
		 *  Give this new user a custom data structure!
		 */

		iClient* tmpUser =
			static_cast< iClient* >( data1 );
		networkData* newData = new (nothrow) networkData(); 
		assert( newData != 0 ) ;

		customDataAlloc++;

		// Not authed.
		newData->currentUser = NULL;
		tmpUser->setCustomData(this,
			static_cast< void* >( newData ) );

		break;
		} // case EVT_NICK
	} // switch()

return 0;
}

/**
 * Support function to deop all opped users on a channel.
 */
void cservice::deopAllOnChan(Channel* theChan)
{ 
if( !theChan )
	{
	/* Don't try this on a null channel. */
	return;
	}

sqlChannel* reggedChan = getChannelRecord(theChan->getName());

if (!reggedChan)
	{
	return;
	}

if (!reggedChan->getInChan())
	{ 
	return;
	}

/* Check we're actually opped first.. */

ChannelUser* tmpBotUser = theChan->findUser(getInstance());
if( !tmpBotUser || !tmpBotUser->getMode(ChannelUser::MODE_O) )
	{
	return; 
	}

vector< iClient* > deopList;

for( Channel::const_userIterator ptr = theChan->userList_begin();
	ptr != theChan->userList_end() ; ++ptr )
	{
	if( ptr->second->getMode(ChannelUser::MODE_O))
		{
			
		/* Don't deop +k things */
		if ( !ptr->second->getClient()->getMode(iClient::MODE_SERVICES) ) 
			deopList.push_back( ptr->second->getClient() );

		} // If opped.
	}

if( !deopList.empty() )
	{
	DeOp(theChan, deopList);
	}

}

size_t cservice::countChanOps(const Channel* theChan)
{ 
if( !theChan )
	{
	/* Don't try this on a null channel. */
	return 0;
	}

size_t chanOps = 0;

for( Channel::const_userIterator ptr = theChan->userList_begin();
	ptr != theChan->userList_end() ; ++ptr )
	{
	if( ptr->second->getMode(ChannelUser::MODE_O))
		{
		chanOps++;			
		} // If opped.
	}

return chanOps;
}

/**
 * Support function to deop all non authed opped users on a channel.
 */
void cservice::deopAllUnAuthedOnChan(Channel* theChan)
{ 
// TODO: assert( theChan != 0 ) ;

if( !theChan )
	{
	/* Don't try this on a null channel. */
	return;
	}

sqlChannel* reggedChan = getChannelRecord(theChan->getName());

if( !reggedChan || !reggedChan->getInChan() )
	{
	return;
	}

/* Check we're actually opped first.. */

ChannelUser* tmpBotUser = theChan->findUser(getInstance());
if(! tmpBotUser || !tmpBotUser->getMode(ChannelUser::MODE_O))
	{ 
	return;
	}

vector< iClient* > deopList;

for( Channel::const_userIterator ptr = theChan->userList_begin();
	ptr != theChan->userList_end() ; ++ptr )
	{
	if( ptr->second->getMode(ChannelUser::MODE_O))
		{
		/* Are they authed? */
		sqlUser* authUser = isAuthed(ptr->second->getClient(), false); 
 
		if (!authUser)
			{
			/* Not authed, deop this guy + Don't deop +k things */
			if ( !ptr->second->getClient()->getMode(iClient::MODE_SERVICES) ) 
				{
				deopList.push_back( ptr->second->getClient() ); 
				}

		/* Authed but no access? Tough. :) */
			}
		else if ((reggedChan) && !(getEffectiveAccessLevel(authUser, reggedChan, false) >= level::op))
			{
			/* Don't deop +k things */
			if ( !ptr->second->getClient()->getMode(iClient::MODE_SERVICES) ) 
				{
				deopList.push_back( ptr->second->getClient() );
				}
			} 

		} // if opped.
	} // forall users in channel.

if( !deopList.empty() )
	{
	DeOp(theChan, deopList);
	}

}
 
/**
 * Handler for registered channel events.
 * Performs a number of functions, autoop, autovoice, bankicks, etc. 
 */
int cservice::OnChannelEvent( const channelEventType& whichEvent,
	Channel* theChan,
	void* data1, void* data2, void* data3, void* data4 )
{
 
iClient* theClient = 0 ;

switch( whichEvent )
	{
	case EVT_CREATE:
	case EVT_JOIN: 
		{
		/* 
		 * We should only ever recieve events for registered channels, or those
		 * that are 'pending'. If we do get past the pending check, there must be
		 * some kind of database inconsistancy.
		 */

		theClient = static_cast< iClient* >( data1 ) ;

		pendingChannelListType::iterator ptr = pendingChannelList.find(theChan->getName()); 

		if(ptr != pendingChannelList.end())
			{
			/*
			 *  Yes, this channel is pending registration, update join count
			 *  and check out this user joining.
			 */

			ptr->second->join_count++;

			/*
			 *  Now, has this users IP joined this channel before?
			 *  If not - we keep a record of it.
			 */

			sqlPendingChannel::trafficListType::iterator Tptr =  
				ptr->second->trafficList.find(theClient->getIP());

			sqlPendingTraffic* trafRecord;

			/*
			 * If we have more than 50 unique IP's join, we don't bother
			 * recording anymore.
			 */

			if (ptr->second->unique_join_count <= 50)
				{
				if(Tptr == ptr->second->trafficList.end())
					{
					/* New IP, create and write the record. */
	
					trafRecord = new sqlPendingTraffic(SQLDb);
					trafRecord->ip_number = theClient->getIP();
					trafRecord->join_count = 1;
					trafRecord->channel_id = ptr->second->channel_id;
					trafRecord->insertRecord();

					ptr->second->trafficList.insert(sqlPendingChannel::trafficListType::value_type(
						theClient->getIP(), trafRecord));
	
					logDebugMessage("Created a new IP traffic record for IP#%u (%s) on %s",
						theClient->getIP(), theClient->getNickUserHost().c_str(),
						theChan->getName().c_str()); 
					} else
					{
					/* Already cached, update and save. */
					trafRecord = Tptr->second;
					trafRecord->join_count++;
					trafRecord->commit();
					}

					ptr->second->unique_join_count = ptr->second->trafficList.size();

					logDebugMessage("New total for IP#%u on %s is %i",
						theClient->getIP(), theChan->getName().c_str(),
						trafRecord->join_count);
				}
 
			sqlUser* theUser = isAuthed(theClient, false);
			if (!theUser)
				{
				/*
				 *  If this user isn't authed, he can't possibly be flagged
				 *  as one of the valid supporters, so we drop out.
				 */

				return xClient::OnChannelEvent( whichEvent, theChan,
					data1, data2, data3, data4 );
				}

				/*
				 * Now, if this guy is a supporter, we bump his join count up.
				 */

				sqlPendingChannel::supporterListType::iterator Supptr = ptr->second->supporterList.find(theUser->getID());
				if (Supptr != ptr->second->supporterList.end())
					{ 
						Supptr->second++;
						ptr->second->commitSupporter(Supptr->first, Supptr->second);
						logDebugMessage("New total for Supporter #%i (%s) on %s is %i.", theUser->getID(), 
							theUser->getUserName().c_str(), theChan->getName().c_str(), Supptr->second);
					}

			return xClient::OnChannelEvent( whichEvent, theChan,
				data1, data2, data3, data4 ); 
			}
 
		sqlChannel* reggedChan = getChannelRecord(theChan->getName());
		if(!reggedChan)
			{
			elog	<< "cservice::OnChannelEvent> WARNING, "
				<< "unable to locate channel record"
				<< " for registered channel event: "
				<< theChan->getName()
				<< endl;
			return 0;
			} 

		/* 
		 * First thing we do - check if this person is banned.
		 * If so, they're booted out.
		 */ 

		if (checkBansOnJoin(theChan, reggedChan, theClient))
			{
			break;
			}
 
		/* Is it time to set an autotopic? */
		if (reggedChan->getFlag(sqlChannel::F_AUTOTOPIC) &&
			(reggedChan->getLastTopic()
			+ topic_duration <= currentTime()))
			{
			doAutoTopic(reggedChan);
			}

		/* Check noop isn't set */ 
		if (reggedChan->getFlag(sqlChannel::F_NOOP))
			{
			break;
			}
 
		/* Deal with auto-op first - check this users access level. */
		sqlUser* theUser = isAuthed(theClient, false);
		if (!theUser)
			{
			/* If not authed.. */
			break;
			}

		/* Check access in this channel. */
		int accessLevel = getEffectiveAccessLevel(theUser, reggedChan, false); 
		if (!accessLevel)
			{
			/* No access.. */
			break;
 			}
 
		sqlLevel* theLevel = getLevelRecord(theUser, reggedChan);
		if(!theLevel) 
			{
			break;
			}

		/* Check strictop isn't on, and this user is < 100 */
		if (reggedChan->getFlag(sqlChannel::F_STRICTOP))
			{
			if (!(accessLevel >= level::op))
				{
				break; 
				}
			}	
 
		/* Next, see if they have auto op set. */
		if (theLevel->getFlag(sqlLevel::F_AUTOOP)) 
			{
			/* If they are suspended, or somehow have less than 100, don't op them */
			if (accessLevel)
				{
				Op(theChan, theClient); 
				break;
				}
			}

		/* Or auto voice? */
		if (theLevel->getFlag(sqlLevel::F_AUTOVOICE)) 
			{
			/* If they are suspended, or somehow have less than 75, don't voice them */
			if (accessLevel)
				{
				Voice(theChan, theClient);
				break;
				}
			}
 
		break;
		}

	default:
		break;
	} // switch()

return xClient::OnChannelEvent( whichEvent, theChan,
	data1, data2, data3, data4 );
}
 
/**
 *  This function matches a client against the bans stored in the
 *  database for this channel.
 *  Returns an sqlBan if it matches, false otherwise.
 *  'theChan' and 'theClient' must _not_ be null.
 */
sqlBan* cservice::isBannedOnChan(sqlChannel* theChan, iClient* theClient)
{
vector< sqlBan* >* banList = getBanRecords(theChan);
vector< sqlBan* >::iterator ptr = banList->begin();

while (ptr != banList->end())
	{
	sqlBan* theBan = *ptr; 

	if( match(theBan->getBanMask(),
		theClient->getNickUserHost()) == 0)
			{
			return theBan;
			}
	++ptr; 
	} /* while() */
	
return NULL;
}
 
/**
 * This function compares a client with any active bans set in the DB.
 * If matched, the ban is applied and the user is kicked.
 * Returns true if matched, false if not.
 * N.B: Called from OnChannelEvent, theClient is guarantee'd to be in the
 * channel and netChan will exist.
 *--------------------------------------------------------------------------*/ 
bool cservice::checkBansOnJoin( Channel* netChan, sqlChannel* theChan,
	iClient* theClient )
{

sqlBan* theBan = isBannedOnChan(theChan, theClient);

// TODO: Ban through the server. 
// TODO: Violation of rule of numbers
/* If we found a matching ban */
if( theBan && (theBan->getLevel() >= 75) )
	{
	strstream s;
	s	<< getCharYYXXX()
		<< " M "
		<< theChan->getName()
		<< " +b "
		<< theBan->getBanMask()
		<< ends;
	
	Write( s );
	delete[] s.str(); 

	netChan->setBan( theBan->getBanMask() ) ;

	/* Don't kick banned +k bots */
	if ( !theClient->getMode(iClient::MODE_SERVICES) )
		{
		Kick(netChan, theClient,
			string( "("
			+ theBan->getSetBy()
			+ ") "
			+ theBan->getReason()) );
		}

		return true;
	} /* Matching Ban > 75 */ 

/*
 * If they're banned < 75, return true, but don't
 * do anything.
 */
if( theBan && (theBan->getLevel() < 75) )
	{
	return true;
	}

return false;
}

int cservice::OnWhois( iClient* sourceClient,
			iClient* targetClient )
{
	/*
	 *  Return info about 'targetClient' to 'sourceClient'
	 */

strstream s;
s	<< getCharYY()
	<< " 311 " 
	<< sourceClient->getCharYYXXX() 
	<< " " << targetClient->getNickName()
	<< " " << targetClient->getUserName()
	<< " " << targetClient->getInsecureHost()
	<< " * :"
	<< ends; 
Write( s );
delete[] s.str();
 
strstream s4;
s4	<< getCharYY()
	<< " 317 " 
	<< sourceClient->getCharYYXXX() 
	<< " " << targetClient->getNickName()
	<< " 0 " << targetClient->getConnectTime()
	<< " :seconds idle, signon time"
	<< ends;
Write( s4 );
delete[] s4.str(); 

if (targetClient->isOper())
	{
	strstream s5;
	s5	<< getCharYY()
		<< " 313 " 
		<< sourceClient->getCharYYXXX() 
		<< " " << targetClient->getNickName()
		<< " :is an IRC Operator"
		<< ends; 
	Write( s5 );
	delete[] s5.str(); 
	}

sqlUser* theUser = isAuthed(targetClient, false);

if (theUser)
	{
	strstream s6;
	s6	<< getCharYY()
		<< " 316 " 
		<< sourceClient->getCharYYXXX() 
		<< " " << targetClient->getNickName()
		<< " :is Logged in as "
		<< theUser->getUserName()
		<< ends; 
	Write( s6 );
	delete[] s6.str(); 
	}

if (isIgnored(targetClient))
	{
	strstream s7;
	s7	<< getCharYY()
		<< " 316 " 
		<< sourceClient->getCharYYXXX() 
		<< " " << targetClient->getNickName()
		<< " :is currently being ignored. " 
		<< ends; 
	Write( s7 );
	delete[] s7.str(); 
	}

strstream s3;
s3	<< getCharYY()
	<< " 318 " 
	<< sourceClient->getCharYYXXX() 
	<< " " << targetClient->getNickName()
	<< " :End of /WHOIS list."
	<< ends; 
Write( s3 );
delete[] s3.str();

return 0;
}
 
/*--doAutoTopic---------------------------------------------------------------
 *
 * This support function sets the autotopic in a particular channel. 
 */
void cservice::doAutoTopic(sqlChannel* theChan)
{

/* Quickly drop out if nothing is set.. */ 
if ( theChan->getDescription().empty() && theChan->getURL().empty() )
	{
	return;
	}

string extra ;
if( !theChan->getURL().empty() )
	{
	extra = " ( " + theChan->getURL() + " )" ;
	}

strstream s;
s	<< getCharYYXXX()
	<< " T "
	<< theChan->getName()
	<< " :" 
	<< theChan->getDescription()
	<< extra
	<< ends; 

Write( s );
delete[] s.str(); 

theChan->setLastTopic(currentTime());
}	

/**
 * Bans a user via IRC and the database with 'theReason',
 * and then kicks. theChan cannot be null.
 */
bool cservice::doInternalBanAndKick(sqlChannel* theChan,
	iClient* theClient, const string& theReason)
{ 
/* Fetch the banVector for this channel */
vector< sqlBan* >* banList = getBanRecords(theChan);

/*
 *  Check to see if this banmask already exists in the
 *  channel. (Ugh, and overlapping too.. hmm).
 */

/* Create a new Ban record */
sqlBan* newBan = new (nothrow) sqlBan(SQLDb);
assert( newBan != 0 ) ;

string banTarget = Channel::createBan(theClient);

// TODO: Build a suitable constructor in sqlBan
newBan->setChannelID(theChan->getID());
newBan->setBanMask(banTarget);
newBan->setSetBy(nickName);
newBan->setSetTS(currentTime());
newBan->setLevel(25);

/* Move 360 to config */
newBan->setExpires( 300 + currentTime());
newBan->setReason(theReason);

/*
 *  Check for duplicates, if none found - 
 *  add to internal list and commit to the db.
 */
 
vector< sqlBan* >::iterator ptr = banList->begin(); 
while (ptr != banList->end())
	{
	sqlBan* theBan = *ptr;
		
	if(string_lower(banTarget) == string_lower(theBan->getBanMask()))
		{
			/*
			 * If this mask is already banned, we're just getting
			 * lagged info.
			 */
			return true;
		}
	++ptr;
	}
 
banList->push_back(newBan);

/* Insert this new record into the database. */
newBan->insertRecord();

/*
 * Finally, if this guy is auth'd.. suspend his account.
 */

sqlUser* theUser = isAuthed(theClient, false); 
if (theUser)
	{
	sqlLevel* accessRec = getLevelRecord(theUser, theChan);
	if (accessRec)
		{
		accessRec->setSuspendExpire(currentTime() + 300);
		accessRec->setSuspendBy(nickName);
		accessRec->commit(); 
		}
	}

Channel* netChan = Network->findChannel(theChan->getName());

// Oh dear?
if (!netChan)
	{
	return true;
	}
 
Kick( netChan, theClient, theReason ) ;
 
return true ;
}

/**
 * This method writes a 'channellog' record, recording an event that has
 * occured in this channel. 
 */ 

void cservice::writeChannelLog(sqlChannel* theChannel, iClient* theClient, 
	unsigned short eventType, const string& theMessage)
{ 
sqlUser* theUser = isAuthed(theClient, false);
string userExtra = theUser ? theUser->getUserName() : "Not Logged In";

strstream theLog;
theLog	<< "INSERT INTO channellog (ts, channelID, event, message, "
	<< "last_updated) VALUES "
	<< "("
	<< currentTime()
	<< ", "
	<< theChannel->getID()
	<< ", "
	<< eventType
	<< ", "
 	<< "'["
	<< nickName
	<< "]: "
	<< theClient->getNickUserHost()
	<< " (" << userExtra << ") "
	<< escapeSQLChars(theMessage)
	<< "', "
	<< currentTime()
	<< ")"
	<< ends;

#ifdef LOG_SQL
	elog	<< "cservice::writeChannelLog> "
		<< theLog.str()
		<< endl;
#endif

SQLDb->ExecCommandOk(theLog.str());

delete[] theLog.str();
}

/**
 * Global method to replace ' with \' in strings for safe placement in
 * SQL statements.
 */
const string gnuworld::escapeSQLChars(const string& theString)
{ 
string retMe ;

for( string::const_iterator ptr = theString.begin() ;
	ptr != theString.end() ; ++ptr )
	{
	if( *ptr == '\'' )
		{
		retMe += "\\\047" ;
		}
	else if ( *ptr == '\\' )
		{
		retMe += "\\\134" ;
		}
	else
		{
		retMe += *ptr ;
		}
	}
return retMe ;
}

time_t cservice::currentTime() const
{
/* Returns the current time according to the postgres server. */ 
return dbTimeOffset + ::time(NULL);
} 

int cservice::Notice( const iClient* Target, const string& Message )
{
size_t count = 0 ;

if( Connected && MyUplink )
	{
	setOutputTotal( Target, getOutputTotal(Target) + Message.size() );
	char buffer[512] = { 0 };
	char *b = buffer ;
	const char *m = 0 ;

	// TODO: wtf is this bs?
	for (m=Message.c_str();*m!=0;m++) 
		{
		if (*m == '\n' || *m == '\r') 
			{
			*b='\0';
			count+=MyUplink->Write( "%s O %s :%s\r\n",
				getCharYYXXX().c_str(),
				Target->getCharYYXXX().c_str(),
				buffer ) ; 
			b=buffer;
			}
		else
			*(b++)=*m;
		}
        *b='\0';
	count+=MyUplink->Write( "%s O %s :%s\r\n",
		getCharYYXXX().c_str(),
		Target->getCharYYXXX().c_str(),
		buffer ) ; 
	}
	
return count ;
}

int cservice::Notice( const iClient* Target, const char* Message, ... )
{
if( Connected && MyUplink && Message && Message[ 0 ] != 0 )
	{
	char buffer[ 512 ] = { 0 } ;
	va_list list;

	va_start(list, Message);
	vsprintf(buffer, Message, list);
	va_end(list);
 
	setOutputTotal( Target, getOutputTotal(Target) + strlen(buffer) );
	return MyUplink->Write("%s O %s :%s\r\n",
		getCharYYXXX().c_str(),
		Target->getCharYYXXX().c_str(),
		buffer ) ; 
	}
return -1 ;
}

void cservice::dbErrorMessage(iClient* theClient)
{
Notice(theClient,
	"An error occured while performing this action, "
	"the database may be unavailable. Please try again later."); 
dbErrors++;
}

void cservice::loadPendingChannelList()
{ 
/*
 *  First thing first, if the list has something in it, we want to dump it
 *  out to the database.
 *  Then, clear the list free'ing up memory and finally emptying the list. 
 */

if (pendingChannelList.size() > 0)
{
	pendingChannelListType::iterator ptr = pendingChannelList.begin();

	ExecStatusType beginStatus = SQLDb->Exec("BEGIN;") ; 
	if( PGRES_COMMAND_OK != beginStatus )
	{
		elog << "Error starting transaction." << endl;
	}

	elog << "BEGIN" << endl;
 	
	while (ptr != pendingChannelList.end())
		{
		sqlPendingChannel* pendingChan = ptr->second;

		/* Commit the record */
		pendingChan->commit();

		/* Stop listening on this channels events for now. */
		MyUplink->UnRegisterChannelEvent(ptr->first, this);

		ptr->second = NULL; 
		delete(pendingChan);
		++ptr;
		} /* while() */ 

	ExecStatusType endStatus = SQLDb->Exec("END;") ; 
	if( PGRES_COMMAND_OK != endStatus )
	{
		elog << "Error Ending transaction." << endl;
	}

	elog << "END" << endl;

	pendingChannelList.clear();
} 

/*
 * For simplicity, we assume that if a pending channel is in state "1", then it has 10 valid
 * supporters who have said "Yes" and we're looking at them.
 */

strstream theQuery;
theQuery	<<  "SELECT channels.name, pending.channel_id, user_id, pending.join_count, supporters.join_count, pending.unique_join_count"
			<< " FROM pending,supporters,channels"
			<< " WHERE pending.channel_id = supporters.channel_id"
			<< " AND channels.id = pending.channel_id"
			<< " AND pending.status = 1;"
			<< ends;

#ifdef LOG_SQL
	elog	<< "cmaster::loadPendingChannelList> "
		<< theQuery.str()
		<< endl; 
#endif

ExecStatusType status = SQLDb->Exec(theQuery.str()) ;
delete[] theQuery.str() ;

if( PGRES_TUPLES_OK == status )
	{
	for (int i = 0 ; i < SQLDb->Tuples(); i++)
		{ 
		string chanName = SQLDb->GetValue(i,0);
		sqlPendingChannel* newPending;

		/*
		 *  Is this channel already in our pending list? If so, simply
		 *  lookup and append this supporter to its list.
		 *  If not, create a new 'pending' channel entry.
		 */

		pendingChannelListType::iterator ptr = pendingChannelList.find(chanName);

		if(ptr != pendingChannelList.end()) 
			{
			// It already exists.
			newPending = ptr->second;
			}
				else 
			{
			newPending = new sqlPendingChannel(SQLDb);
			newPending->channel_id = atoi(SQLDb->GetValue(i,1));
			newPending->join_count = atoi(SQLDb->GetValue(i,3));
			newPending->unique_join_count = atoi(SQLDb->GetValue(i,5));
			pendingChannelList.insert( pendingChannelListType::value_type(chanName, newPending) );

			/*
			 *  Lets register our interest in listening on JOIN events for this channel.
			 */
			MyUplink->RegisterChannelEvent(chanName, this);
			}

		/*
		 *  Next, update the internal supporters list.
		 */
		newPending->supporterList.insert(  sqlPendingChannel::supporterListType::value_type(
			atoi(SQLDb->GetValue(i, 2)), atoi(SQLDb->GetValue(i, 4)) )  );
		}
	}

	logDebugMessage("Loaded Pending Channels, there are currently %i channels being traffic monitored.",
		pendingChannelList.size());

#ifdef LOG_DEBUG
	elog	<< "Loaded pending channels, there are currently "
			<< pendingChannelList.size()
			<< " channels being notified and recorded."
			<< endl;
#endif

	/* 
	 * For each pending channel, load up its IP traffic
	 * cache. 
	 */ 

	pendingChannelListType::iterator ptr = pendingChannelList.begin(); 
	while (ptr != pendingChannelList.end())
		{
		sqlPendingChannel* pendingChan = ptr->second;
		pendingChan->loadTrafficCache();
		++ptr;
		};

}

void cservice::checkDbConnectionStatus()
{ 
	if(SQLDb->Status() == CONNECTION_BAD)
	{
		logAdminMessage("\002WARNING:\002 Backend database connection has been lost, attempting to reconnect."); 
		elog	<< "cmaster::cmaster> Attempting to reconnect to database." << endl; 
	
		/* Remove the old database connection object. */ 
		delete(SQLDb);
	
		string Query = "host=" + confSqlHost + " dbname=" + confSqlDb + " port=" + confSqlPort + " user=" + confSqlUser; 
	
		SQLDb = new (nothrow) cmDatabase( Query.c_str() ) ;
	
		assert( SQLDb != 0 ) ;
	 
		if (SQLDb->ConnectionBad())
		{
			elog	<< "cmaster::cmaster> Unable to connect to SQL server."
					<< endl 
					<< "cmaster::cmaster> PostgreSQL error message: "
					<< SQLDb->ErrorMessage()
					<< endl ;
	
			connectRetries++;
			if (connectRetries >= 6) 
			{
				logAdminMessage("Unable to contact database after 6 attempts, shutting down.");
				MyUplink->flushBuffer();
				::exit(0);
			} else
			{
				logAdminMessage("Connection failed, retrying:");
			}
			
			
		} else
		{
				logAdminMessage("Successfully reconnected to database server. Panic over ;)");
		}
	}
 
}
 
void Command::Usage( iClient* theClient )
{
bot->Notice( theClient, string( "SYNTAX: " ) + getInfo() ) ;
}
 
} // namespace gnuworld

