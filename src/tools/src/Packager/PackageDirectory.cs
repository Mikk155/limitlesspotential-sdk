namespace Packager
{
    internal sealed record PackageDirectory(string Path, IEnumerable<string> IncludePatterns, IEnumerable<string> ExcludePatterns);
}
