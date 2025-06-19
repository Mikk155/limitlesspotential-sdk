from build.EventBuilder import EventBuilder

class MultiThreading( EventBuilder ):

    def __init__( self ):
        super().__init__( __name__ );

    def Description( self ) -> str:
        return 'Call builders on multipe threading';

MultiThreading();
