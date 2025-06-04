import os
import sys
import stat
import shutil
import psutil
import filecmp
import colorama
import subprocess

colorama.init();

'''
    Options to execute through this script
'''
Options: dict[ str, list[ bool, str ] ] = {
    "CompileGameTools": [ False, "Compile CSharp tool kit" ],
    "CompileGameForgeGameData": [ False, "Generate game FGD file" ],
    "CompileGameSDK": [ False, "Compile game libraries" ],
    "RunGame": [ False, "Install and run game" ]
};

OptionIndexes: tuple[str] = tuple( Options.keys() );

# To know if the script should wait for inputs
UserInterface: bool = True;

# If used by command line allow to direct set
for arg in sys.argv:

    OptionName: str = arg[ 1: ];

    if OptionName in Options:

        OptionLabel: list[ bool, str ] = Options[ OptionName ];

        OptionLabel[0] = True;

        Options[ OptionName ] = OptionLabel;

        UserInterface = False;

if UserInterface:

    user_input = "dummy";

    while user_input != "":

        print_message = f"{colorama.Fore.LIGHTBLUE_EX}Python scr{colorama.Fore.LIGHTYELLOW_EX}ipt compiler";

        for index, option in enumerate( OptionIndexes ):

            OptionCurrent = Options[option];

            print_message += f'\n\t{colorama.Fore.YELLOW}{index + 1}{colorama.Fore.WHITE}: [{colorama.Fore.GREEN if OptionCurrent[0] else colorama.Fore.RED}{OptionCurrent[0]}{colorama.Fore.WHITE}]\t{OptionCurrent[1]}';

        print( f"{print_message}\n- Select a option and press enter" );

        user_input = input();

        if user_input.isdigit():

            OptionSelected = int(user_input) - 1;

            if len(OptionIndexes) > OptionSelected:

                key: str = OptionIndexes[OptionSelected];

                value: list[ bool, str ] = Options[ key ];

                value[0] = False if value[0] else True;

                Options[ key ] = value;

        os.system( 'cls' );

ErrorNumbers = 0;

if Options[ "CompileGameTools" ][0]:

    print( f"{colorama.Fore.LIGHTMAGENTA_EX}Start compiling C# .NET tool kit...{colorama.Fore.WHITE}" );

    command: list[str] = [ "dotnet", "build", "-c", "Release", "-o", "./../../assets/tools" ];

    try:

        tools_folder = os.path.join( os.path.dirname( os.path.abspath( __file__ ) ), "src/tools" );

        if subprocess.run(command, cwd=tools_folder, check=True, text=True).returncode != 0:

            ErrorNumbers += 1;

        else:

            for root, dirs, files in os.walk( tools_folder, topdown=False ):

                for folder_name in ( "bin", "obj" ):

                    if folder_name in dirs:

                        folder_path = os.path.join( root, folder_name );

                        
                        if os.path.exists( folder_path ):

                            for dirpath, dirnames, filenames in os.walk( folder_path ):

                                for name in filenames:

                                    filepath = os.path.join( dirpath, name );

                                    os.chmod( filepath, stat.S_IWRITE );

                            shutil.rmtree( folder_path, ignore_errors=True );

    except subprocess.CalledProcessError as e:

        print( f"{colorama.Fore.RED}.NET Error: {colorama.Fore.WHITE}{e}" );

        ErrorNumbers += 1;

if Options[ "CompileGameForgeGameData" ][0]:

    print( f"{colorama.Fore.LIGHTMAGENTA_EX}Start C# .NET game FGD generator...{colorama.Fore.WHITE}" );

    command: list[str] = [ "dotnet", "run" ];

    try:

        tools_folder = os.path.join( os.path.dirname( os.path.abspath( __file__ ) ), "src/FGDGenerator" );

        if subprocess.run(command, cwd=tools_folder, check=True, text=True).returncode != 0:

            ErrorNumbers += 1;

        else:

            shutil.rmtree( os.path.join( tools_folder, "bin" ), ignore_errors=True );
            shutil.rmtree( os.path.join( tools_folder, "obj" ), ignore_errors=True );

    except subprocess.CalledProcessError as e:

        print( f"{colorama.Fore.RED}.NET Error: {colorama.Fore.WHITE}{e}" );

        ErrorNumbers += 1;

if Options[ "CompileGameSDK" ][0]:

    print( f"{colorama.Fore.LIGHTMAGENTA_EX}Start C++ Game compilation...{colorama.Fore.WHITE}" );

    try:

        # Kill hl.exe if it's running
        for proc in psutil.process_iter( [ 'pid', 'name' ] ):

            if proc.info[ 'name' ] == "hl.exe":

                print( f"{colorama.Fore.RED}Program hl.exe is running. Force closing...{colorama.Fore.WHITE}" );

                proc.terminate();

                proc.wait( timeout=5 );

    except ( psutil.NoSuchProcess, psutil.AccessDenied, psutil.ZombieProcess ):

        pass;

    cmake_exe: str = shutil.which( "cmake" )

    if not cmake_exe or cmake_exe is None:

        if "-path" in sys.argv and len(sys.argv) > sys.argv.index( "-path" ) + 1:

            cmake_exe = sys.argv[ sys.argv.index( "-path" ) + 1 ];

        else:

            print( \
            f"{colorama.Fore.RED}Failed to get CMake.exe path. You can try provide the full path yourself.\n"\
            f"> python build.py -path \"$absolute path\"{colorama.Fore.WHITE}" );

            ErrorNumbers += 1;

    if cmake_exe:

        if not os.path.exists(cmake_exe):

            print( \
            f"{colorama.Fore.RED}Can NOT Find \"build\" Directory\n"\
            f"\"{cmake_exe}\"\n"\
            f"Did you generated the Project solution with CMake?{colorama.Fore.WHITE}" );

            ErrorNumbers += 1;

        else:

            command: list[str] = [ cmake_exe, "--build", ".", "--config", "Debug", "--target", "INSTALL" ];

            try:

                if subprocess.run(command, cwd=os.path.join( os.path.dirname( os.path.abspath( __file__ ) ), "build" ), check=True, text=True).returncode != 0:

                    ErrorNumbers += 1;

            except subprocess.CalledProcessError as e:

                print( f"{colorama.Fore.RED}CMake Error: {colorama.Fore.WHITE}{e}" );

                ErrorNumbers += 1;

if Options[ "RunGame" ][0] and ErrorNumbers == 0:

    print( f"{colorama.Fore.LIGHTMAGENTA_EX}Start assets installation...{colorama.Fore.WHITE}" );

    assets =  os.path.join( os.path.dirname( os.path.abspath( __file__ ) ), "assets" );

    mod_directory = None;

    with open( os.path.join( os.path.join( os.path.dirname( os.path.abspath( __file__ ) ), "build" ), "CMakeCache.txt" ), "r" ) as cache:

        lines: list[str] = cache.readlines();

        for line in lines:

            if line.startswith( "CMAKE_INSTALL_PREFIX" ):

                mod_directory = line[ line.find("=") + 1 : ].strip()

    for root, dirs, files in os.walk( assets ):

        if '.git' in root or 'cl_dlls' in root or 'dlls' in root:

            continue;

        relative_path = os.path.relpath( root, assets );

        dst_path = os.path.join( mod_directory, relative_path );

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

    halflife_directory = os.path.dirname( mod_directory );

    mod_name = os.path.basename( mod_directory );

    old_directory = os.path.dirname( __file__ );

    os.chdir( halflife_directory );

    subprocess.Popen( [
            "hl.exe",
            "-game", mod_name,
            "-dev",
            "+sv_cheats", "1"
        ]
    );

input( f"All done. {ErrorNumbers} errors" );
