class EventBuilder:

    name: str = "EventBuilder";

    from utils.Logger import Logger;
    m_Logger: Logger;

    # This is the state, changes by either gui buttons or sys arguments
    state: bool = False;

    compiled: bool = None;

    # Current thread if any
    from threading import Thread;
    thread: Thread = None;

    threads: list[object] = [];

    from subprocess import Popen;
    process: Popen  = None;

    queued: bool = False;

    MultiThread: bool = False;

    Requires: tuple[str] = ();

    def __init__( self, name: str, Requires = () ):

        self.Requires = Requires;

        name = name.split('.');
        name = name[ len(name) - 1 ];

        self.name = name;

        from utils.Logger import Logger;
        self.m_Logger = Logger( self.name );

        global events;

        events.append( self );

        mem = str(self).split();

        self.mem = mem[ len(mem) - 1 ][:-1];

        self.m_Logger.debug( "Initialized event at index <r>{}<> mem(<m>{}<>)", events.index(self), self.mem );

    def toggle( self ) -> None:

        self.state = not self.state;

        self.m_Logger.debug("Toggled state to <{}>{}<>", 'g' if self.state else 'r', str(self.state) );

        self.button.config( bg="green" if self.state else "red" );

    def Description( self ) -> str:
        '''Called to display information on menu'''
        return '';

    def IsMultithread( self ) -> bool:
        '''Return true if this is multi-threading'''
        return False;

    def RequiredEvents( self ) -> tuple[object]:

        if len( self.Requires ) == 0:
            return ();

        return tuple( EV for EV in events if EV.state and EV.name in self.Requires and EV.compiled is not True );

    def RunCommand( self, args: list[str], path: str ) -> None:
        '''
            Runs a command, if multithreading is on it will start a thread. This method will call BuildSuscess or BuildFail on its own
        '''

        try:

            if EventBuilder.MultiThread is True:

                from tempfile import NamedTemporaryFile;

                bat = '@echo off\n{}\nif %ERRORLEVEL% neq 0 (\n\tpause\n\texit /b 1\n)\nexit /b 0\n'.format( ''.join( f'{arg} ' for arg in args ) );

                with NamedTemporaryFile( mode="w", suffix=".bat", delete=False ) as bat_file:

                    bat_file.write( bat );

                    bat = bat_file.name;

                from subprocess import Popen, CREATE_NEW_CONSOLE;

                self.process = Popen( bat, cwd=path, shell=True, creationflags=CREATE_NEW_CONSOLE );

                self.process.wait();

                if self.process.returncode != 0:

                    self.BuildFail( self.process.stderr ); # Thread fails to set the main's thread class member "compiled"
                    return;

            else:

                from subprocess import run;

                process = run( args, cwd=path, text=True );

                if process.returncode != 0:

                    self.BuildFail( process.stdout );
                    return;

        except Exception as e:

            self.BuildFail( e );
            return;

        self.BuildSuscess();

    def BuildFail( self, error: str ) -> None:

        '''
            Call to tell the script that the build has failed

            ``error`` error message
        '''

        if error is not None:

            self.m_Logger.error( "Object failed compilation: <r>{}<>", error );

        else:

            self.m_Logger.error( "Object failed compilation" );

        self.compiled = False;


    def BuildSuscess( self ) -> None:

        '''
            Call to tell the script that the build has suscess
        '''

        self.compiled = True;

    def Build( self ) -> None:
        '''
            Called when a event should start building. use BuildSuscess or BuildFail to tell the project has been built suscessfuly or not.
        '''

        e = SyntaxError( "Event \"{}\" doesn't override the Build method".format( self.name ) );

        raise e;

    def Cleanup( self ) -> None:
        '''Called after Activate, use to cleanup stuff'''

    @staticmethod
    def CompileAllEvents( EVs: list[object] ) -> list[object]:

        '''
            Compile all events in a single thread
        '''

        EVs: list[EventBuilder] = EVs; # This is only for decorator, remove.

        for EV in EVs.copy():

            EV: EventBuilder;

            if EventBuilder.MultiThread is True:

                Requires: list[EventBuilder] = [ _EV for _EV in events if _EV.state and _EV.name in EV.Requires and _EV not in EventBuilder.threads ];

                if len(Requires) > 0:

                    if not EV.queued:

                        EV.m_Logger.info( "Can't compile yet due to uncompleted requires events \"{}\"".format( ''.join( f'<g>{_EV.name}<>, ' for _EV in Requires )[:-2] ) );

                        EV.queued = True;

                    continue;

            EVs.pop( EVs.index( EV ) );

            EV.m_Logger.debug( "Building object (<m>{}<>)...", EV.mem );

            HasRequired: tuple[EventBuilder] = EV.RequiredEvents();

            if len(HasRequired) == 0:

                if EventBuilder.MultiThread is True and EV.IsMultithread():

                    import threading;

                    EV.thread = threading.Thread( EV.Build() );

                    EV.thread.start();

                    EventBuilder.threads.append( EV );

                else:

                    EV.Build();

                    if EV.compiled is not None:

                        if EV.compiled is True:

                            EV.m_Logger.debug( "Cleanin up object (<m>{}<>)...", EV.mem );

                            EV.Cleanup();

                    else:

                        e = SyntaxError( "Event \"{}\" doesn't call any BuildSuscess or BuildFail methods!".format( EV.name ) );

                        raise e;

            else:

                EV.m_Logger.error( "Couldn't build. Required events has failed: {}".format( ''.join( f'<g>{f.name}<> ' for f in HasRequired ) ) );

        return EVs;

global events;
events: list[ EventBuilder ] = [];
