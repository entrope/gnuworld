/* 
 * PARTCommand.cc 
 *
 * 10/02/2001 - David Henriksen <david@itwebnet.dk>
 * Initial Version. Written, and finished.
 *
 * Makes cmaster leave a registered channel.
 *
 * Caveats: None
 *
 * $Id: PARTCommand.cc,v 1.4 2001/02/12 14:07:30 plexus Exp $
 */


#include	<string>
 
#include	"StringTokenizer.h"
#include	"ELog.h" 
#include	"cservice.h" 
#include	"levels.h"
#include	"responses.h"
#include	"Network.h"

const char PARTCommand_cc_rcsId[] = "$Id: PARTCommand.cc,v 1.4 2001/02/12 14:07:30 plexus Exp $" ;

namespace gnuworld
{

using namespace gnuworld;
 
bool PARTCommand::Exec( iClient* theClient, const string& Message )
{ 
	StringTokenizer st( Message ) ;
	if( st.size() < 2 )
	{
		Usage(theClient);
		return true;
	}
 
	/*
	 *  Fetch the sqlUser record attached to this client. If there isn't one,
	 *  they aren't logged in - tell them they should be.
	 */

	sqlUser* theUser = bot->isAuthed(theClient, true);
	if (!theUser) {
		return false;
	}

	/* 
	 *  Check the channel is actually registered.
	 */

	sqlChannel* theChan = bot->getChannelRecord(st[1]);
	if (!theChan) {
		bot->Notice(theClient, bot->getResponse(theUser, language::chan_not_reg).c_str(),
			st[1].c_str());
		return false;
	} 

	/* Check the bot is in the channel. */
 
	if (!theChan->getInChan()) {
		bot->Notice(theClient, "I'm not in that channel!");
		return false;
	}
 
	/*
	 *  Check the user has sufficient access on this channel.
	 */

	int level = bot->getEffectiveAccessLevel(theUser, theChan, true);
	if (level < level::part)
	{
		bot->Notice(theClient, bot->getResponse(theUser, language::insuf_access).c_str());
		return false;
	} 

	sqlLevel* aLevel = bot->getLevelRecord(theUser, theChan);
 
	theChan->setInChan(false); 
	bot->getUplink()->UnRegisterChannelEvent(theChan->getName(), bot);
	
	if(aLevel)
		{
		if(aLevel->getFlag(sqlLevel::F_FORCED))
			{
			bot->Part(theChan->getName(), "At the request of a CService Administrator");
			return true;
			}
		}
	string partReason = "At the request of " + theUser->getUserName();
	bot->Part(theChan->getName(), partReason);	
	return true;
} 

} // namespace gnuworld.
