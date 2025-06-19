def Open_Menu() -> None:

    import tkinter as tk

    global events;

    from build.EventBuilder import events

    global gui;

    gui = tk.Tk();

    gui.title( "Python script builder" );

    for event in events:

        event.button = tk.Button( gui, text = event.Description(), bg="green" if event.state else "red", \
                        command=lambda e = event: e.toggle() ); # Needs to create a copy due to python passing on references.

        event.button.pack( pady=5 );

    def start_compiling():

        import sys;

        global gui;

        gui.destroy();

    button = tk.Button( gui, text="All done", bg="purple", command = lambda: start_compiling() );

    button.pack( pady=5 );

    gui.mainloop();
