from build.EventBuilder import EventBuilder

class CSharpTools( EventBuilder ):

    def __init__( self ):
        super().__init__( __name__ );

    def Description( self ) -> str:
        return 'Compile CSharp tool kit';

    def IsMultithread( self ):
        return True;

    def Build( self ) -> None:

        self.m_Logger.info( "Start compiling <g>C# .NET<> tool kit..." );

        from utils.path import Path;

        path: str = Path.enter( "src", "tools" );

        command: list[str] = [ "dotnet", "build", "-c", "Release", "-o", "./../../assets/tools" ];

        self.RunCommand( command, path );

    def Cleanup( self ) -> None:

        from utils.path import Path;

        tools_folder = Path.enter( "src", "tools", SupressWarning=True );

        workspace = Path.workspace();

        from shutil import rmtree;
        from stat import S_IWRITE;
        from os import walk, chmod;
        from os.path import join, exists;

        for root, dirs, files in walk( tools_folder, topdown=False ):

            for folder_name in ( "bin", "obj" ):

                if folder_name in dirs:

                    folder_path = join( root, folder_name );

                    if exists( folder_path ):

                        for dirpath, dirnames, filenames in walk( folder_path ):

                            for name in filenames:

                                filepath = join( dirpath, name );

                                chmod( filepath, S_IWRITE );

                        rmtree( folder_path, ignore_errors=True );

                        self.m_Logger.debug( "Clearing \"<lg>{}/<>\"", folder_path[ len(workspace) + 1 : ].replace( '\\', '/' ) );

CSharpTools();
