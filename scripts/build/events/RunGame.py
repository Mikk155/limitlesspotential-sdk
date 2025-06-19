from build.EventBuilder import EventBuilder

class RunGame( EventBuilder ):

    def __init__( self ):

        super().__init__( __name__, (
            "InstallContent",
            "InstallAssets",
            "CompileGame",
        ) );

    def Description( self ) -> str:
        return 'Run game';

    def Build( self ) -> None:

        global events;
        from __main__ import events;

        self.m_Logger.info( "Running game..." );

        from utils.path import Path;
        from os import chdir;
        from os.path import dirname, basename;
        from subprocess import Popen;

        mod_directory = Path.mod();

        halflife_directory = dirname( mod_directory );

        mod_name = basename( mod_directory );

        chdir( halflife_directory );

        Popen( [
                "hl.exe",
                "-game", mod_name,
                "-dev",
                "+sv_cheats", "1"
            ]
        );

        self.BuildSuscess();

RunGame();
