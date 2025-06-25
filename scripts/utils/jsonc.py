class jsonc( dict ):

    def __init__( self, file_path: str, *, exists_ok=None ) -> dict | list:

        """
            Deserialize file into a json dict or list based on the absolute path ``file_path``

            ``exists_ok`` if true and the file doesn't exist we'll create it and initialize a empty dict
        """

        from os.path import exists;

        if exists( file_path ):

            from utils.fmt import fmt;
            from json import loads;

            super().__init__( loads( fmt.PurgeCommentary( open( file = file_path, mode = 'r' ).read() ) ) );

        elif exists_ok:

            open( file_path, 'w' ).write( "{ }" );

            super().__init__( {} );

            return;

        else:

            raise FileNotFoundError( f"Couldn't open file \"{file_path}\"" )

        super().__init__( {} );
