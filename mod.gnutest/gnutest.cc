/**
 * gnutest.cc
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
 * USA.
 *
 * $Id: gnutest.cc,v 1.14 2003/11/27 02:07:36 dan_karrels Exp $
 */

#include	<string>
#include	<iostream>

#include	"client.h"
#include	"gnutest.h"
#include	"iClient.h"
#include	"StringTokenizer.h"
#include	"EConfig.h"
#include	"Network.h"

RCSTAG("$Id: gnutest.cc,v 1.14 2003/11/27 02:07:36 dan_karrels Exp $");

namespace gnuworld
{

using std::string ;
using std::cout ;
using std::endl ;

/*
 *  Exported function used by moduleLoader to gain an
 *  instance of this module.
 */

extern "C"
{
  xClient* _gnuwinit(const string& args)
  { 
    return new gnutest( args );
  }

} 

gnutest::gnutest( const string& fileName )
 : xClient( fileName )
{
EConfig conf( fileName ) ;
operChan = conf.Require( "operchan" )->second ;
}

gnutest::~gnutest()
{}

void gnutest::OnAttach()
{
//elog	<< "gnutest::OnAttach()" << endl ;
xClient::OnAttach() ;
}

void gnutest::OnDetach( const string& reason )
{
//elog	<< "gnutest::OnDetach("
//	<< reason
//	<< ")" << endl ;
xClient::OnDetach( reason ) ;
}

void gnutest::OnConnect()
{
//elog	<< "gnutest::OnConnect()" << endl ;
xClient::OnConnect() ;
}

void gnutest::OnDisconnect()
{
//elog	<< "gnuteset::OnDisconnect()" << endl ;
xClient::OnDisconnect() ;
}

bool gnutest::BurstChannels()
{
Join( operChan ) ;
MyUplink->RegisterChannelEvent( operChan, this ) ;
return xClient::BurstChannels() ;
}

void gnutest::OnChannelEvent( const channelEventType& whichEvent,
	Channel* theChan,
	void* data1, void* data2, void* data3, void* data4 )
{
if( theChan->getName() != operChan )
	{
	elog	<< "gnutest::OnChannelEvent> Got bad channel: "
		<< theChan->getName() << endl ;
	return ;
	}

iClient* theClient = 0 ;

switch( whichEvent )
	{
	case EVT_CREATE:
//		elog	<< "gnutest::OnChannelEvent> EVT_CREATE\n" ;
	case EVT_JOIN:
//		elog	<< "gnutest::OnChannelEvent> Got EVT_JOIN:
//			<< endl ;
		theClient = static_cast< iClient* >( data1 ) ;

		if( theClient->isOper() )
			{
			Op( theChan, theClient ) ;
			}
		break ;
	default:
		break ;
	}

xClient::OnChannelEvent( whichEvent, theChan,
	data1, data2, data3, data4 ) ;
}

void gnutest::OnEvent( const eventType& whichEvent,
	void* data1, void* data2, void* data3, void* data4 )
{
xClient::OnEvent( whichEvent, data1, data2, data3, data4 ) ;
}

void gnutest::OnChannelMessage( iClient* theClient,
	Channel* theChan,
	const string& message )
{
elog	<< "gnutest::OnChannelMessage> theClient: "
	<< *theClient
	<< ", theChan: "
	<< theChan->getName()
	<< ", message: "
	<< message
	<< endl ;
}

void gnutest::OnPrivateMessage( iClient* theClient,
	const string& message,
	bool )
{
//if( !theClient->isOper() )
//	{
//	elog	<< "gnutest::OnPrivateMessage> Denying access "
//		<< "to non-oper: "
//		<< *theClient
//		<< endl ;
//	return ;
//	}

//elog	<< "gnutest::OnPrivateMessage> Message: "
//	<< message
//	<< ", from client: "
//	<< *theClient
//	<< endl ;

StringTokenizer st( message ) ;
if( st.empty() )
	{
	Notice( theClient, "Are you speaking to me?" ) ;
	return ;
	}

if( st[ 0 ] == "shutdown" )
	{
	MyUplink->Shutdown() ;
	return ;
	}
else if( st[ 0 ] == "reload" )
	{
	Notice( theClient, "Reloading client...see you on the flip side" ) ;

	MyUplink->UnloadClient( this, "Reloading..." ) ;
	MyUplink->LoadClient( "libgnutest", getConfigFileName() ) ;
	return ;
	}

if( st.size() < 2 )
	{
	Notice( theClient, "Are you speaking to me?" ) ;
	return ;
	}

if( st[ 0 ] == "invite" )
	{
	Invite( theClient, st[ 1 ] ) ;
	}
else if( st[ 0 ] == "moo" )
	{
	string raw = st.assemble( 1 ) ;
	Write( raw ) ;
	}
else if( st[ 0 ] == "join" )
	{
	Join( st[ 1 ] ) ;
	addChan( st[ 1 ] ) ;
	}
else if( st[ 0 ] == "part" )
	{
	Part( st[ 1 ] ) ;
	removeChan( st[ 1 ] ) ;
	}
else if( st[ 0 ] == "ban" )
	{
	if( st.size() != 3 )
		{
		Notice( theClient, "Usage: ban #channel nickname" ) ;
		return ;
		}

	Channel* theChan = Network->findChannel( st[ 1 ] ) ;
	if( NULL == theChan )
		{
		Notice( theClient, "Unable to find channel" ) ;
		return ;
		}

	iClient* theClient = Network->findNick( st[ 2 ] ) ;
	if( NULL == theClient )
		{
		Notice( theClient, "Unable to find nickname" ) ;
		return ;
		}

	if( 0 == theChan->findUser( theClient ) )
		{
		Notice( theClient, "The user doesn't appear to be on that channel" ) ;
		return ;
		}

	Ban( theChan, theClient ) ;
	}
else if( st[ 0 ] == "unban" )
	{
	if( st.size() != 3 )
		{
		Notice( theClient, "Usage: unban #channel banmask" ) ;
		return ;
		}

	Channel* theChan = Network->findChannel( st[ 1 ] ) ;
	if( NULL == theChan )
		{
		Notice( theClient, "Unable to find channel" ) ;
		return ;
		}

	if( !theChan->findBan( st[ 2 ] ) )
		{
		Notice( theClient, "Unable to find ban" ) ;
		return ;
		}

	UnBan( theChan, st[ 2 ] ) ;
	}
else if( st[ 0 ] == "bankick" )
	{
	if( st.size() < 4 )
		{
		Notice( theClient, "Usage: bankick #channel nick reason" ) ;
		return ;
		}

	Channel* theChan = Network->findChannel( st[ 1 ] ) ;
	if( NULL == theChan )
		{
		Notice( theClient, "Unable to find channel" ) ;
		return ;
		}

	iClient* theClient = Network->findNick( st[ 2 ] ) ;
	if( NULL == theClient )
		{
		Notice( theClient, "Unable to find nickname" ) ;
		return ;
		}

	if( 0 == theChan->findUser( theClient ) )
		{
		Notice( theClient, "The user doesn't appear to be on that channel" ) ;
		return ;
		}

	BanKick( theChan, theClient, st.assemble( 3 ) ) ;
	}
else if( st[ 0 ] == "op" )
	{
	if( st.size() != 3 )
		{
		Notice( theClient, "Usage: op #channel nick" ) ;
		return ;
		}

	Channel* theChan = Network->findChannel( st[ 1 ] ) ;
	if( NULL == theChan )
		{
		Notice( theClient, "Unable to find channel" ) ;
		return ;
		}

	iClient* theClient = Network->findNick( st[ 2 ] ) ;
	if( NULL == theClient )
		{
		Notice( theClient, "Unable to find nickname" ) ;
		return ;
		}

	if( 0 == theChan->findUser( theClient ) )
		{
		Notice( theClient, "The user doesn't appear to be on that channel" ) ;
		return ;
		}

	Op( theChan, theClient ) ;
	}
else if( st[ 0 ] == "deop" )
	{
	if( st.size() != 3 )
		{
		Notice( theClient, "Usage: deop #channel nick" ) ;
		return ;
		}

	Channel* theChan = Network->findChannel( st[ 1 ] ) ;
	if( NULL == theChan )
		{
		Notice( theClient, "Unable to find channel" ) ;
		return ;
		}

	iClient* theClient = Network->findNick( st[ 2 ] ) ;
	if( NULL == theClient )
		{
		Notice( theClient, "Unable to find nickname" ) ;
		return ;
		}

	if( 0 == theChan->findUser( theClient ) )
		{
		Notice( theClient, "The user doesn't appear to be on that channel" ) ;
		return ;
		}

	DeOp( theChan, theClient ) ;
	}
else if( st[ 0 ] == "schedule" )
	{
	Channel* theChan = Network->findChannel( st[ 1 ] ) ;
	if( NULL == theChan )
		{
		Notice( theClient, "Unable to find channel" ) ;
		return ;
		}

	xServer::timerID id = MyUplink->RegisterTimer( ::time( 0 ) + 60,
		this ) ;
	if( 0 == id )
		{
		Notice( theClient, "Failed" ) ;
		}
	else
		{
		Notice( theClient, "Scheduled for 1 minute from now" ) ;
		timerChan = theChan->getName() ;
		}
	}
else if( st[ 0 ] == "spawnclient" )
	{
	spawnClient( theClient, st ) ;
	}
else if( st[ 0 ] == "removeclient" )
	{
	removeClient( theClient, st ) ;
	}
else if( st[ 0 ] == "spawnserver" )
	{
	spawnServer( theClient, st ) ;
	}
else if( st[ 0 ] == "removeserver" )
	{
	removeServer( theClient, st ) ;
	}

xClient::OnPrivateMessage( theClient, message ) ;
}

void gnutest::OnFakeChannelMessage( iClient* srcClient,
	iClient* destClient,
	Channel* theChan,
	const string& message )
{
//elog	<< "gnutest::OnFakeChannelMessage> srcClient: "
//	<< *srcClient
//	<< ", destClient: "
//	<< *destClient
//	<< ", channel: "
//	<< theChan->getName()
//	<< ", message: "
//	<< message
//	<< endl ;
if( srcClient->getNickName() == "beware" )
	{
	stringstream s ;
	s	<< destClient->getCharYYXXX()
		<< " P "
		<< theChan->getName()
		<< " :"
		<< message
		<< endl ;
//	Write( s.str() ) ;
	}
if( srcClient->getNickName() == "ripper_" )
	{
	stringstream s ;
	s	<< destClient->getCharYYXXX()
		<< " P "
		<< theChan->getName()
		<< " :I agree with ripper_..." ;
//	Write( s.str() ) ;
	}
}

void gnutest::OnFakePrivateMessage( iClient* srcClient,
	iClient* destClient,
	const string& message,
	bool )
{
//elog	<< "gnutest::OnFakePrivateMessage> srcClient: "
//	<< *srcClient
//	<< ", destClient: "
//	<< *destClient
//	<< ", message: "
//	<< message
//	<< ", secure: "
//	<< secure
//	<< endl ;

StringTokenizer st( message ) ;
if( st.size() < 2 )
	{
	return ;
	}

if( st[ 0 ] == "join" )
	{
	// st[ 1 ] exists
	MyUplink->JoinChannel( destClient, st[ 1 ] ) ;
	}
else if( st[ 0 ] == "part" )
	{
	MyUplink->PartChannel( destClient, st[ 1 ],
		"gnutest, the other white meat" ) ;
	}
}

void gnutest::spawnServer( iClient* requestingClient,
	const StringTokenizer& st )
{
if( st.size() < 3 )
	{
	Notice( requestingClient,
		"Usage: spawnserver <name> <description>" ) ;
	return ;
	}

string name( st[ 1 ] ) ;
if( string::npos == name.find( '.' ) )
	{
	Notice( requestingClient, "Server name must have at least "
		"one \'.\'" ) ;
	return ;
	}

string description( st.assemble( 2 ) ) ;

string yyxxx( "00]]]" ) ;
iServer* newServer = new (std::nothrow) iServer(
	getIntYY(),
	yyxxx,
	name,
	::time( 0 ),
	description ) ;
assert( newServer != 0 ) ;

if( !MyUplink->AttachServer( newServer, this ) )
	{
	elog	<< "gnutest::spawnServer> Failed to add new iServer: "
		<< *newServer
		<< endl ;

	Notice( requestingClient, "Failed to add new server" ) ;
	}
else
	{
	elog	<< "gnutest::spawnServer> Added new iServer: "
		<< *newServer
		<< endl ;

	Notice( requestingClient, "Added new server with description: %s",
		description.c_str() ) ;
	}
}

void gnutest::removeServer( iClient* requestingClient,
	const StringTokenizer& st )
{
if( st.size() < 2 )
	{
	Notice( requestingClient, "Usage: removeServer <name>" ) ;
	}

string name( st[ 1 ] ) ;

iServer* theServer = Network->findServerName( name ) ;
if( 0 == theServer )
	{
	elog	<< "gnutest::removeServer> Failed to find server name: "
		<< name
		<< endl ;

	Notice( requestingClient, "Failed to find server: %s",
		name.c_str() ) ;
	return ;
	}

if( !MyUplink->DetachServer( theServer ) )
	{
	elog	<< "gnutest::removeServer> Failed to DetachServer(): "
		<< *theServer
		<< endl ;

	Notice( requestingClient, "Failed to remove server: %s",
		name.c_str() ) ;
	}
else
	{
	elog	<< "gnutest::removeServer> Successfully removed server: "
		<< *theServer
		<< endl ;

	Notice( requestingClient, "Successfully removed server: %s",
		name.c_str() ) ;
	delete theServer ; theServer = 0 ;
	}
}

void gnutest::removeClient( iClient* requestingClient,
	const StringTokenizer& st )
{
if( st.size() != 2 )
	{
	Notice( requestingClient, "Usage: spawnclient <nickname>" ) ;
	return ;
	}

string nickName( st[ 1 ] ) ;

elog	<< "gnutest::removeClient> Removing: "
	<< nickName
	<< endl ;

iClient* removeMe = Network->findFakeNick( nickName ) ;
if( 0 == removeMe )
	{
	Notice( requestingClient, "Unable to find fake client: %s",
		nickName.c_str() ) ;
	return ;
	}

if( MyUplink->DetachClient( removeMe, "Requested shutdown" )  != 0 )
	{
	Notice( requestingClient, "Successfully removed fake client: %s",
		nickName.c_str() ) ;
	delete removeMe ; removeMe = 0 ;
	}
else
	{
	Notice( requestingClient, "Failed to remove fake client: %s",
		nickName.c_str() ) ;
	}
}

void gnutest::spawnClient( iClient* requestingClient,
	const StringTokenizer& st )
{
if( st.size() != 2 )
	{
	Notice( requestingClient, "Usage: spawnclient <nickname>" ) ;
	return ;
	}

string nickName( st[ 1 ] ) ;

elog	<< "gnutest::spawnClient> Spawning "
	<< nickName
	<< endl ;

char newCharYY[ 6 ] ;
newCharYY[ 2 ] = 0 ;
inttobase64( newCharYY, MyUplink->getIntYY(), 2 ) ;

//elog	<< "gnutest::spawnClient> newCharYY: "
//	<< newCharYY
//	<< endl ;

iClient* newClient = new (std::nothrow) iClient(
	MyUplink->getIntYY(), // intYY
	newCharYY, // charYYXXX
	nickName,
	"username",
	"AAAAAA", // host base 64
	"insecurehost.com", // insecureHost
	"realInsecureHost.com", // realInsecureHost
	"+i", // mode
	string(), // account
	"test spawn client, moo", // description
	31337 // connect time
	) ;
assert( newClient != 0 ) ;

if( !MyUplink->AttachClient( newClient, this ) )
	{
	elog	<< "gnutest::spawnClient> Failed to add new client: "
		<< *newClient
		<< endl ;

	Notice( requestingClient, "Failed to create new fake client" ) ;
	delete newClient ; newClient = 0 ;
	}
else
	{
	Notice( requestingClient, "Created new client %s",
		nickName.c_str() ) ;
	elog	<< "gnutest::spawnClient> Added client: "
		<< *newClient
		<< endl ;
	}
}

bool gnutest::isOnChannel( const string& chanName ) const
{
if( !::strcasecmp( chanName.c_str(), operChan.c_str() ) )
	{
	return true ;
	}

return std::find( channels.begin(), channels.end(), chanName )
	!= channels.end() ;
}

bool gnutest::isOnChannel( const Channel* theChan ) const
{
assert( theChan != 0 ) ;

return isOnChannel( theChan->getName() ) ;
}

bool gnutest::addChan( Channel* theChan )
{
assert( theChan != 0 ) ;

return addChan( theChan->getName() ) ;
}

bool gnutest::addChan( const string& chanName )
{
channels.push_back( chanName ) ;
return true ;
}

bool gnutest::removeChan( Channel* theChan )
{
assert( theChan != 0 ) ;

return removeChan( theChan->getName() ) ;
}

bool gnutest::removeChan( const string& chanName )
{
std::remove( channels.begin(), channels.end(), chanName ) ;
return true ;
}

void gnutest::OnTimer( xServer::timerID, void* )
{
Channel* theChan = Network->findChannel( timerChan ) ;
if( NULL == theChan )
	{
	elog	<< "gnutest::OnTimer> Unable to find channel: "
		<< timerChan << endl ;
	return ;
	}

Message( theChan, "Respect my authoritah!" ) ;
}

} // namespace gnuworld
