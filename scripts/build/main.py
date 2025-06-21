# -TODO Threading is horribly broken, but i don't think i want to spam theading for now so i'd leave it for later.

import sys

def FixRelativeImport() -> None:

    '''
        Fix relative library importing by appending the directory of *this* file in the sys path.
    '''

    from os.path import join, abspath, dirname;

    sys.path.append( abspath( join( dirname( __file__ ), '..' ) ) );

FixRelativeImport();

from utils.Logger import Logger;

global gLogger;
gLogger: Logger = Logger( "Global" );

# Import builder events
import build.events.MultiThreading; # MultiThreading should always be at index 0 for indexing.
import build.events.CSharpTools;
import build.events.FGDGenerator;
import build.events.CompileGame;
import build.events.InstallAssets;
import build.events.InstallContent;
import build.events.RunGame; # The last event on the list is special and will only run when everything else is done.

global events;
from build.EventBuilder import events, EventBuilder;

def GithubActivate() -> bool:
    '''
        Github yml workflows pass on arguments so do not open a menu.
    '''

    if len( sys.argv ) < 2:
        return False;

    # Make a mutable copy of the arguments.
    ARGV: list[str] = [ arg[1:] for arg in sys.argv ];

    # Remove the script name argument.
    ARGV.pop(0);

    # Get a copy of events with only the matched names in arguments.
    EVARGV: list[ EventBuilder ] = [ event for event in events if event.name in ARGV ];

    for event in EVARGV:

        # Set enable
        event.state = True;

        # Popout of argument list
        ARGV.pop( ARGV.index( event.name ) );

    # If the ARGV length is greater than 0 then something was not found in the event list
    if len(ARGV) == 1:

        gLogger.error( "Unknown argument \"<g>{}<>\"", ARGV[0], Exit=True );

    elif len(ARGV) > 1:

        gLogger.error( "Unknown arguments: {}", ''.join( f'<g>{arg}<>, ' for arg in ARGV )[:-1], Exit=True );

    return True;

if not GithubActivate():

    from gui import Open_Menu;

    Open_Menu();

if len( [ EV for EV in events if EV.state ] ) == 0:

    gLogger.error( "No active events. Exiting...", Exit=True );

# The class MultiThreading has served his purpose and is not needed anymore
EventBuilder.MultiThread = events.pop(0).state;

def StartBuilding() -> bool:

    # Get a mutable list of only active events.
    EventsCopy: list[EventBuilder] = [ event for event in events if event.state ];

    from time import sleep;

    while len(EventsCopy) > 0:

        EventsCopy = EventBuilder.CompileAllEvents( EventsCopy );

        continue;

        # Here is a dead end loop.
        # Since the event's thread can't set the class's member "compiled" to true.
        # I'd have yet to propertly investigate how multithreading works :)

        if EventBuilder.MultiThread is True:

            if len( EventBuilder.threads ) > 0:

                print( "Threads: {} {}".format( len( EventBuilder.threads ), ''.join( f'{f.name} ' for f in EventBuilder.threads ) ) );
                sleep(0.5);

                # Popoff any that has finished
                for EV in EventBuilder.threads.copy():
                    
                    EV: EventBuilder;

                    if EV.compiled is not None:

                        ThreadEV: EventBuilder = EventBuilder.threads.pop( EventBuilder.threads.index( EV ) );

    CompiledProjects: int = len( [ EV for EV in events if EV.compiled is not None and EV.compiled is True] );

    if CompiledProjects > 1:

        gLogger.info( "<g>All done<>: <r>{}<> projects builded.", CompiledProjects );

    elif CompiledProjects == 1:

        gLogger.info( "<g>All done<>: <r>one<> project builded." );

    else:

        gLogger.error( "<r>{}<> projects builded.", CompiledProjects );

        return False;

    return True;

while StartBuilding() is False:

    input( "Build failed. Press enter to retry" );

input( "Builder ended." );
