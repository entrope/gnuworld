/*
 * DEAUTHCommand.cc
 *
 * Deauthenticates a user
 *
 */

#include	<string>
#include	<cstdlib>
#include        <iomanip.h>

#include	"ccontrol.h"
#include	"CControlCommands.h"
#include	"StringTokenizer.h"

const char DEAUTHCommand_cc_rcsId[] = "$Id: DEAUTHCommand.cc,v 1.7 2001/12/23 09:07:57 mrbean_ Exp $";

namespace gnuworld
{

using std::string ;

namespace uworld
{

bool DEAUTHCommand::Exec( iClient* theClient, const string& Message)
{
//Fetch the user authenticate entry
ccUser* tmpUser = bot->IsAuth(theClient->getCharYYXXX());
if (!tmpUser) 
	{
	bot->Notice(theClient,"Your are not authenticated");
	return false;
	}
if(bot->deAuthUser(tmpUser))
	{
	bot->Notice(theClient,"DeAuthentication successfull");
	bot->MsgChanLog("(%s) - %s : Deauthenticated\n",tmpUser->getUserName().c_str()
			,theClient->getNickUserHost().c_str());
	return true;
	}
else
	{
	bot->Notice(theClient,"DeAuthentication failed");
	return false;
	}
}	

}
}
