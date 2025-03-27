using Utilities.Entities;
using Utilities.Tools.UpgradeTool;

namespace MapUpgrader.Upgrades.Common
{
    /// <summary>
    /// Converts <c>monster_zombie_barney</c> entities to a monster_zombie customized.
    /// </summary>
    internal sealed class ConvertZombieBarney : MapUpgrade
    {
        protected override void ApplyCore( MapUpgradeContext context )
        {
            foreach( var entity in context.Map.Entities.OfClass( "monster_zombie_barney" ).ToList())
            {
                entity.ClassName = "monster_zombie";
                entity.SetModel( "models/zombie_barney.mdl" );
//                entity.SetString( "configlist", "config/zombie_barney" );
            }
        }
    }
}
