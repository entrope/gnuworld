/**
 * SGLINECommand.cc
 * Glines a specific mask 
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
 * $Id: SGLINECommand.cc,v 1.9 2004/03/09 11:50:43 mrbean_ Exp $
 */

#include	<string>
#include        <iomanip>

#include	<cstdlib>

#include	"ccontrol.h"
#include	"CControlCommands.h"
#include	"StringTokenizer.h"
#include	"Network.h"
#include	"misc.h"
#include	"Gline.h"
#include	"ip.h"
#include	"ELog.h"
#include	"Constants.h"
#include	"config.h"

RCSTAG( "$Id: SGLINECommand.cc,v 1.9 2004/03/09 11:50:43 mrbean_ Exp $" ) ;

namespace gnuworld
{

using std::string ;

// Input: sgline *@blah.net reason
// Input: sgline 3600 *@blah.net reason
//
// Output: C GL * +*@lamer.net 3600 :Banned (*@lamer) ...
//
namespace uworld
{

bool SGLINECommand::Exec( iClient* theClient, const string& Message )
{

StringTokenizer st( Message ) ;

if(!dbConnected)
        {
        bot->Notice(theClient,"Sorry, but the db connection is down now, please try again alittle later");
        return false;
        }
if(st.size() < 3)
        {
        Usage( theClient ) ;
        return true ;
        }
StringTokenizer::size_type pos = 1 ;
bool RealName = (!strcasecmp(st[pos],"-rn"));
if(RealName && (st.size() < 4))
	{
	Usage(theClient);
	return true;
	}

string RealHost;


ccUser* tmpUser = bot->IsAuth(theClient);
bot->MsgChanLog("SGLINE %s\n",st.assemble(1).c_str());

if(!RealName &&(string::npos != st[pos].find_first_of('#')))
	{
	bot->Notice(theClient,"I dont think glining that host is such a good idea");
	return true;
	}
time_t gLength = bot->getDefaultGlineLength() ;

// (pos) is the index of the next token, the user@host mask.

if(!RealName)
	{
	string::size_type atPos = st[ pos ].find_first_of( '@' ) ;
	if( string::npos == atPos )
		{
		// User has only specified hostname, not a user name
		bot->Notice( theClient, "GLINE: Please specify gline mask in the "
			"format: user@host" ) ;
		return true ;
		}

	string userName = st[ pos ].substr( 0, pos ) ;
	string hostName = st[ pos ].substr( pos + 1 ) ;
	}
else //RealName Gline
	{
	++pos;
	bool hostOk = false;
	if(st[pos].substr(0,2) != "$R")
		{
		RealHost = "$R" + st[pos];
		}
	else
		{
		RealHost = st[pos];
		}
	//Do alittle sanity check
	for(string::size_type p =2; p< st[pos].size() && !hostOk;++p)
		{
		if(RealHost[p] != '$' && RealHost[p] != '*' && RealHost[p] != '?')
			hostOk = true;
		}
	if(!hostOk)
		{
		bot->Notice(theClient,"You must specify atleast one char other than *?$ as the realname!");
		return true;
		}
    }
    	
string Length;
Length.assign(st[pos+1]);
unsigned int Units = 1; //Defualt for seconds
unsigned int ResStart = 2;
bool Ok = true;
if(!strcasecmp(Length.substr(Length.length()-1).c_str(),"d"))
	{
	Units = 24*3600;
	Length.resize(Length.length()-1);
	}
else if(!strcasecmp(Length.substr(Length.length()-1).c_str(),"h"))
	{
	Units = 3600;
	Length.resize(Length.length()-1);
	}
else if(!strcasecmp(Length.substr(Length.length()-1).c_str(),"m"))
	{
	Units = 60;
	Length.resize(Length.length()-1);
	}
else if(!strcasecmp(Length.substr(Length.length()-1).c_str(),"s"))
	{
	Units = 1;
	Length.resize(Length.length()-1);
	}
gLength = atoi(Length.c_str()) * Units;

if(gLength == 0) 
	{
	gLength = bot->getDefaultGlineLength() ;
	ResStart = 1;
	}
if(!tmpUser)
	{ // We shouldnt have got here in the first place, but check it anyway
	return false;
	}
	
unsigned int Users;
string nickUserHost = bot->removeSqlChars(theClient->getRealNickUserHost()) ;
string Reason = st.assemble( pos + ResStart );

if(!RealName)
	{
	int gCheck = bot->checkSGline(st[pos],gLength,Users);

	if(gCheck & gline::NEG_TIME)
		{
		bot->Notice(theClient,"Hmmz, dont you think that giving a negative time is kinda stupid?");
		Ok = false;
		}	

	if(gCheck & gline::HUH_NO_HOST)
		{
		bot->Notice(theClient,"I dont think glining that host is such a good idea, do you?");
		Ok = false;
		}
	if(gCheck & gline::BAD_HOST)
		{
		bot->Notice(theClient,"illegal host");
		Ok = false;
		}
	if(!Ok)
		{
		bot->Notice(theClient,"Please fix all of the above, and try again");
		return false;

		}

	char Us[100];
	Us[0] = '\0';
	sprintf(Us,"%d",Users);
	Reason = string("[") + Us + string("] ") + Reason;
	} //RealName Gline
if(Reason.size() > gline::MAX_REASON_LENGTH)
	{
	bot->Notice(theClient,"Gline reason can't be more than %d chars",
		    gline::MAX_REASON_LENGTH);
	return false;
	}
/*server->setGline( nickUserHost,
	st[ pos ],
	string("[") + Us + "] " + Reason,
	//st.assemble( pos + ResStart ) + "[" + Us + "]",
	gLength , bot) ;*/
ccGline *TmpGline = bot->findGline(st[pos]);
bool Up = false;

if(TmpGline)
	Up =  true;	
else TmpGline = new ccGline(bot->SQLDb);
if(!RealName)
	TmpGline->setHost(bot->removeSqlChars(st [ pos ]));
else
	TmpGline->setHost(RealHost);
TmpGline->setExpires(::time(0) + gLength);
TmpGline->setAddedBy(nickUserHost);
TmpGline->setReason(Reason);
TmpGline->setAddedOn(::time(0));
TmpGline->setLastUpdated(::time(0));
bot->addGlineToUplink(TmpGline);
if(Up)
	{	
	TmpGline->Update();
	}
else
	{
	TmpGline->Insert();
	//We need to update the Id
	TmpGline->loadData(TmpGline->getHost());
	bot->addGline(TmpGline);
	}

return true ;
}

}
}
