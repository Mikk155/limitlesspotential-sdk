using Utilities.Entities;
using Utilities.Tools.UpgradeTool;
using System.Collections.Immutable;

namespace MapUpgrader.Upgrades.Common
{
    /// <summary>
    /// Converts the old game_player_zone entity to our new trigger_entity_zone
    /// </summary>
    internal sealed class ConvertGameZonePlayer : MapUpgrade
    {
        private static readonly ImmutableDictionary<string, string> KeyValueNames = new Dictionary<string, string>
        {
            ["incount"] = "m_iszInCount",
            ["outcount"] = "m_iszOutCount",
            ["outtarget"] = "message",
            ["intarget"] = "netname",
        }.ToImmutableDictionary();

        protected override void ApplyCore(MapUpgradeContext context)
        {
            foreach( var entity in context.Map.Entities.OfClass( "game_zone_player" ) )
            {
                foreach( var kv in entity.WithoutClassName().ToList() )
                {
                    if( KeyValueNames.TryGetValue( kv.Key, out var className ) )
                    {
                        entity.Remove(kv.Key);
                        entity.SetString(className, kv.Value);
                    }
                }

                // the entity uses to pass the received USE_TYPE so let's use USE_SAME
                entity.SetInteger( "m_InUse", 5 );
                entity.SetInteger( "m_OutUse", 5 );

                // No "fire after action" exist
                entity.Remove( "target" );
            }
        }
    }
}
