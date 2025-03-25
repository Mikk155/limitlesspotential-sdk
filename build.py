import os
import sys
import shutil
import filecmp
import subprocess

def exit_freeze( string: str ) -> None:
    print(string);
    input( "Press any key to continue" );
    sys.exit(0);

def CMakeExecutablePath() -> str:
    """
        Find CMake's executable path
    """

    cmake_path: str = shutil.which( "cmake" )

    if cmake_path:

        return cmake_path

    elif "-path" in sys.argv and len(sys.argv) > sys.argv.index( "-path" ) + 1:

        return sys.argv[ sys.argv.index( "-path" ) + 1 ];

    else:

        exit_freeze( \
        f"Failed to get CMake.exe path. You can try provide the full path yourself.\n"\
        f"> python build.py -path \"$absolute path\"" );

def GetBuildDirectory() -> str:
    return os.path.join( os.path.dirname( os.path.abspath( __file__ ) ), "build" );

def CompileProject() -> subprocess.CompletedProcess:
    """
        Compile the Project solution
    """

    build_dir = GetBuildDirectory();

    if not os.path.exists(build_dir):

        exit_freeze( \
        f"Can NOT Find \"build\" Directory\n"\
        f"\"{build_dir}\"\n"\
        f"Did you generated the Project solution with CMake?" );

    cmake_exe: str = CMakeExecutablePath();

    command: list[str] = [ cmake_exe, "--build", ".", "--config", "Debug", "--target", "INSTALL" ];

    try:

        result = subprocess.run(command, cwd=build_dir, check=True, text=True);

        return result;

    except subprocess.CalledProcessError as e:

        exit_freeze( f"CMake Error: {e}" );

def GetModGameDir() -> str:
    """
        Get the installation path
    """
    with open( os.path.join( GetBuildDirectory(), "CMakeCache.txt" ), "r" ) as cache:
        lines: list[str] = cache.readlines();
        for line in lines:
            if line.startswith( "CMAKE_INSTALL_PREFIX" ):
                return line[ line.find("=") + 1 : ].strip()

def InstallAssets() -> None:
    """
        Install game assets
    """

    assets =  os.path.join( os.path.dirname( os.path.abspath( __file__ ) ), "assets" );

    destination = GetModGameDir();

    for root, dirs, files in os.walk( assets ):
        relative_path = os.path.relpath( root, assets )
        dst_path = os.path.join( destination, relative_path )

        if not os.path.exists( dst_path ):

            os.makedirs( dst_path );

        for file in files:

            src_file = os.path.join( root, file );

            dst_file = os.path.join( dst_path, file );

            if not os.path.exists( dst_file ) or not filecmp.cmp( src_file, dst_file, shallow=False ):

                shutil.copy2( src_file, dst_file );

                print( f"Installed: \"{dst_file}\"" );

#            else:

#                print( f"Up-to-date: \"{dst_file}\"" );

if __name__ == "__main__":

    result = CompileProject();

    if result.returncode == 0:

        InstallAssets();

    else:

        exit_freeze( f"{result.stderr}" );
