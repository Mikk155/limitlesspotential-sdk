using System;
using System.Text;
using System.Collections.Generic;
using System.IO;
using Newtonsoft.Json;
using Newtonsoft.Json.Linq;

#pragma warning disable CS8602 // Dereference of a possibly null reference.
#pragma warning disable CS8600 // Converting null literal or possible null value to non-nullable type.

class Program
{
    /// <summary>
    /// This is the name of the program. Since generating a FGD For Valve Hammer Editor would have issues as it doesn't support some J.A.C.K features.
    /// </summary>
    private static string program = string.Empty;

    /// <summary>
    /// These entities doesn't use any BaseClass and it's data must be on its own class.
    /// </summary>
    private static readonly string[] unique_entities = [ "worldspawn", "infodecal" ];

    /// <summary>
    /// The whole game data objects
    /// </summary>
    private static JObject FGD = [];

    /// <summary>
    /// Contains all entity descriptions with multiple languages.
    /// </summary>
    private static JObject Sentences = [];

    /// <summary>
    /// Contains all entity data. not base classes
    /// </summary>
    private static List<string> entities = [];

    /// <summary>
    /// keep track of writted baseclasses
    /// </summary>
    private static List<string> writed_classes = [];

    /// <summary>
    /// Buffer to write to the .fgd file
    /// </summary>
    private static StringBuilder Buffer = new();

    /// <summary>
    /// Supported languages in sentences.json
    /// </summary>
    private static List<JToken> languages = [ "english" ];

    /// <summary>
    /// Current language label to get from sentences.json
    /// </summary>
    private static string language = string.Empty;

    static string WorkSpace()
    {
        return Path.Combine( AppContext.BaseDirectory[..AppContext.BaseDirectory.LastIndexOf( "FGDGenerator" )], "FGDGenerator" );
    }

    static void WriteFGD()
    {
        string workspace = Program.WorkSpace();

        string fgd_folder = Path.Combine( workspace, $"../../assets/tools/FGD" );
        string fgd_language = Path.Combine( fgd_folder, Program.language );

        if( !Directory.Exists( fgd_language ) )
        {
            Directory.CreateDirectory( fgd_language );
        }

        string fgd_file = Path.Combine( fgd_language, $"halflife-unified-{program}.fgd" );

        File.WriteAllText( fgd_file, Program.Buffer.ToString() );
    }

    static void LoadGameData()
    {
        try
        {
#pragma warning disable CS8601
            Program.Sentences = JsonConvert.DeserializeObject<JObject>( File.ReadAllText( Path.Combine( Program.WorkSpace(), "sentences.json" ) ) );
#pragma warning restore CS8601
        }
        catch( Exception ex )
        {
            Console.WriteLine( $"[ERROR] Invalid json content for \"sentences.json\" {ex.Message}" );
        }

        if( Program.Sentences.ContainsKey( "languages" ) )
        {
            JToken valid_languages = Program.Sentences[ "languages" ];

            if( valid_languages is not null && valid_languages.Count() > 0 )
            {
                languages = [.. valid_languages];
            }
        }

        foreach( string side in new[] { "base", "entities" } )
        {
            string dir_folder = Path.Combine( Program.WorkSpace(), side );

            foreach( string file in Directory.GetFiles( dir_folder, "*.json", SearchOption.AllDirectories ) )
            {
                try
                {
                    string json_content = File.ReadAllText( file );

                    JObject json_objects = JsonConvert.DeserializeObject<JObject>( json_content );

                    if( json_objects == null )
                    {
                        Console.WriteLine( $"[ERROR] Invalid json content for \"{file}\"" );
                        continue;
                    }

                    FGD[ Path.GetFileNameWithoutExtension( file ) ] = json_objects;
                }
                catch( Exception ex )
                {
                    Console.WriteLine( $"[ERROR] Invalid json content for \"{file}\" {ex.Message}" );
                }
            }
        }
    }

    static string GetSentence( string? sentence_field, bool? is_title = false )
    {
        if( sentence_field is not null )
        {
            if( Sentences.ContainsKey( sentence_field ) )
            {
                JObject SentenceField = (JObject)Program.Sentences[ sentence_field ];

                if( SentenceField is not null && SentenceField.ContainsKey(  Program.language ) )
                {
                    return SentenceField[ Program.language ].ToString();
                }
                else
                {
                    Console.WriteLine( $"[WARNING] No sentence \"{Program.language}\" on group named \"{sentence_field}\"" );
                }
            }
            else
            {
                Console.WriteLine( $"[WARNING] No sentence group named \"{sentence_field}\"" );
            }

            if( is_title is not null && is_title == true )
            {
                return $"#{sentence_field}";
            }
        }

        return "";
    }

    static void SortEntities()
    {
        //Console.WriteLine( $"Detected entities:" );

        foreach( KeyValuePair<string, JToken?> jsonObject in FGD )
        {
            string classList = jsonObject.Value?[ "Class" ]?.ToString();

            if( classList is not null && ( classList == "Point" || classList == "Solid" ) )
            {
                //Console.WriteLine( $"{jsonObject.Key}" );
                entities.Add( jsonObject.Key );
            }
        }
        entities.Sort();
    }

    static string Header()
    {
        return $"""
//================================================================================================
//============ Copyright 1996-2005, Valve Corporation, All rights reserved. ===============================
//
// Purpose: Half-Life: Unified SDK game definition file (.fgd) for {Program.program.ToUpper()}
//
// This file has been generated by a script
// https://github.com/Mikk155/halflife-unified-sdk/blob/master/src/FGDGenerator/Program.cs
//
// DO NOT MODIFY THIS FILE, SEE https://github.com/Mikk155/halflife-unified-sdk/tree/master/src/FGDGenerator
//
//================================================================================================
//================================================================================================

""";
    }

    static void WriteKeyValues( string classname, JObject data )
    {
        foreach( KeyValuePair<string, JToken?> DataKeys in data )
        {
            string Key = DataKeys.Key;
            JObject Value = (JObject)DataKeys.Value;

            if( Value is null || Key is null )
                continue;

            if( Key == "spawnflags" )
            {
                if( Value.ContainsKey( "choices" ) )
                {
                    Program.Buffer.Append( $"\n\tspawnflags(flags) =\n\t[" );
                    // -TODO Use "flags" instead of choices.
                    JObject Choices = (JObject)Value["choices"];

                    foreach ( KeyValuePair<string, JToken?> BitsData in Choices )
                    {
                        JToken bits_values = BitsData.Value;

                        if( bits_values is not null )
                        {
                            string title = GetSentence(bits_values[ "title" ]?.ToString(), true);
                            string description = GetSentence(bits_values[ "description" ]?.ToString());
                            //-TODO Default bit set active/inactive value? static 0 now.
                            Program.Buffer.Append( $"\n\t\t{BitsData.Key} : \"{title}\" : 0 : \"{description}\"" );
                        }
                    }
                    Program.Buffer.Append( $"\n\t]\n" );
                }
            }
            else
            {
                if( Program.program == "hammer" )
                {
                    // These are JACK-Only value types.
                    switch( Key )
                    {
                        case "float":
                        case "vector":
                        case "scale":
                        case "sky":
                            Key = "string";
                        break;
                        case "target_generic":
                        case "target_name_or_class":
                            Key = "target_destination";
                        break;
                    }
                }

                string title = GetSentence(Value[ "title" ]?.ToString(), true);
                string description = GetSentence(Value[ "description" ]?.ToString());
                string variable = Value[ "variable" ]?.ToString();
                string value = Value[ "value" ]?.ToString();

                Program.Buffer.Append( $"\n\t{Key}({variable}) : \"{title}\" : " );

                if( variable == "choices" )
                {
                    JToken choices = Value[ "choices" ];

                    Program.Buffer.Append( $"\"{value}\" : \"{description}\" = \n\t[" );

                    foreach ( KeyValuePair<string, JToken?> choice in (JObject)choices )
                    {
                        string choice_title = GetSentence(choice.Value[ "title" ]?.ToString(), true);
                        string choice_description = GetSentence(choice.Value[ "description" ]?.ToString());
                        Program.Buffer.Append( $"\n\t\t\"{choice.Key}\" : \"{choice_title}\" : \"{choice_description}\"" );
                    }
                    Program.Buffer.Append( $"\n\t]" );
                }
                else
                {
                    if( Key == "integer" || Key == "float" )
                    {
                        if( value is null || value == "" )
                        {
                            if( variable == "float" )
                                value = "0.0";
                            else
                                value = "0";
                        }

                        Program.Buffer.Append( $"{value}" );
                    }
                    else
                    {
                        Program.Buffer.Append( $"\"{value}\"" );
                    }

                    Program.Buffer.Append( $" : \"{description}\"" );
                }
            }
        }
    }

    static void WriteClass( string classname, JObject data, List<string>? bases )
    {
        string Class = data[ "Class" ]?.ToString();

        if( Class is null )
        {
            Class = "Base";
            Console.WriteLine( $"[WARNING] Class {classname} has not a \"Class\" value defined. Assuming is a \"Base\"" );
        }
        else if( Class != "Base" && Class != "Point" && Class != "Solid" )
        {
            Console.WriteLine( $"[WARNING] Unsupported \"Class\" \"{Class}\" for {classname}. Skiping." );
            return;
        }

        Program.Buffer.Append( $"\n@{Class}Class " );

        if( classname != "Mandatory" )
        {
            // Get Bases
            string UsingBases = string.Empty;
            if( bases is not null )
            {
                // Mandatory should contain targetname and spawnflag "Not in deathmatch" as all entities support and uses these.
                UsingBases = "base( Mandatory";
                foreach( string KeyBase in bases ) {
                    UsingBases += $", {KeyBase}";
                }
                Program.Buffer.Append( $"{UsingBases} ) " );
            }
        }

        foreach( KeyValuePair<string, JToken?> DataKeys in (JObject)data )
        {
            string Key = DataKeys.Key;
            JToken Value = DataKeys.Value;

            if( Value is null || Key is null || Key == "data" || Key == "$schema" )
                continue;

            switch( Key )
            {
                case "size":
                {
                    List<JToken> size = [.. Value];

                    if( size.Count > 0 )
                    {
                        List<JToken> min = [.. size[0]];
                        List<JToken> max = [];

                        if( size.Count == 2 )
                            max = [.. size[1]];

                        while( min.Count < 3 ) {
                            min.Add( 0 );
                            Console.WriteLine( $"[WARNING] Unset \"size\" for index \"{min.Count}\" for min for {classname}. Setting to 0." );
                        }

                        while( max.Count < 3 ) {
                            max.Add( 0 );
                            Console.WriteLine( $"[WARNING] Unset \"size\" for index \"{min.Count}\" for max for {classname}. Setting to 0." );
                        }

                        Program.Buffer.Append( $"size( {min[0]} {min[1]} {min[2]}, {max[0]} {max[1]} {max[2]} ) " );
                    }

                    break;
                }
                case "color":
                {
                    List<JToken> color = Value.ToList();
                    while( color.Count < 3 ) {
                        color.Add( 255 );
                        Console.WriteLine( $"[WARNING] Unset \"color\" for index \"{color.Count}\" for {classname}. Setting to 255." );
                    }
                    Program.Buffer.Append( $"color( {color[0]} {color[1]} {color[2]} ) " );
                    break;
                }
                case "studio":
                {
                    if( Value.Type == JTokenType.String )
                        Program.Buffer.Append( $"studio( \"{Value}\" ) " );
                    else if( Value is not null && (bool)Value )
                        Program.Buffer.Append( $"studio() " );
                    break;
                }
                case "sprite":
                {
                    if( Value.Type == JTokenType.String )
                        Program.Buffer.Append( $"sprite( \"{Value}\" ) " );
                    else if( Value is not null && (bool)Value )
                        Program.Buffer.Append( $"sprite() " );
                    break;
                }
                case "flags":
                {
                    Program.Buffer.Append( $"flags( \"{Value}\" ) " );
                    break;
                }
                case "iconsprite":
                {
                    Program.Buffer.Append( $"iconsprite( \"{Value}\" ) " );
                    break;
                }
                case "offset":
                {
                    List<JToken> offset = Value.ToList();
                    while( offset.Count < 3 ) {
                        offset.Add( 0 );
                        Console.WriteLine( $"[WARNING] Unset \"offset\" for index \"{offset.Count}\" for {classname}. Setting to 0." );
                    }
                    Program.Buffer.Append( $"offset( {offset[0]} {offset[1]} {offset[2]} ) " );
                    break;
                }
            }
        }

        Program.Buffer.Append( $"= {classname}" );

        // It's an actual entity, write the title and description
        if( Class != "Base" ) {
            Program.Buffer.Append( $" : \"{GetSentence(data["title"]?.ToString(), true)}\" : \"{GetSentence(data["description"]?.ToString())}\"" );
        }

        if( data.ContainsKey( "data" ) ) {
            JToken mydata = data[ "data" ];
            if( mydata is not null ) {
                Program.Buffer.Append( $"\n[" );
                WriteKeyValues( classname, (JObject)mydata );
                Program.Buffer.Append( "\n]\n" );
            }
        }
        else { /* No keyvalues. close right away */
            Program.Buffer.Append( $" []" );
        }

        if( Class == "Solid" && data.ContainsKey( "point" ) )
        {
            JToken ispoint = data[ "point" ];

            if( ispoint is not null && (bool)ispoint )
            {
                data.Remove( "point" );
                data[ "Class" ] = "Point";

                // We really want to add "hulls" to everything? -TODO
                if( bases.Find( x => x == "hulls" ) == null )
                {
                    foreach( string base_name in new[] { "Angles", "Targetx", "Target", "Master", "Global", "Mandatory" } )
                    {
                        int found_class = bases.FindIndex( x => x == base_name );

                        if( found_class != -1 )
                        {
                            bases.Insert( found_class + 1, "hulls" );
                            break;
                        }
                    }
                }

                Program.WriteClass( classname, data, bases );
            }
        }
    }

    static void InspectClass( string classname, JObject data )
    {
        // If it's a special entity then write it right away in one Class.
        if( unique_entities.Contains( classname ) )
        {
            WriteClass( classname, data, null );
            return;
        }

        List<string> BaseClassList = [];

        // Write BaseClasses first.
        if( data.ContainsKey( "base" ) )
        {
            JToken bases = data[ "base" ];

            if( bases is not null )
            {
                foreach( string basename in bases.ToList() )
                {
                    if( FGD.ContainsKey( basename.ToString() ) )
                    {
                        JToken ClassObject = FGD[ basename.ToString() ];

                        if( ClassObject is not null )
                        {
                            Program.InspectClass( basename.ToString(), (JObject)ClassObject );
                        }
                        else
                        {
                            Console.WriteLine( $"Failed to get BaseClass \"{basename}\" for class \"{classname}\"" );
                        }
                    }
                    else if( !writed_classes.Contains( basename.ToString() ) )
                    {
                        Console.WriteLine( $"[WARNING] Invalid BaseClass \"{basename}\" for class \"{classname}\"" );
                    }
                    BaseClassList.Add( basename.ToString() );
                }
            }
            data.Remove( "base" );
        }

        // Convert a entity keyvalues into a own base class so these exclusive appears first and not all bellow in the editor.
        if( Program.entities.Contains( classname ) && data.ContainsKey( "data" ) )
        {
            JToken keyvalues = data[ "data" ];

            if( keyvalues is not null && keyvalues.Any() )
            {
                JObject KeyValuesData = [];
                KeyValuesData[ "data" ] = keyvalues;
                KeyValuesData[ "Class" ] = "Base";
                Program.WriteClass( $"base_{classname}", (JObject)KeyValuesData, null );
                data.Remove( "data" );
                BaseClassList.Insert( 0, $"base_{classname}" );
            }
        }

        Program.WriteClass( classname, data, BaseClassList );

        FGD.Remove( classname );

        Program.writed_classes.Add( classname );
    }

    static void SerializeFGDFile()
    {
        JToken Mandatory = FGD[ "Mandatory" ];

        if( Mandatory is not null )
            Program.InspectClass( "Mandatory", (JObject)Mandatory );

        foreach( string Class in entities )
        {
            JToken ClassObject = FGD[ Class ];

            if( ClassObject is not null )
            {
                Program.InspectClass( Class, (JObject)ClassObject );
            }
        }
    }

    static void Main( string[] args )
    {
        Program.LoadGameData();

        Program.SortEntities();

        foreach( JToken _language in languages )
        {
            Program.language = _language.ToString();

            foreach( string _program in new[] { "hammer", "jack" } )
            {
                Program.program = _program;
                Program.Buffer.Clear();
                Program.Buffer.Append( Program.Header() );

                Console.WriteLine( $"Generating FGD \"{_language}\" for program {_program}" );

                JObject FGD_COPY = (JObject)FGD.DeepClone();
                Program.SerializeFGDFile();
                Program.writed_classes.Clear();
                // -TODO Notice about unserialized classes if any left in FGD
                FGD = FGD_COPY;

                Program.WriteFGD();
            }
        }
    }
}
