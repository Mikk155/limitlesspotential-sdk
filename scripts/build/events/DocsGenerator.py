from build.EventBuilder import EventBuilder

class DocsGenerator( EventBuilder ):

    def __init__( self ):
        super().__init__(__name__ );

    def Description( self ) -> str:
        return 'Generate documents';

    def IsMultithread( self ):
        return True;

    def GetAllSourceFiles( self ) -> list[str]:

        AllFiles = [];

        from os import walk;
        from utils.path import Path;

        directory_path = Path.enter( "src", "game" );

        for directory in [ "client", "server", "shared" ]:

            for root, _, files in walk( Path.enter( directory, CurrentDir=directory_path ) ):

                for file in files:

                    if file.endswith( ( ".cpp", ".h" ) ):

                        AllFiles.append( ( Path.enter( file, CurrentDir=root ), file, file[ file.rfind( '/' ) ], file[ file.rfind('.') : ] ) );

        return AllFiles;

    def GetAllDocsGenerators( self ) -> list[ tuple[ str, object ] ]:

        AllFiles: list[ tuple[ str, object ] ] = [];

        from os import walk;
        from utils.path import Path;
        from importlib import util as lib;

        directory_path = Path.enter( "scripts", "DocsGenerator" );

        for root, _, files in walk( directory_path ):

            for file in files:

                if file.endswith( ".py" ):

                    filename = file[: len(file) - 3 ];

                    spec = lib.spec_from_file_location( filename, f"../DocsGenerator/{file}" );

                    obj = lib.module_from_spec( spec );

                    spec.loader.exec_module( obj );

                    AllFiles.append( ( filename, obj ) );

        return AllFiles;

    def Build( self ) -> bool:

        self.m_Logger.info( "Start generating documents..." );

        # -TODO Threading event?

        from os import walk;
        from utils.path import Path;
        from utils.jsonc import jsonc;

        workspace = Path.workspace();

        self.sentences = jsonc( Path.enter( "sentences.json" ) );

        module_name = None;

        try:

            AllGenerators: tuple[ str, object ] = self.GetAllDocsGenerators();

            directory_path = Path.enter( "src", "game" );

            for directory in [ "client", "server", "shared" ]:

                for root, _, files in walk( Path.enter( directory, CurrentDir=directory_path ) ):

                    for file in files:

                        if not file.endswith( ( ".cpp", ".h" ) ):
                            continue;

                        abs_file = Path.enter( file, CurrentDir=root );

                        filedata = ( abs_file, abs_file[ len( workspace ) + 1 : ], file, file[ file.rfind('.') + 1 : ] );

                        content = open( filedata[0], 'r', encoding='utf-8' ).read();

                        DocsGenerators = [ docgen for docgen in AllGenerators if f'DOCGEN {docgen[0]}.py' in content ];

                        for docgen in DocsGenerators:

                            content_blocks = [];

                            index_content = 0;

                            while index_content != -1:

                                index_content = content.find( f'DOCGEN {docgen[0]}.py', index_content + 1 );

                                if index_content != -1:
                                    content_block = content[ content.find( "*/", content.find( f'DOCGEN {docgen[0]}.py', index_content ) ) + 2 : content.find( 'DOCGEN END', index_content ) ];
                                    content_blocks.append( content_block[ : content_block.rfind( '/*' ) ] );

                            module_name = docgen[0];

                            obj_Initialize = docgen[1].Initialize( self.m_Logger, filedata, content_blocks );

                            if isinstance( obj_Initialize, str ):

                                self.BuildFail( f'{module_name}: {obj_Initialize}' );

        except Exception as e:

            self.BuildFail( f'{module_name}: {e}' );

        self.BuildSuscess();

DocsGenerator();
