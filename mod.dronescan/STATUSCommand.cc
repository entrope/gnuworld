/**
 * STATUSCommand.cc
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
 * 2003-07-07	GK@NG	Initial writing
 *
 * $Id: STATUSCommand.cc,v 1.1 2003/07/26 16:47:18 jeekay Exp $
 */

#include "Convert.h"
#include "StringTokenizer.h"

#include "levels.h"
#include "dronescan.h"
#include "dronescanCommands.h"
#include "dronescanTests.h"
#include "sqlUser.h"

RCSTAG("$Id: STATUSCommand.cc,v 1.1 2003/07/26 16:47:18 jeekay Exp $");

namespace gnuworld {

namespace ds {

bool STATUSCommand::Exec( const iClient *theClient, const string& Message, const sqlUser* theUser )
{
	if(theUser->getAccess() < level::status) return false;

	StringTokenizer st(Message);
	
	/* Usage:
	 *  STATUS
	 */
	
	if(st.size() != 1) return false;
	
	time_t uptime = ::time(0) - bot->getUplink()->getStartTime();
	bot->Reply(theClient, "Uptime: %u (%s)",
		uptime,
		Convert::ConvertSecondsToString(uptime).c_str()
		);
	
	bot->Reply(theClient, "Tests:");
	dronescan::testMapType::const_iterator itr = bot->testMap.begin();
	for( ; itr != bot->testMap.end() ; ++itr) {
		bot->Reply(theClient, " %-10s: %s",
			itr->first.c_str(),
			itr->second->getStatus().c_str()
			);
	}
	
	return true;
	
	
} // STATUSCommand::Exec(iClient*, const string&)

} // namespace ds

} // namespace gnuworld