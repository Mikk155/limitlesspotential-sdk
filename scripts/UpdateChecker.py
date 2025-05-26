#================================================================
#
#   This Python script will use GIT to download game assets and http request to get dlls
#
#   * Requirements:
#       - GitPython>= 3.1.43
#
#   * Arguments:
#
#       -hl ("C:/path/to/Half-Life")
#           + Alternative if the script fails on getting the path automatically.
#
#================================================================

data = (
    # Repository
    "https://github.com/Mikk155/limitlesspotential.git",
    # Branch
    "main",
    # Mod folder
    "limitlesspotential",
    # Your local backups for conflicted files
    "limitlesspotential_backup"
);

import os
import git
import sys
import psutil
import shutil

def GetSteamPath() -> tuple[ str, bool ]:

    '''
    Get steam's installation path
    '''

    from platform import system;

    __OS__ = system();

    if __OS__ == "Windows":

        __paths__ = [
            os.path.expandvars( r"%ProgramFiles(x86)%\Steam" ),
            os.path.expandvars( r"%ProgramFiles%\Steam" )
        ];

        for __path__ in __paths__:

            if os.path.exists( __path__ ):

                return ( __path__, True );

        try:

            import winreg;

            with winreg.OpenKey( winreg.HKEY_CURRENT_USER, r"Software\Valve\Steam" ) as key:

                __path__ = winreg.QueryValueEx(key, "SteamPath")[0];

                return ( __path__, True );

        except ( ImportError, FileNotFoundError, OSError, PermissionError ) as e:

            print( f"Something went wrong using winreg. code: {e}" ); # type: ignore

    elif __OS__ == "Linux":
        __paths__ = [
            "/usr/bin/steam",
            "/usr/local/bin/steam"
        ];

        for __path__ in __paths__:

            if os.path.exists( __path__ ):

                __path__ = os.path.dirname( os.path.abspath( __path__ ) );

                return ( __path__, True );

    else:

        print( f"Unsupported Operative System {__OS__}" );

    return ( "", False );

def GamePath() -> str:

    '''
        Get the Half-Life/limitlesspotential folder
    '''

    game_path = '';

    if "-hl" in sys.argv and len( sys.argv ) > sys.argv.index( "-hl" ):

        game_path = sys.argv[ sys.argv.index( "-hl" ) + 1 ];

    else:

        __STEAM__ = GetSteamPath();

        if not __STEAM__[1]:

            script_path = os.path.split(__file__);

            script_name = script_path[ len(script_path) - 1 ];

            input( f"Couldn't automatically detect steam installation, use the -hl argument\npython {script_name} -hl \"C:/Path/To/Half-Life\"")

            sys.exit(0);
    
        else:

            game_path = os.path.join( __STEAM__[0], "steamapps" );

            game_path = os.path.join( game_path, "common" );

            game_path = os.path.join( game_path, "Half-Life" );

    if not os.path.exists( game_path ):

        input( f"The foder doesn't exists! \"{game_path}\"" );

        sys.exit(0);

    return game_path;

class Progress( git.RemoteProgress ):

    def update( self, op_code, cur_count, max_count, message ):

        if max_count:

            percent = ( cur_count / max_count ) * 100;

            message = f' {message} ' if message is not None else '';

            sys.stdout.write( f"\rProgress: {cur_count}/{max_count} ({percent:.2f}%){message}" );

        else:

            sys.stdout.write( f"\rProgress: {cur_count}/Unknown." );

        sys.stdout.flush();

for proc in psutil.process_iter( [ 'pid', 'name' ] ):

    try:

        if proc.info[ 'name' ] == "hl.exe":

            print( f"Program hl.exe is running. Force closing..." );

            proc.terminate();

            proc.wait( timeout=5 );

    except ( psutil.NoSuchProcess, psutil.AccessDenied, psutil.ZombieProcess ) as e:

        pass;

game_path = GamePath();

mod_path = os.path.join( game_path, data[2] );

try:

    repo = git.Repo( mod_path );

    if repo.bare or 'origin' not in repo.remotes:

        input( f"Invalid repository at {mod_path}\nDelete the folder and try again." )

        sys.exit(0);

    origin = repo.remotes.origin;

    origin.fetch( progress=Progress() );

    commits_behind = repo.iter_commits( f'HEAD..origin/{data[1]}' );

    changes = sum( 1 for _ in commits_behind );

    if changes > 0:

        print( f"Pulling {changes} changes." );

        try:

            origin.pull( progress=Progress() );

        except git.GitCommandError as e:

            if "would be overwritten by merge" in str(e):

                changed_files = [ item.a_path for item in repo.index.diff(None) ];

                changed_files += repo.untracked_files;

                changed_files = list( set( changed_files ) );

                if changed_files:

                    backup_path = os.path.join( game_path, data[3] );

                    print( f"\nThere are file conflicts, your local changes will be moved to {backup_path}" );

                    os.makedirs( backup_path, exist_ok=True );

                    for file in changed_files:

                        src = os.path.join( repo.working_tree_dir, file );

                        dst = os.path.join( backup_path, file );

                        os.makedirs( os.path.dirname( dst ), exist_ok=True );

                        if os.path.isfile( src ):

                            shutil.move( src, dst )

                            print( "  →", file )

                    repo.git.reset( '--hard', f'origin/{data[1]}' );

                    origin.pull( progress=Progress() );

    else:

        print( "Up to date" );

except ( git.GitError ) as e:

    print( f"Couldn't detect any git directory in {mod_path}\nCode: {e}\nCloning {data[0]}" );

    try:

        git.Repo.clone_from( data[0], mod_path, progress=Progress() );

    except Exception as e:

        input( f"Something went wrong cloning. code: {e}" );

input( f"All done!" )
