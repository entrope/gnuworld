/*
 * msg_CM. cc
 *
 * $Id: msg_CM.cc,v 1.4 2002/05/15 22:14:10 dan_karrels Exp $
 */

#include	<iostream>

#include	"server.h"
#include	"Channel.h"
#include	"ChannelUser.h"
#include	"Network.h"
#include	"iClient.h"
#include	"xparameters.h"
#include	"ELog.h"

const char msg_CM_cc_rcsId[] = "$Id: msg_CM.cc,v 1.4 2002/05/15 22:14:10 dan_karrels Exp $" ;

namespace gnuworld
{

using std::endl ;

/**
 * CLEARMODE message handler.
 * ZZAAA CM #channel obv
 * The above message would remove all ops, bans, and voice modes
 *  from channel #channel.
 */
int xServer::MSG_CM( xParameters& Param )
{

if( Param.size() < 3 )
	{
	elog	<< "MSG_CM> Invalid number of parameters"
		<< endl ;
	return -1 ;
	}

Channel* tmpChan = Network->findChannel( Param[ 1 ] ) ;
if( !tmpChan )
	{
	// Log Error.
	elog	<< "msg_CM> Unable to locate channel: "
		<< Param[ 1 ]
		<< endl ;
	return -1 ;
	}

/*
 * First, determine what we are going to clear.
 */
string Modes = Param[ 2 ] ;

// These two variables will be set to true if we are to clear either
// the ops, voice, or bans, respectively
bool clearOps = false ;
bool clearVoice = false ;
bool clearBans = false ;

for( string::size_type i = 0 ; i < Modes.size() ; i++ )
	{
	switch( Modes[ i ] )
		{
		case 'o':
		case 'O':
			clearOps = true ;
			elog	<< tmpChan->getName()
				<< "msg_CM> Doing CLEAR_CHANOPS"
				<< endl;
			break ;
		case 'v':
		case 'V':
			clearVoice = true ;
			elog	<< tmpChan->getName()
				<< "msg_CM> Doing CLEAR_VOICED"
				<< endl;
			break ;
		case 's':
		case 'S':
			OnChannelModeS( tmpChan, false, 0 ) ;
			elog	<< tmpChan->getName()
				<< "msg_CM> Doing CLEAR_SECRET"
				<< endl;
			break ;
		case 'm':
		case 'M':
			OnChannelModeM( tmpChan, false, 0 ) ;
			elog	<< tmpChan->getName()
				<< "msg_CM> Doing CLEAR_MODERATED"
				<< endl;
			break ;
		case 't':
		case 'T':
			OnChannelModeT( tmpChan, false, 0 ) ;
			elog	<< tmpChan->getName()
				<< "msg_CM> Doing CLEAR_TOPICLIMIT"
				<< endl;
			break ;
		case 'i':
		case 'I':
			OnChannelModeI( tmpChan, false, 0 ) ;
			elog	<< tmpChan->getName()
				<< "msg_CM> Doing CLEAR_INVITEONLY"
				<< endl;
			break ;
		case 'n':
		case 'N':
			OnChannelModeN( tmpChan, false, 0 ) ;
			elog	<< tmpChan->getName()
				<< "msg_CM> Doing CLEAR_NOPRIVMSGS"
				<< endl;
			break ;
		case 'k':
		case 'K':
			OnChannelModeK( tmpChan, false, 0, string() ) ;
			tmpChan->setKey( string() );
			elog	<< tmpChan->getName()
				<< "msg_CM> Doing CLEAR_KEY"
				<< endl;
			break ;
		case 'b':
		case 'B':
			clearBans = true ;
			tmpChan->removeAllBans();
			elog	<< tmpChan->getName()
				<< "msg_CM> Doing CLEAR_BANS"
				<< endl;
			break ;
		case 'l':
		case 'L':
			OnChannelModeL( tmpChan, false, 0, 0 ) ;
			elog	<< tmpChan->getName()
				<< "msg_CM> Doing CLEAR_LIMIT"
				<< endl;
			break ;
		default:
			// Unknown mode
			elog	<< "msg_CM> Unknown mode: "
				<< Modes[ i ]
				<< endl ;
			break ;
		} // switch
	} // for

if( clearOps || clearVoice )
	{
	/*
	 * Lets loop over everyone in the channel and either deop
	 * or devoice them.
	 */
	xServer::opVectorType opVector ;
	xServer::voiceVectorType voiceVector ;

	for( Channel::const_userIterator ptr = tmpChan->userList_begin();
		ptr != tmpChan->userList_end() ; ++ptr )
		{
		if( clearOps )
			{
			ptr->second->removeModeO();
			opVector.push_back( pair< bool, ChannelUser* >
				( false, ptr->second ) ) ;
			}
		if( clearVoice )
			{
			ptr->second->removeModeV();
			voiceVector.push_back( pair< bool, ChannelUser* >
				( false, ptr->second ) ) ;
			}
		}

	if( !voiceVector.empty() )
		{
		OnChannelModeV( tmpChan, 0, voiceVector ) ;
		}
	if( !opVector.empty() )
		{
		OnChannelModeO( tmpChan, 0, opVector ) ;
		}
	} // if( clearOps || clearVoice )

if( clearBans )
	{
	xServer::banVectorType banVector ;

	for( Channel::banIterator ptr = tmpChan->banList_begin(),
		endPtr = tmpChan->banList_end() ;
		ptr != endPtr ; ++ptr )
		{
		banVector.push_back( pair< bool, string >
			( false, *ptr ) ) ;
		}

	OnChannelModeB( tmpChan, 0, banVector ) ;
	} // if( clearBans )

return 0 ;
}

} // namespace gnuworld
