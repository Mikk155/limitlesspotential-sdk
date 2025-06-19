from build.EventBuilder import EventBuilder

class InstallContent( EventBuilder ):

    def __init__( self ):

        super().__init__( __name__, (
            "CSharpTools",
            "InstallAssets",
        ) );

    def Description( self ) -> str:
        return 'Execute ContentInstaller';

    def IsMultithread( self ):
        return True;

    def Build( self ) -> None:

        self.m_Logger.info( "Start <g>C# .NET<> Content installation..." );

        from utils.path import Path;

        mod_directory = Path.mod();

        tools_directory = Path.enter( "tools", CurrentDir=mod_directory );

        command: list[str] = [ Path.enter( "ContentInstaller.exe", CurrentDir=tools_directory ), "--mod-directory", mod_directory ];

        self.RunCommand( command, mod_directory );

InstallContent();
