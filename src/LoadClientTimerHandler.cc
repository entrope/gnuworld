/* LoadClientTiemrHandler.cc */

#include	"LoadClientTimerHandler.h"
#include	"server.h"
#include	"ELog.h"

const char LoadClientTimerHandler_h_rcsId[] = __LOADCLIENTTIMERHANDLER_H ;
const char LoadClientTimerHandler_cc_rcsId[] = "$Id: LoadClientTimerHandler.cc,v 1.1 2001/06/14 22:14:13 dan_karrels Exp $" ;
const char ELog_h_rcsId[] = __ELOG_H ;

namespace gnuworld
{

int LoadClientTimerHandler::OnTimer( timerID, void* )
{
elog	<< "LoadClientTimerHandler::OnTimer("
	<< moduleName
	<< ", "
	<< configFileName
	<< ")"
	<< endl ;

// Load the client from the module, attach it to the server, and
// burst it onto the network
theServer->AttachClient( moduleName, configFileName, true ) ;

delete this ;
return 0 ;
}

} // namespace gnuworld