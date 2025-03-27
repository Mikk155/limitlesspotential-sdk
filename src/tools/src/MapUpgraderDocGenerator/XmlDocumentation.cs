using System.Xml.Serialization;

namespace MapUpgraderDocGenerator
{
    [XmlRoot("doc", IsNullable = false)]
    public sealed class XmlDocumentation
    {
        [XmlArray("members")]
        [XmlArrayItem("member")]
        public List<XmlMemberDocumentation> Members { get; set; } = new();
    }
}
