from build.EventBuilder import EventBuilder

class InstallAssets( EventBuilder ):

    def __init__( self ):

        super().__init__( __name__, (
            "CSharpTools",
            "FGDGenerator",
            "CompileGame",
        ) );

    def Description( self ) -> str:
        return 'Install assets';

    def Build( self ) -> None:

        self.m_Logger.info( "Start Game asset installation..." );

        import filecmp;
        from utils.path import Path;
        from shutil import copy2;
        from os import walk;
        from os.path import relpath, join, exists;

        assets: str = Path.enter( "assets" );

        mod_directory = Path.mod();

        ignore_directories = (
            '.git',
            'schemas',
            'cl_dlls',
            'dlls',
            '.',
        );

        Directories: tuple;

        for r, d, f in walk( assets ):
            Directories = d;
            break;

        # Get a tuple of directories excluding the ones specified above
        Directories = ( d for d in Directories if d not in ignore_directories );

        workspace = Path.workspace();

        for Directory in Directories:

            DirectoryPath = Path.enter( Directory, CurrentDir=assets );

            self.m_Logger.info( "Comparing files at \"<lb>{}<>\"", Directory );

            for root, dirs, files in walk( DirectoryPath ):

                relative_path = relpath( root, DirectoryPath );

                dst_path = Path.enter( Directory, relative_path, CreateIfNoExists=True, CurrentDir=mod_directory, SupressWarning=True );

                for file in files:

                    src_file = join( root, file );

                    dst_file = join( dst_path, file );

                    if not exists( dst_file ):

                        self.m_Logger.info( "Installing <g>{}<>", src_file[ len(workspace) + 1 : ].replace( '\\', '/' ) );

                    elif not filecmp.cmp( src_file, dst_file, shallow=False ):

                        self.m_Logger.info( "Updating <g>{}<>", src_file[ len(workspace) + 1 : ].replace( '\\', '/' ) );

                    else:

                        #self.m_Logger.debug( "Up-to-date <g>{}<>", src_file[ len(workspace) + 1 : ].replace( '\\', '/' ) );

                        continue;

                    copy2( src_file, dst_file );

        self.BuildSuscess();

InstallAssets();
