/**
 * msg_WA.cc
 * Copyright (C) 2002 Daniel Karrels <dan@karrels.com>
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
 * $Id: msg_WA.cc,v 1.4 2002/07/05 01:10:06 dan_karrels Exp $
 */

#include	"server.h"
#include	"xparameters.h"
#include	"ServerCommandHandler.h"

const char msg_WA_cc_rcsId[] = "$Id: msg_WA.cc,v 1.4 2002/07/05 01:10:06 dan_karrels Exp $" ;
const char server_h_rcsId[] = __SERVER_H ;
const char xParameters_h_rcsId[] = __XPARAMETERS_H ;

namespace gnuworld
{

CREATE_HANDLER(msg_WA)

// WALLOPS
bool msg_WA::Execute( const xParameters& )
{
return false ;
}

} // namespace gnuworld
