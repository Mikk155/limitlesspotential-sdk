using Utilities.Entities;
using Utilities.Tools.UpgradeTool;

namespace MapUpgrader.Upgrades.Common
{
    internal sealed class AddAchievementsHL : MapUpgrade
    {
        protected override void ApplyCore(MapUpgradeContext context)
        {
            switch( context.Map.BaseName )
            {
                case "c1a0d":
                {
                    Entity entity = context.Map.Entities.CreateNewEntity( "game_achievement" );
                    entity.SetString( "targetname", "suit_cd_audio" );
                    entity.SetString( "m_label", "hl_suit" );
                    entity.SetString( "m_name", "Suit up" );
                    entity.SetString( "m_description", "Get the H.E.V suit" );
                    break;
                }
                case "c4a3":
                {
                    Entity entity = context.Map.Entities.CreateNewEntity( "game_achievement" );
                    entity.SetString( "targetname", "n_ending" );
                    entity.SetString( "m_label", "hl_nihilanth" );
                    entity.SetString( "m_name", "Free xen" );
                    entity.SetString( "m_description", "Kill the nihilanth" );
                    break;
                }
            }
        }
    }
}
