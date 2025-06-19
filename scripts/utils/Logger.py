from colorama import init as colorama
global colorama_init;
colorama_init: bool = False;

class Logger():

    name: str = "Global";

    class Level:

        DEBUG = ( 1 << 0 );
        INFORMATION = ( 1 << 1 );
        WARNING = ( 1 << 2 );
        # Error is unused and always ON.
        ERROR = ( 1 << 3 );

    level: int = ( Level.DEBUG | Level.INFORMATION | Level.WARNING );

    @staticmethod
    def SetLevel( self, level: Level ):

        if Logger.level & level:

            Logger.level &= level;

        else:

            Logger.level |= level;

    def __init__( self, name: str ):

        global colorama_init;
        if not colorama_init:
            colorama();
            colorama_init = True;

        self.name = name;

    from colorama import Fore as c;

    def log( self, levelname: str, color: str, message: str, level: Level, *args ) -> str:

        if not Logger.level & level and level != Logger.Level.ERROR:
            return;

        ColorList = {
            "R": Logger.c.RED,
            "G": Logger.c.GREEN,
            "Y": Logger.c.YELLOW,
            "B": Logger.c.BLUE,
            "M": Logger.c.MAGENTA,
            "C": Logger.c.CYAN,
            "LR": Logger.c.LIGHTRED_EX,
            "LG": Logger.c.LIGHTGREEN_EX,
            "LY": Logger.c.LIGHTYELLOW_EX,
            "LB": Logger.c.LIGHTBLUE_EX,
            "LM": Logger.c.LIGHTMAGENTA_EX,
            "LC": Logger.c.LIGHTCYAN_EX,
            # Aliases
            "red": Logger.c.RED,
            "green": Logger.c.GREEN,
            "yellow": Logger.c.YELLOW,
            "blue": Logger.c.BLUE,
            "magenta": Logger.c.MAGENTA,
            "cyan": Logger.c.CYAN,
            "lightred": Logger.c.LIGHTRED_EX,
            "lightgreen": Logger.c.LIGHTGREEN_EX,
            "lightyellow": Logger.c.LIGHTYELLOW_EX,
            "lightblue": Logger.c.LIGHTBLUE_EX,
            "lightmagenta": Logger.c.LIGHTMAGENTA_EX,
            "lightcyan": Logger.c.LIGHTCYAN_EX,
        };

        for pv, v in ColorList.items():

            pv = [ pv.upper(), pv.lower() ];

            for p in pv:

                while f'<{p}>' in message:

                    message = message.replace( f'<{p}>', v, 1 );
                    message = message.replace( f'<>', Logger.c.RESET, 1 );

        from datetime import datetime;

        now = datetime.now();

        message = '[{}{}{}:{}{}{}:{}{}{}] [{}{}{}] [{}{}{}] {}'.format(
            Logger.c.YELLOW, now.hour, Logger.c.RESET,
            Logger.c.YELLOW, now.minute, Logger.c.RESET,
            Logger.c.YELLOW, now.second, Logger.c.RESET,
            color, levelname, Logger.c.RESET, Logger.c.CYAN,
            self.name, Logger.c.RESET, message.format( *args )
        );

        print( message );

    def debug( self, message: str, *args ):
        self.log( "debug", self.c.CYAN, message, Logger.Level.DEBUG, *args );

    def info( self, message: str, *args ):
        self.log( "info", self.c.GREEN, message, Logger.Level.INFORMATION, *args );

    def warn( self, message: str, *args ):
        self.log( "warning", self.c.YELLOW, message, Logger.Level.WARNING, *args );

    def error( self, message: str, *args, Exit:bool = False ):
        self.log( "error", self.c.RED, message, Logger.Level.ERROR, *args );
        if Exit:
            exit(1);
