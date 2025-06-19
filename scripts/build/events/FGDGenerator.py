from build.EventBuilder import EventBuilder

class FGDGenerator( EventBuilder ):

    def __init__( self ):
        super().__init__( __name__ );

    def Description( self ) -> str:
        return 'Generate game FGD file';

    def IsMultithread( self ):
        return True;

    def Build( self ) -> None:

        self.m_Logger.info( "Start <g>C# .NET<> FGD generator..." );

        from utils.path import Path;

        path: str = Path.enter( "src", "FGDGenerator" );

        command: list[str] = [ "dotnet", "run" ];

        self.RunCommand( command, path );

    def Cleanup( self ) -> None:

        from utils.path import Path;

        tools_folder = Path.enter( "src", "FGDGenerator" );

        workspace = Path.workspace();

        from shutil import rmtree;

        for folder in [ "bin", "obj" ]:

            path_folder = Path.enter( "bin",  CurrentDir=tools_folder, SupressWarning=True );

            rmtree( path_folder, ignore_errors=True );

            self.m_Logger.debug( "Clearing \"<lg>{}/<>\"", path_folder[ len(workspace) + 1 : ].replace( '\\', '/' ) );

FGDGenerator();
