def Open_Menu() -> None:

    import tkinter as tk

    global events;

    from build.EventBuilder import events

    def toggle( btn, var ):

        var.set( not var.get() );

        btn.config( bg="green" if var.get() else "red" );

    global gui;

    gui = tk.Tk();

    gui.title( "Python script builder" );

    for event in events:

        event.variable = tk.BooleanVar();

        event.button = tk.Button( gui, text = event.Description(), bg="green" if event.variable.get() else "red", \
                        command=lambda b= event.variable, btn=None: toggle( event.btn_holder[0], b ) );

        event.btn_holder = [ event.button ]; # In Python the list class is passed as reference

        event.btn_holder[0] = event.button;

        event.button.pack( pady=5 );

    def start_compiling():

        import sys;

        global gui;

        for event in events:

            if event.variable and event.variable.get():

                event.state = True;

        gui.destroy();

    button = tk.Button( gui, text="All done", bg="purple", \
            command=lambda b= tk.BooleanVar(), btn=None: start_compiling() );

    btn_holder = [ button ];

    btn_holder[0] = button;

    button.pack( pady=5 );

    gui.mainloop();
