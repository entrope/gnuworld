/* ccException.h */

#ifndef __CCEXCEPTION_H
#define __CCEXCEPTION_H "$Id: ccException.h,v 1.3 2001/07/23 10:28:51 mrbean_ Exp $"

#include	<string>

#include	<ctime>

#include	"libpq++.h"

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
		{ return (0 == strcasecmp( Host, ExceptionHost )) ; }
	
	inline const string 		getHost() const
			{ return Host; }
		
	inline const int		getConnections() const
		{ return Connections; }
	
	inline const string		getAddedBy() const
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
	
	int		loadData(const string& Host);
	
	int		updateData();
				
	bool		Insert();
	
	bool		Delete();
	
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
