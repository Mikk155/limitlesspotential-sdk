# This script has been used to refactore the repository.
# You can see the commit changes at:
#   https://github.com/Mikk155/halflife-unified-sdk/commit/5a0ee1301b394eb2cdf9a7ac442b6f9464d250d5) and [Refactore all parenthesis](https://github.com/Mikk155/halflife-unified-sdk/commit/dea0406cd65848072d2b1e30fbb92ca43e13079d
# 
# This is a simple formatting refactoring to match my liking.
#
# For instance `END_DATAMAP()` will be pushed to the left while `DEFINE_` fields will keep all a indent.
#
# while, for, if. they all will have their open parentheses close without a first space.
# The condition inside will have a space in the open and closed parentheses.
# 
# That's all.
#
# this python script is shit but did the work for "my liking" so now i feel conforted writing.

import os

# Maybe i missed something?
implicit_casts = [
    "void",
    "bool",
    "char",
    "signed char",
    "unsigned char",
    "short",
    "unsigned short",
    "int",
    "unsigned int",
    "long",
    "unsigned long",
    "long long",
    "unsigned long long",
    "float",
    "double",
    "long double",
    "const char",
    "void**",
    "nullptr_t",
    "*",
    "CBaseEntity",
    "CBaseDelay",
    "CBaseToggle",
    "CBaseItem",
    "CBaseMonster",
    "CBasePlayer",
]

skips = [
    "//",
    "#define",
    "#pragma",
    "constexpr",
]

abs_path = os.path.join( os.path.dirname( __file__ ), ".." );
abs_path = os.path.join( abs_path, "src" );
abs_path = os.path.join( abs_path, "game" );

for directory in [ "client", "server", "shared" ]:

    for root, _, files in os.walk( os.path.join( abs_path, directory ) ):

        for file in files:

            if file.endswith( ( ".cpp", ".h" ) ):

                file_path = os.path.join( root, file );

                lines = [];

                with open( file_path, 'r', encoding='utf-8' ) as file:

                    lines = file.readlines();

                    in_open_comment = False;

                    for index_line, line in enumerate(lines.copy()):

                        if not line:
                            continue;

                        if line.find( "/*" ) != -1:
                            in_open_comment = True;
                        
                        if in_open_comment:
                            if line.find( "*/" ) != -1:
                                in_open_comment = False;
                            continue;

                        if any( line.strip( " " ).startswith( T ) for T in skips ) or any( line.strip( "\t" ).startswith( T ) for T in skips ):
                            continue;

                        if line.startswith( "\tEND_DATAMAP();" ):
                            line = line.replace( '\t', '', 1 );
                        elif line.startswith( "DEFINE_" ) and not line.startswith( "DEFINE_DUMMY_DATAMAP" ):
                            line = f"\t{line}";

                        for typeof in [ "while", "for", "if" ]:

                            line = line.replace( f"{typeof} (", f"{typeof}(" );

                        def is_in_string( line_read: str, char_index: int ) -> bool:

                            in_string = False;
                            was_scape = False;
                            last_index = 0;
                            strings = [];

                            for quote_index, char in enumerate(line_read):
                                if char == '"' and not was_scape:
                                    in_string = not in_string;
                                    if not in_string:
                                        strings.append( [ last_index, quote_index ] );
                                    else:
                                        last_index = quote_index;
                                elif char == "\\" and not was_scape:
                                    was_scape = True;
                                    continue;
                                was_scape = False;

                            for indexes in strings:
                                if char_index < indexes[1] and char_index > indexes[0]:
                                    return True;
                            return False;

                        index = 0

                        while index != -1:

                            oldindex = index;

                            index = line.find( "(", index + 1 );

                            comment = line.rfind( "//" );
                            if index == -1 or ( comment != -1 and comment < index ):
                                break;

                            if is_in_string( line, index ):
                                continue;

                            if not line[index+1:index+2] in [ ")", " " ] and line[index+2:index+3] != ")":

                                line = line[0:index+1] + " " + line[index+1:]

                        index = 0

                        while index != -1:

                            oldindex = index;

                            index = line.find( ")", index + 1 );

                            comment = line.rfind( "//" );
                            if index == -1 or ( comment != -1 and comment < index ):
                                break;

                            if is_in_string( line, index ):
                                continue;

                            if not line[index-1:index] in [ "(", " " ] and line[index-2:index-1] != "(":

                                line = line[0:index] + " " + line[ index: ]

                        for casts in implicit_casts:

                            line = line.replace( f"( {casts} )", f"({casts})" );
                            line = line.replace( f"( {casts}* )", f"({casts}*)" );

                        line = line.replace( f"( \"Id Technology\" )", f"(\"Id Technology\")" );
                        line = line.replace( f"( c )", f"(c)" );
                        line = line.replace( f" )\"", f")\"" );
                        line = line.replace( f"R\"( ", f"R\"(" );

                        line = line.replace( '\t', ' ' * 4 );
                        lines[index_line] = line;

                    if lines[len(lines)-1] != "":
                        lines.append( "" );

                try:

                    with open(file_path, 'w', encoding='utf-8') as file:

                        file.writelines( lines );

                        lines = [];

                except Exception as e:

                    print( f"Failed for {file_path} exception: {e}" );
