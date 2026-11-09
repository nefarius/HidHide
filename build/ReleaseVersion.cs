using System.Globalization;
using System.Text;
using System.Text.RegularExpressions;

/// <summary>
/// Resolves HidHide product versions for GitHub Actions.
/// A release tag <c>vMAJOR.MINOR.PATCH</c> becomes binary version <c>MAJOR.MINOR.PATCH.&lt;run number&gt;</c>.
/// Every other ref becomes <c>0.0.0.&lt;run number&gt;</c>.
/// </summary>
static class ReleaseVersion
{
    public const int MaxComponent = 65535;

    static readonly Regex ReleaseTagPattern = new(@"^v(?<major>0|[1-9]\d*)\.(?<minor>0|[1-9]\d*)\.(?<patch>0|[1-9]\d*)$", RegexOptions.CultureInvariant | RegexOptions.Compiled);

    public static ResolvedReleaseVersion Resolve(string? reference, int runNumber)
    {
        if (runNumber < 1 || runNumber > MaxComponent)
            throw new ArgumentOutOfRangeException(nameof(runNumber), runNumber, $"Run number must be in 1..{MaxComponent}.");

        var original = reference?.Trim() ?? string.Empty;
        var isTag = IsTagReference(original);
        var tag = isTag ? StripTagPrefix(original) : string.Empty;

        if (!isTag)
        {
            return new ResolvedReleaseVersion(false, string.Empty, string.Empty, $"0.0.0.{runNumber.ToString(CultureInfo.InvariantCulture)}", runNumber);
        }

        var match = ReleaseTagPattern.Match(tag);
        if (!match.Success)
            throw new InvalidOperationException($"Release tags must be vMAJOR.MINOR.PATCH (example: v1.8.0). Got: '{tag}'.");

        var major = ParseComponent(match.Groups["major"].Value);
        var minor = ParseComponent(match.Groups["minor"].Value);
        var patch = ParseComponent(match.Groups["patch"].Value);
        var setup = string.Create(CultureInfo.InvariantCulture, $"{major}.{minor}.{patch}");
        return new ResolvedReleaseVersion(true, tag, setup, string.Create(CultureInfo.InvariantCulture, $"{setup}.{runNumber}"), runNumber);
    }

    public static IReadOnlyList<string> FormatGitHubOutput(ResolvedReleaseVersion version) =>
    [
        $"is-release={version.IsRelease.ToString().ToLowerInvariant()}",
        $"tag={version.Tag}",
        $"setup-version={version.SetupVersion}",
        $"driver-version={version.DriverVersion}",
        $"revision={version.Revision.ToString(CultureInfo.InvariantCulture)}",
    ];

    public static void WriteGitHubOutput(ResolvedReleaseVersion version, string outputPath)
    {
        var directory = Path.GetDirectoryName(outputPath);
        if (!string.IsNullOrEmpty(directory))
            Directory.CreateDirectory(directory);

        var payload = string.Join('\n', FormatGitHubOutput(version)) + "\n";
        File.AppendAllText(outputPath, payload, new UTF8Encoding(encoderShouldEmitUTF8Identifier: false));
    }

    public static void Verify()
    {
        var release = Resolve("refs/tags/v1.8.0", 42);
        Assert(release.IsRelease && release.Tag == "v1.8.0" && release.SetupVersion == "1.8.0" && release.DriverVersion == "1.8.0.42" && release.Revision == 42, "annotated release tag");

        var bare = Resolve("v1.8.0", 7);
        Assert(bare.IsRelease && bare.DriverVersion == "1.8.0.7", "bare release tag");

        var branch = Resolve("refs/heads/master", 9);
        Assert(!branch.IsRelease && branch.Tag == string.Empty && branch.SetupVersion == string.Empty && branch.DriverVersion == "0.0.0.9", "branch build");

        var pullRequest = Resolve("refs/pull/12/merge", 3);
        Assert(!pullRequest.IsRelease && pullRequest.DriverVersion == "0.0.0.3", "pull request build");

        var ceiling = Resolve("refs/tags/v65535.65535.65535", MaxComponent);
        Assert(ceiling.DriverVersion == "65535.65535.65535.65535", "16-bit ceiling");

        AssertThrows(() => Resolve("refs/tags/v1.8", 1), "three-component tags are required");
        AssertThrows(() => Resolve("refs/tags/v1.8.0.1", 1), "four-part tags are rejected");
        AssertThrows(() => Resolve("refs/tags/v01.8.0", 1), "leading zeros are rejected");
        var branchNamedLikeTag = Resolve("refs/heads/v1.8.0", 1);
        Assert(!branchNamedLikeTag.IsRelease && branchNamedLikeTag.DriverVersion == "0.0.0.1", "branch names are not release tags");
        AssertThrows(() => Resolve("refs/tags/v65536.0.0", 1), "component above 65535");
        AssertThrows(() => Resolve("refs/heads/master", 0), "zero run number");
        AssertThrows(() => Resolve("refs/heads/master", MaxComponent + 1), "run number above 65535");

        var output = Path.Combine(Path.GetTempPath(), "hidhide-release-version-" + Guid.NewGuid().ToString("N") + ".txt");
        try
        {
            WriteGitHubOutput(release, output);
            var lines = File.ReadAllLines(output);
            Assert(lines.SequenceEqual(FormatGitHubOutput(release)), "GitHub output");
            Assert(File.ReadAllBytes(output).Take(3).SequenceEqual(new byte[] { 0xEF, 0xBB, 0xBF }) == false, "GitHub output has no BOM");
        }
        finally
        {
            if (File.Exists(output))
                File.Delete(output);
        }
    }

    static bool IsTagReference(string reference)
    {
        if (reference.StartsWith("refs/tags/", StringComparison.Ordinal))
            return true;

        return !reference.StartsWith("refs/", StringComparison.Ordinal) && reference.StartsWith('v');
    }

    static string StripTagPrefix(string reference)
    {
        const string prefix = "refs/tags/";
        return reference.StartsWith(prefix, StringComparison.Ordinal) ? reference[prefix.Length..] : reference;
    }

    static int ParseComponent(string value)
    {
        var component = int.Parse(value, CultureInfo.InvariantCulture);
        if (component > MaxComponent)
            throw new InvalidOperationException($"Version component {component} exceeds the 16-bit maximum ({MaxComponent}).");

        return component;
    }

    static void Assert(bool condition, string assertion)
    {
        if (!condition)
            throw new InvalidOperationException($"Version resolution failed: {assertion}.");
    }

    static void AssertThrows(Action action, string assertion)
    {
        try
        {
            action();
        }
        catch (ArgumentOutOfRangeException)
        {
            return;
        }
        catch (InvalidOperationException)
        {
            return;
        }

        throw new InvalidOperationException($"Version resolution failed to reject: {assertion}.");
    }
}

sealed record ResolvedReleaseVersion(bool IsRelease, string Tag, string SetupVersion, string DriverVersion, int Revision);
