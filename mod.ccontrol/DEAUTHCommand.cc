#include	<string>
#include	<cstdlib>
#include        <iomanip.h>

#include	"ccontrol.h"
#include	"CControlCommands.h"
#include	"StringTokenizer.h"

namespace gnuworld
{

using std::string ;

bool DEAUTHCommand::Exec( iClient* theClient, const string& Message)
{
AuthInfo* tmpUser = bot->IsAuth(theClient->getCharYYXXX());
if (!tmpUser) 
{
    bot->Notice(theClient,"Your are not authenticated");
    return false;
}
if(bot->deAuthUser(theClient->getCharYYXXX()))
{
    bot->Notice(theClient,"DeAuthentication successfull");
    return true;
}
else
{
    bot->Notice(theClient,"DeAuthentication failed");
    return false;
}
}	
}