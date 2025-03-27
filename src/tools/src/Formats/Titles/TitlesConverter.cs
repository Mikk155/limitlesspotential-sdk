using Utilities.Tools;
using Newtonsoft.Json;
using Serilog;
using System.Text;
using System.Text.RegularExpressions;
using Microsoft.VisualBasic;

namespace Formats.Titles
{
    /// <summary>
    /// Converts original Half-Life <c>titles.txt</c> data to <c>titles.json</c>.
    /// </summary>
    public static class TitlesConverter
    {
        public static void Convert(Stream inputStream, Stream outputStream, ILogger logger)
        {
            using var reader = new StreamReader(inputStream);
            using var writer = new JsonTextWriter(new StreamWriter(outputStream));
            List<string> saved_titles = new();

            Dictionary<string, string> titles = new();
            Dictionary<string, object> keyValuePairs = new();

            writer.Formatting = Formatting.Indented;
            writer.Indentation = 1;
            writer.IndentChar = '\t';

            writer.WriteStartObject();

            int lineNumber = 0;

            string? line;

            bool capsulated = false;
            string message = "";
            string name = "";

            while ((line = reader.ReadLine()) != null)
            {
                ++lineNumber;

                if( string.IsNullOrEmpty(line) || line.StartsWith( "//" ) )
                    continue ;

                if( capsulated )
                {
                    if( line[..1] == "}" )
                    {
                        if( name != "" )
                        {
                            if( saved_titles.Contains( name ) )
                            {
                                logger.Warning( $"Title defined twice! {name}" );
                            }
                            else
                            {
                                saved_titles.Append( name );
                                writer.WritePropertyName( name );
                                writer.WriteStartObject();
                                writer.WritePropertyName( "english" );
                                writer.WriteValue( message );
                                foreach( var key in keyValuePairs.Keys )
                                {
                                    writer.WritePropertyName( key );

                                    var value = keyValuePairs[ key ];

                                    if( value is List<int> )
                                    {
                                        List<int>? colors = value as List<int>;
                                        if( colors is not null )
                                        {
                                            writer.WriteStartArray();
                                            foreach( int color in colors )
                                            {
                                                writer.WriteValue(color);
                                            }
                                            writer.WriteEndArray();
                                        }
                                    }
                                    else
                                    {
                                        writer.WriteValue( value );
                                    }
                                }
                                writer.WriteEndObject();
//                                logger.Information( $"{name}: {message}" );
                            }
                        }
                        capsulated = false;
                        message = name = "";
                    }
                    else
                    {
                        if( message != "" )
                            message = $"{message}\n";
                        message = $"{message}{line}";
                    }
                }
                else if( line[..1] == "{" )
                {
                    capsulated = true;
                }
                else if( line.StartsWith( "$" ) )
                {
                    line = line[1..];

                    string[] VarSplit = line.Split( " " );

                    if( VarSplit.Length == 0 )
                        continue;

                    string VarName = line[..VarSplit[0].Length ];

                    switch( VarName )
                    {
                        case "position":
                        {
                            if( VarSplit.Length > 1 && float.TryParse( VarSplit[1], out float x ) )
                                keyValuePairs[ "x" ] = x;
                            if( VarSplit.Length > 2 && float.TryParse( VarSplit[2], out float y ) )
                                keyValuePairs[ "y" ] = y;
                            break;
                        }
                        case "effect":
                        {
                            if( VarSplit.Length > 1 && int.TryParse( VarSplit[1], out int effect ) )
                                keyValuePairs[ "effect" ] = effect;
                            break;
                        }
                        case "color":
                        {
                            if( keyValuePairs.ContainsKey( "color" ) )
                            {
#pragma warning disable CS8600, CS8601, CS8602
                                List<int> color = keyValuePairs["color"] as List<int>;

                                string[] colors = VarSplit[1..];

                                List<int> parsed_colors = new();

                                foreach( var col in colors )
                                    if( int.TryParse( col, out int value ) )
                                        parsed_colors.Add(value);

                                for( int i = 0; i < parsed_colors.Count && i < parsed_colors.Count; i++ )
                                {
                                        color[i] = parsed_colors[i];
                                    }
                                    keyValuePairs[ "color" ] = color;
#pragma warning restore CS8600, CS8601, CS8602
                                }
                            else
                            {
                                keyValuePairs[ "color" ] = new List<int> { 255, 255, 255, 255 };
                            }
                            break;
                        }
                        case "fadein":
                        {
                            if( VarSplit.Length > 1 && float.TryParse( VarSplit[1], out float fadein ) )
                                keyValuePairs[ "fadein" ] = fadein;
                            break;
                        }
                        case "fadeout":
                        {
                            if( VarSplit.Length > 1 && float.TryParse( VarSplit[1], out float fadeout ) )
                                keyValuePairs[ "fadeout" ] = fadeout;
                            break;
                        }
                        case "holdtime":
                        {
                            if( VarSplit.Length > 1 && float.TryParse( VarSplit[1], out float hold ) )
                                keyValuePairs[ "hold" ] = hold;
                            break;
                        }
                    }
                }
                else
                {
                    name = line.Trim();
                }
            }

            writer.WriteEndObject();
        }
    }
}
