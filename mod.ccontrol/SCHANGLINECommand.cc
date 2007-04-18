/**
 * SCHANGLINECommand.cc
 * Glines a specific channel
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
 * $Id: SCHANGLINECommand.cc,v 1.1 2007/03/30 09:33:04 mrbean_ Exp $
 */

#include	<string>
#include        <iomanip>
#include	<map>

#include	<cstdlib>

#include	"ccontrol.h"
#include	"CControlCommands.h"
#include	"StringTokenizer.h"
#include	"Network.h"
#include	"misc.h"
#include	"Gline.h"
#include	"ip.h"
#include	"ELog.h"
#include	"Gline.h"
//#include	"gline.h"
#include 	"time.h"
#include	"ccUser.h"
#include	"Constants.h"
#include	"gnuworld_config.h"

RCSTAG( "$Id: SCHANGLINECommand.cc,v 1.1 2007/03/30 09:33:04 mrbean_ Exp $" ) ;

namespace gnuworld
{

using std::string;

// Input: changline #blah reason
//
// Output: C GL * +*@lamer.net 3600 :Banned (*@lamer) ...
//

namespace uworld
{

bool SCHANGLINECommand::Exec( iClient* theClient, const string& Message )
{
	StringTokenizer st(Message);
	unsigned ResStart = 2;

	if (st.size() < 4)
	{
		Usage(theClient);
		return true;
	}

	StringTokenizer::size_type pos = 1;
	time_t gLength = bot->getDefaultGlineLength();
	ccUser* tmpUser = bot->IsAuth(theClient);

	/* log use of the command */
	bot->MsgChanLog("SCHANGLINE %s\n",st.assemble(1).c_str());

	/* make sure they're trying a channel gline! */
	if (st[pos].substr(0,1) != "#")
	{
		bot->Notice(theClient,"Umm... this is SCHANGLINE, not GLINE - Try "
			"glining a channel maybe?");
		return true;
	}
	gLength = extractTime( st[2] );
	if (gLength == 0) 
	{
		gLength = bot->getDefaultGlineLength();
		ResStart = 1;
	}

	string nickUserHost = theClient->getRealNickUserHost();
	
	if (!tmpUser)
	{
		bot->Notice(theClient,"You must login to issue this channel gline!");
		return false;
	}

	if (st[1].size() > channel::MaxName)
	{
		bot->Notice(theClient,"Channel name can't be more than %d "
			"characters", channel::MaxName);
		return false;
	}

	Channel* theChan = Network->findChannel(st[1]);
	if (NULL == theChan)
	{
		bot->Notice(theClient, "Unable to find channel %s",
			st[1].c_str());
		return true;
	}

	bot->glineChannelUsers(theChan,st.assemble( pos + ResStart ), gLength, nickUserHost, false);
	return true;
}

}
}