/**
 * msg_K.cc
 */

#include	<new>
#include	<string>

#include	<cassert>

#include	"server.h"
#include	"iClient.h"
#include	"Channel.h"
#include	"ChannelUser.h"
#include	"events.h"
#include	"Network.h"

#include	"ELog.h"
#include	"StringTokenizer.h"

const char msg_K_cc_rcsId[] = "$Id: msg_K.cc,v 1.1 2001/02/02 18:10:30 dan_karrels Exp $" ;

using std::string ;
using std::endl ;

namespace gnuworld
{

// AIAAA K #coder-com 0C] :This is now an IRCoperator only channel
// Note that when a user is kicked from a channel, the user is not
// actually parted.  A separate MSG_L message will be issued after
// the MSG_K().
//
int xServer::MSG_K( xParameters& Param )
{

// Verify that there are at least three arguments provided
// client_source_numeric #channel client_target_numeric
if( Param.size() < 3 )
	{
	// Invalid number of arguments
	elog	<< "xServer::MSG_K> Invalid number of arguments\n" ;

	// Return error
	return -1 ;
	}

// Is this a modeless channel?
// TODO: Can there even be kicks on modeless channels?
if( '+' == Param[ 1 ][ 0 ] )
	{
	// Don't care about modeless channels
	return 0 ;
	}

// Find the target client
iClient* theClient = Network->findClient( Param[ 2 ] ) ;

// Did we find the target client?
if( NULL == theClient )
	{
	// Nope, log the error
	elog	<< "xServer::MSG_K> ("
		<< Param[ 1 ] << ") Unable to find client: "
		<< Param[ 2 ] << endl ;

	// Return error
	return -1 ;
	}

// Find the channel in question.
Channel* theChan = Network->findChannel( Param[ 1 ] ) ;

// Did we find the channel?
if( NULL == theChan )
	{
	// Nope, log the error
	elog	<< "xServer::MSG_K> Unable to find channel: "
		<< Param[ 1 ] << endl ;

	// Return error
	return -1 ;
	}

// Remove client<->channel associations
delete theChan->removeUser( theClient ) ;
theClient->removeChannel( theChan ) ;

// All we really have to do here is post the message.
// TODO: Send the source of the kick
PostChannelEvent( EVT_KICK, theChan,
	static_cast< void* >( theClient ) ) ;

// Any users or services clients left in the channel?
if( theChan->empty() && !Network->servicesOnChannel( theChan ) )
	{
	// Nope, remove the channel
	delete Network->removeChannel( theChan->getName() ) ;

	// TODO: Post event
	}

return 0 ;

}

} // namespace gnuworld