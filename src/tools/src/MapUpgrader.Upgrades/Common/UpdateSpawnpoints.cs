using Utilities.Entities;
using Utilities.Tools.UpgradeTool;
using System.Collections.Immutable;

namespace MapUpgrader.Upgrades.Common
{
    /// <summary>
    /// Updates info_player_* entities
    /// </summary>
    internal sealed class UpdateSpawnpoints : MapUpgrade
    {
        private static readonly ImmutableArray<string> ClassNames = ImmutableArray.Create(
            "info_player_start",
            "info_player_deathmatch",
            "info_player_coop"
        );

        protected override void ApplyCore( MapUpgradeContext context )
        {
            foreach( var entity in context.Map.Entities.Where( e => ClassNames.Contains( e.ClassName ) ) )
            {
                /// Renames info_player_* entities to be a multiplayer map (liblist.gam)
                if( entity.ClassName == "info_player_deathmatch" || entity.ClassName == "info_player_coop" )
                {
                    entity.ClassName = "info_player_start_mp";
                }
                else if( entity.ClassName == "info_player_start" )
                {
                    // info_player_start never fired its target before.
                    entity.SetTarget("");
                    // No offset in SP
                    entity.SetInteger( "m_flOffSet", 1 );
                }
            }
        }
    }
}
