/*
 * GLINECommand.cc
 *
 * Glines a specific mask 
 */


#include	<string>
#include	<cstdlib>
#include        <iomanip.h>
#include	<map>

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

const char GLINECommand_cc_rcsId[] = "$Id: GLINECommand.cc,v 1.38 2002/03/01 21:31:57 mrbean_ Exp $";

namespace gnuworld
{


using std::string ;

// Input: gline *@blah.net reason
// Input: gline 3600 *@blah.net reason
//
// Output: C GL * +*@lamer.net 3600 :Banned (*@lamer) ...
//

namespace uworld
{

bool GLINECommand::Exec( iClient* theClient, const string& Message )
{
bool Ok = true;
StringTokenizer st( Message ) ;

if(!dbConnected)
        {
        bot->Notice(theClient,"Sorry, but the db connection is down now, please try again alittle later");
        return false;
        }

if( st.size() < 4 )
	{
	Usage( theClient ) ;
	return true ;
	}

StringTokenizer::size_type pos = 1 ;


time_t gLength = bot->getDefaultGlineLength() ;

ccUser* tmpUser = bot->IsAuth(theClient);

bot->MsgChanLog("GLINE %s\n",st.assemble(1).c_str());

bool isChan;
if(st[pos].substr(0,1) == "#")
        isChan = true;
else
	isChan = false; 
string userName;
string hostName;
if(!isChan)
	{
	string::size_type atPos = st[ pos ].find_first_of( '@' ) ;
	if( string::npos == atPos )
		{
		// user has probably specified a nickname (asked by isomer:P)
		iClient* tClient = Network->findNick(st[pos]);
		if(!tClient)
			{
			bot->Notice( theClient, "i can't find %s online, "
				    "please specify a host instead",st[pos].c_str() ) ;
			return true ;
			}
		else
			{
			userName = tClient->getUserName();
			hostName = tClient->getInsecureHost();
			}
		}
	else
		{
		userName = st[ pos ].substr( 0, atPos ) ;
		hostName = st[ pos ].substr( atPos + 1 ) ;
		}
	}
string Length;

Length.assign(st[2]);
unsigned int Units = 1; //Defualt for seconds
unsigned int ResStart = 2;

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
string nickUserHost = bot->removeSqlChars(theClient->getNickUserHost()) ;
	
if(!isChan)
	{
	unsigned int Users;
	if(!tmpUser)
		{
		if((string::npos != hostName.find_first_of("*")) 
		    || (string::npos != hostName.find_first_of("?")) 
		    || ((unsigned)gLength > gline::NOLOGIN_TIME))
			{
			bot->Notice(theClient,"You must login to issue this gline!");
			return true;
			}
		Users = Network->countMatchingUserHost(string(userName + "@" + hostName));
		}
	else
		{
		int gCheck = bot->checkGline(string(userName + "@" + hostName),gLength,Users);
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
		if(gCheck & gline::BAD_TIME)
			{
			bot->Notice(theClient,"Glining for more than %d seconds is a NoNo",gline::MFGLINE_TIME);
			Ok = false;
			}
		if((gCheck & gline::FORCE_NEEDED_HOST) && (Ok))
			{	
			bot->Notice(theClient,"Please use forcegline to gline that host");
			Ok = false;
			}
		if((gCheck & gline::FORCE_NEEDED_TIME) && (Ok))
		    	{
			bot->Notice(theClient,"Please use forcegline to gline for that amount of time");
			Ok = false;
			}
		if((gCheck & gline::FU_NEEDED_USERS) && (Ok))
			{
			bot->Notice(theClient,"This host affects more than %d users, please use forcegline",gline::MFGLINE_USERS);
			Ok = false;
			}
		if((gCheck & gline::FU_NEEDED_TIME) && (Ok))
			{
			bot->Notice(theClient,"Please user forcegline to gline for more than %d second",gline::MFGLINE_TIME);
			Ok = false;
			}
		if((gCheck & gline::FORCE_NEEDED_WILDTIME) && (Ok))
			{
			bot->Notice(theClient,"Wildcard gline for more than %d seconds, must be set with forcegline",gline::MGLINE_WILD_TIME);
			Ok = false;
			}
	    	if(!Ok)
			{
			bot->Notice(theClient,"Please fix all of the above, and try again");
			return false;
			}
		}
	char Us[100];
	Us[0] = '\0';
	sprintf(Us,"%d",Users);
	string Reason = st.assemble( pos + ResStart );
	if(Reason.size() > 255)
		{
		bot->Notice(theClient,"Gline reason can't be more than 255 chars");
		return false;
		}

	//bot->setRemoving(userName + "@" +hostName);
	server->setGline( nickUserHost,
		userName + "@" +hostName,
		string("[") + Us + "] " + Reason,
		//Reason + "[" + Us + "]",
		gLength ,bot) ;
	//bot->unSetRemoving();
	ccGline *TmpGline = bot->findGline(userName + "@" + hostName);
	bool Up = false;
	
	if(TmpGline)
		Up =  true;	
	else TmpGline = new ccGline(bot->SQLDb);
	TmpGline->setHost(bot->removeSqlChars(userName + "@" + hostName));
	TmpGline->setExpires(::time(0) + gLength);
	TmpGline->setAddedBy(nickUserHost);
	TmpGline->setReason(bot->removeSqlChars(st.assemble( pos + ResStart )));
	TmpGline->setAddedOn(::time(0));
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

	return true;
	} //end of regular gline

//Its a channel gline
//ccUser *tmpAuth = bot->IsAuth(theClient);
if(!tmpUser)
	{
	bot->Notice(theClient,"You must login to issue this gline!");
	return false;
	}
if(tmpUser->getType() < operLevel::SMTLEVEL)
	{
	bot->Notice(theClient,"Only smt+ can use the gline #channel command");
	return false;
	}

typedef map<string , int> GlineMapType;
GlineMapType glineList;


if(st[1].size() > channel::MaxName)
	{
	bot->Notice(theClient,"Channel name can't be more than %d chars",channel::MaxName);
	return false;
	}

Channel* theChan = Network->findChannel( st[ 1 ] ) ;
if( NULL == theChan )
	{
	bot->Notice( theClient, "Unable to find channel %s",
		st[ 1 ].c_str() ) ;
	return true ;
	}
ccGline *TmpGline;
iClient *TmpClient;
for( Channel::const_userIterator ptr = theChan->userList_begin();
ptr != theChan->userList_end() ; ++ptr )
	{
	TmpClient = ptr->second->getClient();
	GlineMapType::iterator gptr = glineList.find("*~@" + TmpClient->getInsecureHost());
	if(gptr != glineList.end())
		{
		continue;
		}
	gptr = glineList.find("*" +TmpClient->getUserName() + "@" + TmpClient->getInsecureHost());		
	if(gptr != glineList.end())
		{
		continue;
		}
	if((!TmpClient->getMode(iClient::MODE_SERVICES)) 
	&& !(bot->IsAuth(theClient)) && !(TmpClient->isOper())) 
		{
		TmpGline = new ccGline(bot->SQLDb);
		assert(TmpGline != NULL);
		if(TmpClient->getUserName().substr(0,1) == "~")
			TmpGline->setHost("~*@" + TmpClient->getInsecureHost());
		else
			TmpGline->setHost("*" + TmpClient->getUserName() + "@" + TmpClient->getInsecureHost());
		TmpGline->setExpires(::time(0) + gLength);
		TmpGline->setAddedBy(nickUserHost);
		unsigned int Affected = Network->countMatchingUserHost(TmpGline->getHost()); 
		char Us[20];
		sprintf(Us,"%d",Affected);
		TmpGline->setReason(st.assemble( pos + ResStart ));
		TmpGline->setAddedOn(::time(0));
		TmpGline->Insert();
		TmpGline->loadData(TmpGline->getHost());
		bot->addGline(TmpGline);
//		bot->setRemoving(TmpGline->getHost());
		server->setGline( nickUserHost,
			    TmpGline->getHost(),
			    TmpGline->getReason() + "[" + Us + "]" ,
			    gLength ,bot) ;
//		bot->unSetRemoving();
		glineList.insert(GlineMapType::value_type(TmpGline->getHost(),0));
		}
	}
			
return true ;
}

}
}
