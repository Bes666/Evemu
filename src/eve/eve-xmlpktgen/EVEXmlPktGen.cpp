/**
 * @file EVEXmlPktGen.cpp
 *
 * This file is part of EVEmu: EVE Online Server Emulator.<br>
 * Copyright (C) 2006-2011 The EVEmu Team<br>
 * For the latest information visit <i>http://evemu.org</i>.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA, or go to
 * <i>http://www.gnu.org/copyleft/lesser.txt</i>.
 *
 * @author Zhur
 */

#include "EVEXmlPktGen.h"

#include "XmlPacketGenerator.h"

void usage();

int main( int argc, char* argv[] )
{
#ifdef HAVE_CRTDBG_H
    // Setup memory leak detection
    _CrtSetDbgFlag( _CRTDBG_LEAK_CHECK_DF | _CrtSetDbgFlag( _CRTDBG_REPORT_FLAG ) );
#endif /* HAVE_CRTDBG_H */

    // Setup logging to console
    sLog.add( new log::Console );

    sLog.debug( "eve-xmlpktgen", "Logging initialized" );

    // skip first argument, not interested
    --argc;
    ++argv;

    std::string dirInclude = ".";
    std::string dirSource = ".";

    // parse options
    for(; 0 < argc; --argc, ++argv )
    {
        if( '-' != argv[0][0] )
            // end of options
            break;

        switch( argv[0][1] )
        {
            /* include dir */
            case 'I':
                if( 1 < argc )
                {
                    // consume next argument
                    dirInclude = argv[1];
                    --argc, ++argv;
                }
                else
                {
                    sLog.error( "eve-xmlpktgen", "Error parsing options: no parameter for include dir" );
                    return -1;
                }
                break;

            /* source dir */
            case 'S':
                if( 1 < argc )
                {
                    // consume next argument
                    dirSource = argv[1];
                    --argc, ++argv;
                }
                else
                {
                    sLog.error( "eve-xmlpktgen", "Error parsing options: no parameter for source dir" );
                    return -1;
                }
                break;

            /* help */
            case 'h':
                usage();
                return 0;

            /* error */
            default:
                sLog.error( "eve-xmlpktgen", "Unknown option '%c'", argv[0][1] );
                return -1;
        }
    }

    if( 0 == argc )
    {
        sLog.error( "eve-xmlpktgen", "Error processing files: no files given" );
        return -1;
    }

    // process files
    XmlPacketGenerator gen;
    for(; 0 < argc; --argc, ++argv )
    {
        std::string name = *argv;

        // locate a slash
        size_t slash = name.rfind( '/' );
        if( std::string::npos == slash )
            slash = name.rfind( '\\' );
        if( std::string::npos == slash )
            slash = -1;

        // locate a dot
        size_t dot = name.rfind( '.' );
        if( slash > dot || std::string::npos == dot )
            dot = name.length();

        name = name.substr( slash + 1, dot - slash - 1 );

        gen.SetHeaderFile( ( dirInclude + '/' + name + ".h" ).c_str() );
        gen.SetSourceFile( ( dirSource + '/' + name + ".cpp" ).c_str() );

        if( !gen.ParseFile( *argv ) )
            return -1;
    }

    return 0;
}

void usage()
{
    printf( "Usage: eve-xmlpktgen [OPTIONS] FILE [FILE [...]]\n"
            "\n"
            "Options:\n"
            "  -I directory    Output header files to directory\n"
            "  -S directory    Output source files to directory\n"
            "  -h              Show this help and exit\n"
    );
}
