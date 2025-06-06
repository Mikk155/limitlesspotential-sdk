using Utilities.Entities;
using Utilities.Tools.UpgradeTool;

namespace MapUpgrader.Upgrades.HalfLife
{
    internal sealed class AddAchievementsHL : MapUpgrade
    {
        protected override void ApplyCore(MapUpgradeContext context)
        {
            switch( context.Map.BaseName )
            {
                case "c1a0d":
                {
                    Entity a1 = context.Map.Entities.CreateNewEntity( "game_achievement" );
                    a1.SetString( "targetname", "suit_cd_audio" );
                    a1.SetString( "m_label", "hl_suit" );
                    a1.SetString( "m_name", "Suit up" );
                    a1.SetString( "m_description", "Get the H.E.V suit" );

                    Entity a2 = context.Map.Entities.CreateNewEntity( "game_achievement" );
                    a2.SetString( "targetname", "microwavepopmm1" );
                    a2.SetString( "m_label", "hl_microwave" );
                    a2.SetString( "m_name", "What is that smell?" );
                    a2.SetString( "m_description", "Burn out magnusom's food in the microwave" );
                    break;
                }
                case "c1a3":
                case "c1a3d":
                {
                    List<string> TargetNames = new()
                    {
                        "c1a3_turret00",
                        "c1a3_turret01",
                        "c1a3_turret02"
                    };

                    if( context.Map.BaseName == "c1a3d" )
                    {
                        TargetNames.Clear();
                        TargetNames.Add( "c1a3_turret03" );

                        Entity trigger = context.Map.Entities.CreateNewEntity( "trigger_once" );
                        trigger.SetString( "target", "sentries_achievement" );
                        trigger.SetString( "model", "*75" );
                    }

                    foreach( string targetname in TargetNames )
                    {
                        Entity relay = context.Map.Entities.CreateNewEntity( "trigger_relay" );
                        relay.SetString( "targetname", targetname );
                        relay.SetInteger( "m_UseType", 4 );
                        relay.SetInteger( "spawnflags", 1 );
                        relay.SetString( "target", "sentries_achievement" );
                    }

                    Entity a1 = context.Map.Entities.CreateNewEntity( "game_achievement" );
                    a1.SetString( "targetname", "sentries_achievement" );
                    a1.SetString( "globalname", "sentries_achievement_g" );
                    a1.SetString( "m_label", "hl_sentrylaser" );
                    a1.SetString( "m_name", "You heard something?" );
                    a1.SetString( "m_description", "Complete We've got hostiles without alerting a single sentry" );
                    break;
                }
                case "c4a3":
                {
                    Entity a1 = context.Map.Entities.CreateNewEntity( "game_achievement" );
                    a1.SetString( "targetname", "n_ending" );
                    a1.SetString( "m_label", "hl_nihilanth" );
                    a1.SetString( "m_name", "Free xen" );
                    a1.SetString( "m_description", "Kill the nihilanth" );
                    break;
                }
            }
        }
    }
}
