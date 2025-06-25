from utils.Logger import Logger;
def Initialize( logger: Logger, file: tuple[str, str, str, str], content_blocks: list[str] ) -> bool | str:

    data = {};

    from re import compile, search, Match;

    regex = compile(
        r'RegisterVariable\s*\(\s*Variable\s*\(\s*'
        r'"([^"]+)"\s*,\s*'
        r'([^,]+)\s*'
        r'(?:,\s*([^,]+?)\s*'
        r'(?:,\s*(VarFlags\s*\{[^\}]*\})\s*)?)?'
        r'\)\s*\)'
    );

    for content in content_blocks:

        lines = content.split( "\n" );

        for line in lines:

            regex_match: Match[str] = regex.search( line );

            if regex_match is None:
                continue;

            name = regex_match.group(1);

            default_value = regex_match.group(2).strip();

            type_ = regex_match.group(3)[ 10 : ] if regex_match.group(3) else "Float";

            flags = {};

            if regex_match.group(4):

                _flags = search(r'VarFlags\s*\{(.*)\}', regex_match.group(4) );

                if _flags:

                    for flag_pairs in _flags.group(1).split( "," ):

                        flag_pairs = flag_pairs.strip( " " ).split( "=" );

                        flag_key = flag_pairs[0].strip( " " )[1:];

                        flag_value = flag_pairs[1].strip( " " );

                        flags[ flag_key ] = flag_value;

            Flag_Min = flags[ "Min" ] if "Min" in flags else None;
            Flag_Max = flags[ "Max" ] if "Max" in flags else None;
            Flag_Net = flags[ "network" ] if "network" in flags and flags[ "network" ] is True else None;

            data[ name ] = ( name, default_value, type_, Flag_Min, Flag_Max, Flag_Net, f'docs.ConfigurationVariables.variable.{name}' );

    from utils.path import Path;
    workspace = Path.workspace();

    html = '''<table border="1" cellpadding="5" cellspacing="0">
    <thead>
        <tr>
            <th>docs.ConfigurationVariables.name</th>
            <th>docs.ConfigurationVariables.value</th>
            <th>Type</th>
            <th>docs.ConfigurationVariables.min</th>
            <th>docs.ConfigurationVariables.max</th>
            <th>docs.ConfigurationVariables.network</th>
            <th>Description</th>
        </tr>
    </thead>
<tbody>''';

    for var, vardata in data.items():

        html += f'\n\t<tr>';

        for d in vardata:

            html += '\n\t\t<td>{}</td>'.format( d if d is not None else '' );

        html += f'\n\t</tr>';

    html += f'\n</tbody>\n</table>';

    path = Path.enter( "assets", "docs", "server", "config", "ConfigurationVariables.html" );

    open( path, "w" ).write( html );

    return True;
