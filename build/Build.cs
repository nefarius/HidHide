using System;
using System.IO;
using System.IO.Compression;
using System.Net.Http;
using System.Threading.Tasks;

using Nuke.Common;
using Nuke.Common.CI.AppVeyor;
using Nuke.Common.IO;
using Nuke.Common.Tooling;
using Nuke.Common.Tools.MSBuild;

using static Nuke.Common.IO.FileSystemTasks;
using static Nuke.Common.Tools.MSBuild.MSBuildTasks;

class Build : NukeBuild
{
    [Parameter("Configuration to build - Default is 'Debug' (local) or 'Release' (server)")]
    readonly string Configuration = IsLocalBuild ? "Debug" : "Release";

    [Parameter("Platform to build: x64 | ARM64. Default is current CI platform or x64 locally.")]
    readonly string Platform = IsLocalBuild ? "x64" : (AppVeyor.Instance.Platform ?? "x64");

    /// <summary>
    /// Explicit repo-root solution path so CI does not depend on NUKE solution injection / .nuke parameters.
    /// </summary>
    static AbsolutePath SolutionFile => RootDirectory / "HidHide.sln";

    AbsolutePath ArtifactsDirectory => RootDirectory / "artifacts";
    AbsolutePath StagingRoot => ArtifactsDirectory / "staging";
    AbsolutePath OutputRoot => RootDirectory / "bin" / Configuration / Platform;

    AbsolutePath StageDir => StagingRoot / Platform;

    const string NefconVersion = "1.17.40";
    const string NefconZipName = $"nefcon_v{NefconVersion}.zip";
    static readonly string NefconZipUrl =
        $"https://github.com/nefarius/nefcon/releases/download/v{NefconVersion}/{NefconZipName}";

    Target Clean => _ => _
        .Before(Restore)
        .Executes(() =>
        {
            EnsureCleanDirectory(ArtifactsDirectory);
        });

    Target Restore => _ => _
        .Executes(() =>
        {
            MSBuild(s => s
                .SetTargetPath(SolutionFile)
                .SetTargets("Restore")
                .SetVerbosity(MSBuildVerbosity.Minimal));
        });

    Target Compile => _ => _
        .DependsOn(Restore)
        .Executes(() =>
        {
            var platform = ParsePlatform(Platform);

            MSBuild(s => s
                .SetTargetPath(SolutionFile)
                .SetTargets("Rebuild")
                .SetConfiguration(Configuration)
                .SetTargetPlatform(platform)
                .SetMaxCpuCount(Environment.ProcessorCount)
                .SetNodeReuse(IsLocalBuild)
                .SetVerbosity(MSBuildVerbosity.Minimal));
        });

    Target StageInstallerPayload => _ => _
        .DependsOn(Compile)
        .Executes(async () =>
        {
            EnsureCleanDirectory(StageDir);

            // Driver artifacts
            CopyFileToDirectory(OutputRoot / "HidHide" / "HidHide.sys", StageDir, FileExistsPolicy.Fail);
            CopyFileToDirectory(OutputRoot / "HidHide" / "HidHide.inf", StageDir, FileExistsPolicy.Fail);
            CopyFileToDirectory(OutputRoot / "HidHide" / "HidHide.cat", StageDir, FileExistsPolicy.Fail);

            // Kernel ETW
            CopyFileToDirectory(OutputRoot / "HidHide.man", StageDir, FileExistsPolicy.Fail);
            CopyFileToDirectory(OutputRoot / "HidHide.wprp", StageDir, FileExistsPolicy.Fail);

            // User-mode binaries + ETW
            CopyFileToDirectory(OutputRoot / "HidHideClient.exe", StageDir, FileExistsPolicy.Fail);
            CopyFileToDirectory(OutputRoot / "HidHideClient.man", StageDir, FileExistsPolicy.Fail);
            CopyFileToDirectory(OutputRoot / "HidHideClient.wprp", StageDir, FileExistsPolicy.Fail);

            CopyFileToDirectory(OutputRoot / "HidHideCLI.exe", StageDir, FileExistsPolicy.Fail);
            CopyFileToDirectory(OutputRoot / "HidHideCLI.man", StageDir, FileExistsPolicy.Fail);
            CopyFileToDirectory(OutputRoot / "HidHideCLI.wprp", StageDir, FileExistsPolicy.Fail);

            // Helper tool: nefcon windowless build
            await EnsureNefconwAsync(StageDir);
        });

    Target BuildMsi => _ => _
        .DependsOn(StageInstallerPayload)
        .Executes(() =>
        {
            // Run the WixSharp installer builder; on CI this runs on Windows.
            AbsolutePath installerProject = RootDirectory / "Installer" / "HidHide.Installer.csproj";
            AbsolutePath outDir = ArtifactsDirectory / "msi" / Platform;
            EnsureExistingDirectory(StageDir);
            EnsureExistingDirectory(outDir);

            var args =
                $"run --project \"{installerProject}\" -c {Configuration} -- " +
                $"--staging \"{StageDir}\" --out \"{outDir}\" --platform {Platform}";

            ProcessTasks.StartProcess("dotnet", args, RootDirectory)
                .AssertZeroExitCode();

            // Normalize output name so release scripts can rely on stable filenames.
            var builtMsi = Directory.GetFiles(outDir, "*.msi");
            if (builtMsi.Length != 1)
                throw new Exception($"Expected exactly one MSI in {outDir}, found {builtMsi.Length}.");

            var targetName = $"HidHide_{Platform}.msi";
            var targetPath = Path.Combine(outDir, targetName);
            if (File.Exists(targetPath))
                File.Delete(targetPath);
            File.Move(builtMsi[0], targetPath);
        });

    Target BuildCab => _ => _
        .DependsOn(Compile)
        .Executes(() =>
        {
            // Keep legacy DDF-driven attestation cab generation.
            string ddf = Platform.Equals("ARM64", StringComparison.OrdinalIgnoreCase)
                ? "HidHide_ARM64.ddf"
                : "HidHide_x64.ddf";

            ProcessTasks.StartProcess("makecab.exe", $"/f {ddf}", RootDirectory)
                .AssertZeroExitCode();
        });

    Target Ci => _ => _
        .DependsOn(BuildMsi)
        .DependsOn(BuildCab);

    public static int Main() => Execute<Build>(x => x.Ci);

    static MSBuildTargetPlatform ParsePlatform(string platform) =>
        platform.ToUpperInvariant() switch
        {
            "ARM64" => (MSBuildTargetPlatform)"ARM64",
            "X64" => MSBuildTargetPlatform.x64,
            _ => MSBuildTargetPlatform.x64
        };

    static async Task EnsureNefconwAsync(AbsolutePath stageDir)
    {
        var nefconw = stageDir / "nefconw.exe";
        if (File.Exists(nefconw))
            return;

        var temp = (AbsolutePath)Path.Combine(Path.GetTempPath(), "hidhide-nefcon");
        EnsureExistingDirectory(temp);

        var zipPath = temp / NefconZipName;
        if (!File.Exists(zipPath))
        {
            using var http = new HttpClient();
            using var resp = await http.GetAsync(NefconZipUrl);
            resp.EnsureSuccessStatusCode();
            await using var fs = File.OpenWrite(zipPath);
            await resp.Content.CopyToAsync(fs);
        }

        var extractDir = temp / $"nefcon-{NefconVersion}";
        if (Directory.Exists(extractDir))
            DeleteDirectory(extractDir);
        Directory.CreateDirectory(extractDir);
        ZipFile.ExtractToDirectory(zipPath, extractDir);

        // Zip contains nefconc.exe and nefconw.exe at root.
        var src = Path.Combine(extractDir, "nefconw.exe");
        if (!File.Exists(src))
            throw new FileNotFoundException($"nefconw.exe not found after extracting {zipPath}");

        CopyFileToDirectory(src, stageDir, FileExistsPolicy.Overwrite);
    }
}

