from build.EventBuilder import EventBuilder

class CompileGame( EventBuilder ):

    def __init__( self ):
        super().__init__(__name__ );

    def Description( self ) -> str:
        return 'Compile game libraries';

    def IsMultithread( self ):
        return True;

    def Build( self ) -> bool:

        self.m_Logger.info( "Start compiling <lr>C++<> game libraries..." );

        try:

            from psutil import process_iter;

            # Kill hl.exe if it's running
            for proc in process_iter( ( 'pid', 'name' ) ):

                if proc.info[ 'name' ] == "hl.exe":

                    self.m_Logger.warn( "Program hl.exe is running. Force closing..." );

                    proc.terminate();

                    proc.wait( timeout=5 );

        except:

            pass;

        from shutil import which;

        cmake_exe: str = which( "cmake" );

        from utils.path import Path;

        path: str = Path.enter( "build" );

        command: list[str] = [ cmake_exe, "--build", ".", "--config", "Debug", "--target", "INSTALL" ];

        self.RunCommand( command, path );

CompileGame();
