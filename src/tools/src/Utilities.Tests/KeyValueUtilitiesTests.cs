using Utilities.Tools;
using Xunit;

namespace Utilities.Tests
{
    public class KeyValueUtilitiesTests
    {
        [Fact]
        public void EmptyMapString_ResultMatches()
        {
            Assert.Equal(@"{
""classname"" ""worldspawn""
}
",
                KeyValueUtilities.EmptyMapString);
        }
    }
}
