/**
 * msg_Q.cc
 * Copyright (C) 2002 Daniel Karrels <dan@karrels.com>
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
 * $Id: msg_Q.cc,v 1.7 2002/07/05 01:10:06 dan_karrels Exp $
 */

#include	<iostream>

#include	"server.h"
#include	"events.h"
#include	"Network.h"
#include	"iClient.h"
#include	"Network.h"
#include	"ELog.h"
#include	"ServerCommandHandler.h"

const char msg_Q_cc_rcsId[] = "$Id: msg_Q.cc,v 1.7 2002/07/05 01:10:06 dan_karrels Exp $" ;
const char server_h_rcsId[] = __SERVER_H ;
const char events_h_rcsId[] = __EVENTS_H ;
const char Network_h_rcsId[] = __NETWORK_H ;
const char iClient_h_rcsId[] = __ICLIENT_H ;
const char ELog_h_rcsId[] = __ELOG_H ;
const char xParameters_h_rcsId[] = __XPARAMETERS_H ;

namespace gnuworld
{

using std::endl ;

CREATE_HANDLER(msg_Q)

/**
 * A client has quit.
 * QAE Q :Signed off
 */
bool msg_Q::Execute( const xParameters& Param )
{

if( Param.size() < 1 )
	{
	elog	<< "msg_Q> Invalid number of parameters"
		<< endl ;
	return false ;
	}

// xNetwork::removeClient will remove user<->channel associations
iClient* theClient = Network->removeClient( Param[ 0 ] ) ;
if( NULL == theClient )
	{
	elog	<< "msg_Q> Unable to find client: "
		<< Param[ 0 ]
		<< endl ;
	return false ;
	}

theServer->PostEvent( EVT_QUIT, static_cast< void* >( theClient ) ) ;

// xNetwork::removeClient() will remove channel->user associations.
delete theClient ;

return true ;
}

} // namespace gnuworld
