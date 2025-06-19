class Path:

    from .Logger import Logger;
    m_Logger = Logger( "Path" );

    @staticmethod
    def workspace() -> str:

        '''
            Get the workspace directory
        '''

        from os.path import dirname, abspath;

        return dirname( dirname( abspath( dirname( __file__ ) ) ) );

    @staticmethod
    def enter( *args: tuple[str], SupressWarning = False, CreateIfNoExists = False, CurrentDir = None ) -> str:

        '''
            enter folder names as arguments to join a path
        '''

        from os import makedirs;
        from os.path import join, exists, isfile;

        directory: str = CurrentDir if CurrentDir is not None else Path.workspace();

        for arg in args:

            directory = join( directory, arg );

            if not exists( directory ):

                if CreateIfNoExists:
                    
                    if isfile(directory):

                        Path.m_Logger.warn( "Can NOT CreateIfNoExists \"<c>{}<>\" is not a folder!", directory );

                    else:

                        makedirs( directory, exist_ok=True );

                if SupressWarning is False:

                    Path.m_Logger.warn( "\"<c>{}<>\" doesn't exists!", directory );
        
        return directory;

    @staticmethod
    def steam() -> str:

        '''
            Get the path to the Steam installation
        '''

        from os.path import expandvars, exists, dirname, abspath, join;
        from platform import system;

        OS: str = system();

        DefaultPaths: list[str] = [];

        if OS == "Windows":

            DefaultPaths.append( expandvars( r"%ProgramFiles(x86)%\Steam" ) );
            DefaultPaths.append( expandvars( r"%ProgramFiles%\Steam" ) );

        elif OS == "Linux":

            DefaultPaths.append( "/usr/bin/steam" );
            DefaultPaths.append( "/usr/local/bin/steam" );

        for TryPath in DefaultPaths:

            if exists( TryPath ):

                return dirname( abspath( TryPath ) );

        if OS == "Windows":

            try:

                import winreg;

                with winreg.OpenKey(winreg.HKEY_CURRENT_USER, r"Software\Valve\Steam") as key:

                    TryPath = winreg.QueryValueEx( key, "SteamPath" )[0];

                    return TryPath;

            except ( ImportError, FileNotFoundError, OSError, PermissionError ) as e:

                Path.m_Logger.warn( "Failed to get steam path by winreg on windows: {}", e );

        BuildFolder: str = join( Path.enter( "scripts", "build", CreateIfNoExists=True ), "steam.txt" );

        if not exists( BuildFolder ):

            open( BuildFolder, "w" ).write( 'C:\Program Files (x86)\Steam' );

            Path.m_Logger.error( "Couldn't auto-detect your Steam installation.\n"\
            "A text file has been generated in <c>{}<>\n"\
            "Please input in there the path to Steam for example \"<c>C:\Program Files (x86)\Steam<>\"", BuildFolder, Exit=True );

        return open( BuildFolder, "r" ).read();

    @staticmethod # Assume we've already generated the cmake project for now
    def mod() -> str:

        cache = open( Path.enter( "build", "CMakeCache.txt" ), "r" );

        lines: list[str] = cache.readlines();

        for line in lines:

            if line.startswith( "CMAKE_INSTALL_PREFIX" ):

                return line[ line.find("=") + 1 : ].strip();

        return None;
