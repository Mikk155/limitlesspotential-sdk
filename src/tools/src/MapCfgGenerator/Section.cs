using Newtonsoft.Json;

namespace MapCfgGenerator
{
    internal sealed record Section(Action<JsonTextWriter> WriterCallback, string Condition = "");
}
