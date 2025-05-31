using Utilities.Games;
using Utilities.Tools.UpgradeTool;

namespace MapUpgrader.Upgrades.Common
{
    internal sealed class Setmapcfg : MapUpgrade
    {
        protected override void ApplyCore(MapUpgradeContext context)
        {
            if( ValveGames.BlueShift.IsMap( context.Map.BaseName ) )
            {
                context.Map.Entities.Worldspawn.SetString( "mapcfg", "globals/blueshift/main" );
            }
            else if( ValveGames.HalfLife1.IsMap( context.Map.BaseName ) || ValveGames.HalfLifeUplink.IsMap( context.Map.BaseName ) )
            {
                context.Map.Entities.Worldspawn.SetString( "mapcfg", "globals/halflife/main" );
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
    }
}
