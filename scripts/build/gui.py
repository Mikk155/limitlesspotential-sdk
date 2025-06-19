def Open_Menu() -> None:

    import tkinter as tk

    global events;

    from build.EventBuilder import events;

    from utils.path import Path;

    cache_path: str = Path.enter( "scripts", "build", "cache.json", SupressWarning=True );

    cache: dict[str, bool] = { EV.name: False for EV in events };

    from os.path import exists;

    if exists( cache_path ):

        from json import loads;

        try:

            cache_obj = loads( open( cache_path, "r" ).read() );

            for k, v in cache_obj.items():

                cache[ k ] = v;

        except:

            pass;

    global gui;

    gui = tk.Tk();

    gui.title( "Python script builder" );

    for event in events:

        event.state = cache[ event.name ];

        event.button = tk.Button( gui, text = event.Description(), bg="green" if event.state else "red", \
                        command=lambda e = event: e.toggle() ); # Needs to create a copy due to python passing on references.

        event.button.pack( pady=5 );

    def start_compiling():

        import sys;

        global gui;

        from json import dumps;

        for event in events:

            cache[ event.name ] = event.state;

        open( cache_path, "w" ).write( dumps( cache, indent=4 ) );

        gui.destroy();

    button = tk.Button( gui, text="All done", bg="purple", command = lambda: start_compiling() );

    button.pack( pady=5 );

    gui.mainloop();
