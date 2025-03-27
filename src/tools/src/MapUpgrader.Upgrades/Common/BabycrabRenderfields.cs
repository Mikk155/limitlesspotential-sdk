using Utilities.Entities;
using Utilities.Tools.UpgradeTool;

namespace MapUpgrader.Upgrades.Common
{
    /// <summary>
    /// Remove render/mode/amt settings from babycrabs because the values are now hardcoded in OnCreate instead of Spawn so the mapper can now control the rendering.
    /// </summary>
    internal sealed class BabycrabRenderfields : MapUpgrade
    {
        protected override void ApplyCore( MapUpgradeContext context )
        {
            foreach( var entity in context.Map.Entities.OfClass( "monster_babycrab" ) )
            {
                entity.Remove( "rendermode" );
                entity.Remove( "renderamt" );
            }
        }
    }
}
