/*
 * NOTESCommand.cc
 * Leave a note for another user account, or read your own Notes.
 */

#include	<string>
#include	"StringTokenizer.h"
#include	"ELog.h"
#include	"Network.h"
#include	"cservice.h"
#include	"cservice_config.h"

const char NOTECommand_cc_rcsId[] = "$Id: NOTECommand.cc,v 1.1 2002/04/01 22:08:14 gte Exp $" ;

namespace gnuworld
{
using std::string ;

bool NOTECommand::Exec( iClient* theClient, const string& Message )
{

#ifndef USE_NOTES
	return true;
#endif

bot->incStat("COMMANDS.NOTE");

StringTokenizer st( Message ) ;
if( st.size() < 3 )
	{
	Usage(theClient);
	return true;
	}

/* Is the user authorised? */
sqlUser* theUser = bot->isAuthed(theClient, true);
if(!theUser)
	{
	return false;
	}

/*
 * Sending a Note?
 */
if (string_lower(st[1]) == "send")
{
	/*
	 * First, check this person exists.
	 */
	sqlUser* targetUser = bot->getUserRecord(st[2]);
	string message = st.assemble(3);

	if (!targetUser)
		{
		bot->Notice(theClient, "I don't know who %s is.",
			st[2].c_str());
		return false;
		}

	/*
	 * They do? Great! lets see if we're currently allowed to send
	 * a note.
	 *
	 * Firstly, does the target wish to have a nice quiet inbox?
	 */
	if (targetUser->getFlag(sqlUser::F_NONOTES))
		{
		bot->Notice(theClient, "%s doesn't accept Notes.",
			targetUser->getUserName().c_str());
			return false;
		}

	/*
	 * Have we exceeded the maximum number of Notes we can
	 * send right now?
	 */

	if( (unsigned int)(bot->currentTime() - theUser->getLastNote()) >= bot->noteDuration )
		{
		theUser->setLastNote(bot->currentTime());
		theUser->setNotesSent(0);
		}

	if( ((unsigned int)(bot->currentTime() - theUser->getLastNote()) <= bot->noteDuration) && (theUser->getNotesSent() >= bot->noteLimit) )
		{
		bot->Notice(theClient, "You have exceeded the maximum number of notes you can send at this time, please try later.");
		return false;
		}

	/*
	 * Dump the note into the database.
	 */

	/* .. SQL here .. */

	static const char* queryHeader = "INSERT INTO notes (user_id,from_user_id,message,last_updated) VALUES (";

	strstream queryString;
	queryString	<< queryHeader
				<< targetUser->getID() << ", "
				<< theUser->getID() << ", '"
				<< escapeSQLChars(message) << "', "
				<< "now()::abstime::int4);"
				<< ends;

	#ifdef LOG_SQL
		elog	<< "NOTECommand::Insert Note> "
				<< queryString.str()
				<< endl;
	#endif

	ExecStatusType status = bot->SQLDb->Exec(queryString.str()) ;
	delete[] queryString.str() ;

	if( PGRES_COMMAND_OK != status )
		{
		elog	<< "sqlBan::commit> Something went wrong: "
				<< bot->SQLDb->ErrorMessage()
				<< endl;

		bot->Notice(theClient, "An unknown error occured delivering the note.");
		return false ;
		}

	bot->Notice(theClient, "Successfully delivered message to %s!",
		targetUser->getUserName().c_str());

	theUser->setNotesSent(theUser->getNotesSent() + 1);
	return true;
}

/*
 * Reading a Note?
 */
if (string_lower(st[1]) == "read")
{
	if(string_lower(st[2]) == "all")
	{
	/*
	 * Perform a query to list all notes belonging to this user.
	 */
	strstream allNotesQuery;
	allNotesQuery	<< "SELECT users.user_name, notes.message, notes.last_updated, message_id "
					<< "FROM notes,users "
					<< "WHERE notes.from_user_id = users.id "
					<< "AND notes.user_id = "
					<< theUser->getID()
					<< " ORDER BY notes.last_updated ASC"
					<< ends;

	ExecStatusType status = bot->SQLDb->Exec( allNotesQuery.str() ) ;
	delete[] allNotesQuery.str() ;

	if( PGRES_TUPLES_OK != status )
		{
		elog	<< "SUPPORTCommand> SQL Error: "
				<< bot->SQLDb->ErrorMessage()
				<< endl ;

		bot->Notice(theClient, "An unknown error occured while reading your notes.");
		return false ;
		}

	if (bot->SQLDb->Tuples() <= 0)
		{
		bot->Notice(theClient, "You have no notes.");
		return false;
		}

	unsigned int noteCount = bot->SQLDb->Tuples();

	for (unsigned int i = 0 ; i < noteCount; i++)
		{
		string from = bot->SQLDb->GetValue(i,0);
		string theMessage = bot->SQLDb->GetValue(i,1);
		unsigned int when = atoi(bot->SQLDb->GetValue(i,2));
		unsigned int message_id = atoi(bot->SQLDb->GetValue(i,3));

		bot->Notice(theClient, "\002NOTE\002 (Message-Id: %i): Recieved from %s, %s ago : %s",
			message_id, from.c_str(), bot->prettyDuration(when).c_str(), theMessage.c_str());
		}

	}

	bot->Notice(theClient, "To erase an individual note, type /msg %s notes erase <message-id>. To erase all your notes, type /msg %s notes erase all",
		bot->getNickName().c_str(), bot->getNickName().c_str());

	return true;
}

/*
 * Erasing a Note?
 */
if (string_lower(st[1]) == "erase")
{
	if(string_lower(st[2]) == "all")
		{
		strstream queryString;
		queryString	<< "DELETE FROM notes where user_id = "
					<< theUser->getID()
					<< ends;

		#ifdef LOG_SQL
			elog	<< "NOTECommand::Delete Notes> "
					<< queryString.str()
					<< endl;
		#endif

		ExecStatusType status = bot->SQLDb->Exec(queryString.str()) ;
		delete[] queryString.str() ;

		if( PGRES_COMMAND_OK != status )
			{
			bot->Notice(theClient, "An unknown error occured while deleting your notes.");
			return false;
			}

		bot->Notice(theClient, "Successfully erased all your notes.");
		return true;
		}

	/*
	 * TODO: Delete by message-id.
	 */

	return true;
}


return true ;
}

} // namespace gnuworld.