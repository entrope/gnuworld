/* SHOWIGNORECommand.cc */

#include	<string>
 
#include	"StringTokenizer.h"
#include	"ELog.h" 
#include	"cservice.h" 

const char SHOWIGNORECommand_cc_rcsId[] = "$Id: SHOWIGNORECommand.cc,v 1.3 2001/01/29 04:07:51 gte Exp $" ;

namespace gnuworld
{

using namespace gnuworld;
 
bool SHOWIGNORECommand::Exec( iClient* theClient, const string& Message )
{ 
	StringTokenizer st( Message ) ;
	if( st.size() < 1 )
	{
		Usage(theClient);
		return true;
	}

	for( cservice::silenceListType::const_iterator ptr = bot->silenceList.begin() ;
		ptr != bot->silenceList.end() ; ++ptr )
	{
		bot->Notice(theClient, "%s", ptr->second.c_str());
	}

	bot->Notice(theClient, "-- End of Silence List");
	return true ;
} 

} // namespace gnuworld.
