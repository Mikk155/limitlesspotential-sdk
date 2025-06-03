using Utilities.Entities;
using Utilities.Tools.UpgradeTool;

namespace MapUpgrader.Upgrades.Common
{
    /// <summary>
    /// Update env_sound to our new ambient_dsp
    /// </summary>
    internal sealed class ConvertSpeaker : MapUpgrade
    {
        protected override void ApplyCore( MapUpgradeContext context )
        {
            foreach( Entity entity in context.Map.Entities.Where( e => e.ClassName == "speaker" ) )
            {
                entity.ClassName = "ambient_speaker";

                int flags = entity.GetSpawnFlags();

                if( ( flags & 1 ) != 0 )
                {
                    flags &= ~1;
                    entity.SetBool( "m_bStartOff", 1 );
                    entity.SetSpawnFlags( flags );
                }

                if( entity.ContainsKey( "preset" ) )
                {
                    string? preset = entity.GetInteger( "preset" ) switch
                    {
                        1 => "C1A0_",
                        2 => "C1A1_",
                        3 => "C1A2_",
                        4 => "C1A3_",
                        5 => "C1A4_",
                        6 => "C2A1_",
                        7 => "C2A2_",
                        8 => "C2A3_",
                        9 => "C2A4_",
                        10 => "C2A5_",
                        11 => "C3A1_",
                        12 => "C3A2_",
                        _ => null
                    };

                    if( preset is not null )
                    {
                        entity.SetString( "message", preset );
                    }

                    entity.Remove( "preset" );
                }
            }
        }
    }
}
