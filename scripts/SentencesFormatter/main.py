import sys

def FixRelativeImport() -> None:

    '''
        Fix relative library importing by appending the directory of *this* file in the sys path.
    '''

    from os.path import join, abspath, dirname;

    sys.path.append( abspath( join( dirname( __file__ ), '..' ) ) );

FixRelativeImport();

from utils.path import Path;
from utils.jsonc import jsonc;
from utils.Logger import Logger;

gLogger: Logger = Logger( "Sentences" )

Sentences: dict[ str, dict[ str, str ] ] = jsonc( Path.enter( "sentences.json" ) );

SupportedLanguages: list[str] = Sentences.pop( "languages" );

ErrorCount: int = 0;

for Language in SupportedLanguages:

    for SentenceName, SentenceGroup in Sentences.copy().items():

        if not Language in SentenceGroup:

            gLogger.warn( "Missing language <r>{}<> in sentence group <g>{}<>", Language, SentenceName );

            # Sentences[ Language ] = "";

            ErrorCount += 1;

        elif SentenceGroup[ Language ] == '':

            gLogger.warn( "Empty language <r>{}<> in sentence group <g>{}<>", Language, SentenceName );

            ErrorCount += 1;

if ErrorCount > 1:

    gLogger.error( "<r>{}<> errors found.", ErrorCount );

if ErrorCount > 0:

    gLogger.error( "<r>one<> error found." );

gLogger.info( "All done." );

input();
