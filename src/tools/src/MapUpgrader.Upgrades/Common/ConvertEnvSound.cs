using Utilities.Entities;
using Utilities.Tools.UpgradeTool;

namespace MapUpgrader.Upgrades.Common
{
    /// <summary>
    /// Update env_sound to our new ambient_dsp
    /// </summary>
    internal sealed class ConvertEnvSound : MapUpgrade
    {
        protected override void ApplyCore( MapUpgradeContext context )
        {
            foreach( Entity entity in context.Map.Entities.Where( e => e.ClassName == "env_sound" ) )
            {
                entity.ClassName = "ambient_dsp_auto";

                if( entity.ContainsKey( "roomtype" ) )
                {
                    entity.SetInteger( "m_Roomtype", entity.GetInteger( "roomtype" ) );
                    entity.Remove( "roomtype" );
                }

                if( entity.ContainsKey( "roomtype" ) )
                {
                    entity.SetInteger( "m_flRadius", entity.GetInteger( "radius" ) );
                    entity.Remove( "radius" );
                }
            }
        }
    }
}
