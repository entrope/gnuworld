/* ccException.h */

#ifndef __CCEXCEPTION_H
#define __CCEXCEPTION_H "$Id: ccException.h,v 1.7 2002/06/07 14:38:19 dan_karrels Exp $"

#include	<string>

#include	<ctime>

#include	"libpq++.h"

#include	"match.h"

namespace gnuworld
{ 

using std::string ;

namespace uworld
{

class ccException
{
public:
	
	ccException(PgDatabase* _SQLDb);
	virtual ~ccException();
	
	inline bool operator==( const string& ExceptionHost ) const
		{ return (!strcasecmp( Host, ExceptionHost )
		    || !match(Host.c_str(),ExceptionHost.c_str())) ; }

	inline const string& 		getHost() const
			{ return Host; }
		
	inline const int		getConnections() const
		{ return Connections; }
	
	inline const string&		getAddedBy() const
		{ return AddedBy; }
		
	inline time_t			getAddedOn() const
		{ return AddedOn; }

	inline void 		setHost( const string& _Host ) 
		{ Host = _Host; }
		
	inline void		setConnections( const int _Connections ) 
		{  Connections = _Connections; }
	
	inline void		setAddedBy( const string& _AddedBy) 
		{ AddedBy = _AddedBy; }
		
	inline void		setAddedOn( const time_t _AddedOn )
		{ AddedOn = _AddedOn; }
	
	inline void		setSqldb(PgDatabase* _SQLDb)
		{ SQLDb = _SQLDb; }
		
	int		loadData(const string& Host);
	
	int		updateData();
				
	bool		Insert();
	
	bool		Delete();
	
	static unsigned int numAllocated;

protected:
	
	string 		Host;
	int 		Connections;
	string 		AddedBy;
	time_t		AddedOn;
	PgDatabase* SQLDb;

}; 
}
}
#endif // __CCEXCEPTION_H
