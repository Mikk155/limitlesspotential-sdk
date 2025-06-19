from build.EventBuilder import EventBuilder

class MultiThreading( EventBuilder ):

    def __init__( self ):
        super().__init__( __name__ );

    def Description( self ) -> str:
        return 'Build on multi-threading (broken)';

MultiThreading();
