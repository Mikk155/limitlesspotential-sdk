using Utilities.Entities;
using Utilities.Tools.UpgradeTool;
using System.Collections.Immutable;

namespace MapUpgrader.Upgrades.Common
{
    /// <summary>
    /// Rename redundant entity classnames
    /// </summary>
    internal sealed class RenameRedundantEntities : MapUpgrade
    {
        private static readonly Dictionary<string, string> Classnames = new()
        {
            [ "momentary_rot_button" ] = "func_button_momentary",
            [ "button_target" ] = "func_button_target",
            [ "momentary_door" ] = "func_door_momentary",
            [ "func_rot_button" ] = "func_button_rotating",
            [ "func_monsterclip" ] = "func_clip",
            [ "player_loadsaved" ] = "game_loadsaved",
        };

        protected override void ApplyCore( MapUpgradeContext context )
        {
            foreach( string key in Classnames.Keys )
            {
                context.Map.Entities.RenameClass( key, Classnames[ key ] );
            }
        }
    }
}
