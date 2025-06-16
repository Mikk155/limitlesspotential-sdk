using Utilities.Games;
using Utilities.Tools.UpgradeTool;

namespace MapUpgrader.Upgrades.Common
{
    internal sealed class SetMapConfig : MapUpgrade
    {
        private static void ApplyMapCFG( MapUpgradeContext context )
        {
            if( ValveGames.BlueShift.IsMap( context.Map.BaseName ) )
            {
                context.Map.Entities.Worldspawn.SetString( "mapcfg", "globals/blueshift/main" );
            }
            else if( ValveGames.HalfLife1.IsMap( context.Map.BaseName ) || ValveGames.HalfLifeUplink.IsMap( context.Map.BaseName ) )
            {
                string? config;

                switch( context.Map.BaseName )
                {
                    case "c1a0c":
                        config = "c1a0c";
                    break;

                    // -TODO Enumerate chapters and give arsenal on their config

                    default:
                        config = "main";
                    break;
                }

                context.Map.Entities.Worldspawn.SetString( "mapcfg", $"globals/halflife/{config}" );
            }
            else if( ValveGames.OpposingForce.IsMap( context.Map.BaseName ) )
            {
                switch( context.Map.BaseName )
                {
                    case "op4ctf_xendance":
                    case "op4ctf_power":
                    case "op4cp_park":
                    {
                        context.Map.Entities.Worldspawn.SetString( "mapcfg", "globals/opfor_ctf/game_player_equip" );
                        break;
                    }
                    default:
                    {
                        if( context.Map.BaseName.StartsWith( "op4ctf_" ) )
                        {
                            context.Map.Entities.Worldspawn.SetString( "mapcfg", "globals/opfor_ctf/main" );
                        }
                        else
                        {
                            context.Map.Entities.Worldspawn.SetString( "mapcfg", "globals/opfor/main" );
                        }
                        break;
                    }
                }
            }
        }

        private static void ApplyMapGameMode( MapUpgradeContext context )
        {
            switch( context.Map.BaseName )
            {
                case "boot_camp":
                case "bounce":
                case "undertow":
                case "crossfire":
                case "datacore":
                case "frenzy":
                case "gasworks":
                case "lambda_bunker":
                case "rapidcore":
                case "snark_pit":
                case "stalkyard":
                case "subtransit":
                case "op4_bootcamp":
                case "op4_datacore":
                case "op4_demise":
                case "op4_disposal":
                case "op4_gasworks":
                case "op4_kbase":
                case "op4_kndyone":
                case "op4_meanie":
                case "op4_outpost":
                case "op4_park":
                case "op4_repent":
                case "op4_rubble":
                case "op4_xendance":
                {
                    context.Map.Entities.Worldspawn.SetString( "gamemode", "deathmatch" );
                    break;
                }
                case "op4cp_park":
                case "op4ctf_biodomes":
                case "op4ctf_chasm":
                case "op4ctf_crash":
                case "op4ctf_dam":
                case "op4ctf_gunyard":
                case "op4ctf_hairball":
                case "op4ctf_mortar":
                case "op4ctf_power":
                case "op4ctf_repent":
                case "op4ctf_wonderland":
                case "op4ctf_xendance":
                {
                    context.Map.Entities.Worldspawn.SetString( "gamemode_lock", "1" );
                    context.Map.Entities.Worldspawn.SetString( "gamemode", "ctf" );
                    break;
                }
                default:
                {
                    context.Map.Entities.Worldspawn.SetString( "gamemode_lock", "1" );
                    context.Map.Entities.Worldspawn.SetString( "gamemode", "singleplayer" );
                    break;
                }
            }
        }

        protected override void ApplyCore(MapUpgradeContext context)
        {
            ApplyMapCFG(context);
            ApplyMapGameMode(context);
        }
    }
}
