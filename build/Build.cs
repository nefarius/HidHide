using System.Diagnostics;
using System.IO.Compression;
using System.Net.Http.Headers;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Security.Cryptography.X509Certificates;
using System.Text.Json;

using Fallout.Common;
using Fallout.Common.IO;
using Fallout.Common.Tooling;
using Fallout.Common.Tools.MSBuild;

using Serilog;

using static Fallout.Common.Tools.MSBuild.MSBuildTasks;

class Build : FalloutBuild
{
    const string NefconVersion = "1.17.40";
    const string GoogleTestVersion = "1.8.1.7";
    const string RepositoryOwner = "nefarius";
    const string RepositoryName = "HidHide";
    const string EphemeralTestPfxPassword = "HidHideCiEphemeral9#";

    static readonly string NefconZipName = $"nefcon_v{NefconVersion}.zip";
    static readonly string NefconZipUrl = $"https://github.com/nefarius/nefcon/releases/download/v{NefconVersion}/{NefconZipName}";

    [Parameter("Configuration to build - Default is 'Debug' (local) or 'Release' (server)")]
    readonly string Configuration = IsLocalBuild ? "Debug" : "Release";

    [Parameter("Platform to build: x64 | ARM64")]
    readonly string Platform = "x64";

    [Parameter("Optional PFX used to test-sign the staged driver. CI generates an ephemeral certificate when omitted.")]
    readonly string? HidHideTestSignPfx;

    [Parameter("Password for HidHideTestSignPfx")]
    [Secret]
    readonly string? HidHideTestSignPfxPassword;

    [Parameter("Git ref resolved by ResolveVersion, for example refs/tags/v1.8.0")]
    readonly string ReleaseRef = string.Empty;

    [Parameter("GitHub Actions run number used as the fourth binary-version component")]
    readonly int RunNumber;

    [Parameter("GITHUB_OUTPUT file that receives ResolveVersion results")]
    readonly string? GitHubOutput;

    [Parameter("Release tag whose successful GitHub Actions artifacts are downloaded, for example v1.8.0")]
    readonly string? ReleaseTag;

    [Parameter("Directory that receives downloaded release artifacts")]
    readonly AbsolutePath ReleaseOutput = RootDirectory / "artifacts" / "release";

    [Parameter("Download release artifacts without signing them")]
    readonly bool NoSigning;

    [Parameter("Certificate subject used for local production signing")]
    readonly string CertificateSubject = "Nefarius Software Solutions";

    [Parameter("GitHub token used to download Actions artifacts. Defaults to GH_TOKEN or GITHUB_TOKEN.")]
    [Secret]
    readonly string? GitHubToken;

    [NuGetPackage("Nefarius.Tools.WDKWhere", "wdkwhere.dll", Framework = "net8.0")]
    readonly Tool WdkWhere;

    static AbsolutePath SolutionFile => RootDirectory / "HidHide.sln";
    AbsolutePath ArtifactsDirectory => RootDirectory / "artifacts";
    AbsolutePath StagingRoot => ArtifactsDirectory / "staging";
    AbsolutePath OutputRoot => RootDirectory / "bin" / Configuration / Platform;
    AbsolutePath StageDir => StagingRoot / Platform;

    Target Clean => _ => _
        .Unlisted()
        .Before(Restore)
        .Executes(() =>
        {
            var platform = NormalizePlatform(Platform);
            if (IsLocalBuild)
                ArtifactsDirectory.CreateOrCleanDirectory();
            else
                StagingRoot.CreateOrCleanDirectory();

            var cab = RootDirectory / "disk1" / $"HidHide_{platform}.cab";
            if (File.Exists(cab.ToString()))
                File.Delete(cab.ToString());
        });

    Target Restore => _ => _
        .Description("Restore the solution and the pinned Google Test package")
        .DependsOn(Clean)
        .Executes(() =>
        {
            EnsureGoogleTestNuGetPackage();
            MSBuild(s => s
                .SetTargetPath(SolutionFile)
                .SetTargets("Restore")
                .SetVerbosity(MSBuildVerbosity.Minimal));
        });

    Target Compile => _ => _
        .Description("Rebuild HidHide.sln for one platform")
        .DependsOn(Restore)
        .Executes(() =>
        {
            MSBuild(s => ApplyCiProperties(s
                .SetTargetPath(SolutionFile)
                .SetTargets("Rebuild")
                .SetConfiguration(Configuration)
                .SetProperty("Platform", NormalizePlatform(Platform))
                .SetMaxCpuCount(Environment.ProcessorCount)
                .SetNodeReuse(IsLocalBuild)
                .SetVerbosity(MSBuildVerbosity.Minimal)));
        });

    Target Test => _ => _
        .Description("Run the x64 HidHide unit tests")
        .DependsOn(Compile)
        .OnlyWhenStatic(() => RuntimeInformation.IsOSPlatform(OSPlatform.Windows)
            && Platform.Equals("x64", StringComparison.OrdinalIgnoreCase))
        .Executes(() =>
        {
            var testExecutable = OutputRoot / "HidHide.Tests.exe";
            if (!File.Exists(testExecutable.ToString()))
                throw new FileNotFoundException($"Expected unit test runner at '{testExecutable}'. Build HidHide.Tests for {Configuration}|{Platform}.", testExecutable.ToString());

            ProcessTasks.StartProcess(testExecutable.ToString(), workingDirectory: OutputRoot.ToString(), logInvocation: false)
                .AssertZeroExitCode();
        });

    Target StageInstallerPayload => _ => _
        .Unlisted()
        .DependsOn(Compile)
        .Executes(async () =>
        {
            StageDir.CreateOrCleanDirectory();
            CopyRequired(OutputRoot / "HidHide" / "HidHide.sys", StageDir);
            CopyRequired(OutputRoot / "HidHide" / "HidHide.inf", StageDir);
            CopyRequired(OutputRoot / "HidHide" / "HidHide.cat", StageDir);
            CopyRequired(OutputRoot / "HidHide.man", StageDir);
            CopyRequired(OutputRoot / "HidHide.wprp", StageDir);
            CopyRequired(OutputRoot / "HidHideClient.exe", StageDir);
            CopyRequired(OutputRoot / "HidHideClient.man", StageDir);
            CopyRequired(OutputRoot / "HidHideClient.wprp", StageDir);
            CopyRequired(OutputRoot / "HidHideCLI.exe", StageDir);
            CopyRequired(OutputRoot / "HidHideCLI.man", StageDir);
            CopyRequired(OutputRoot / "HidHideCLI.wprp", StageDir);
            StageVisualCppRuntime(StageDir, NormalizePlatform(Platform));
            await EnsureNefconwAsync(StageDir, NormalizePlatform(Platform));
        });

    Target TestSignStagingDriver => _ => _
        .Unlisted()
        .DependsOn(StageInstallerPayload)
        .Executes(() =>
        {
            if (IsLocalBuild)
                return;

            var sys = StageDir / "HidHide.sys";
            var cat = StageDir / "HidHide.cat";
            if (!File.Exists(sys.ToString()) || !File.Exists(cat.ToString()))
                throw new FileNotFoundException("Expected HidHide.sys and HidHide.cat in staging.");

            var userPfx = string.IsNullOrWhiteSpace(HidHideTestSignPfx) ? null : Path.GetFullPath(HidHideTestSignPfx);
            var userPfxExists = userPfx != null && File.Exists(userPfx);
            if (userPfxExists && string.IsNullOrWhiteSpace(HidHideTestSignPfxPassword))
                throw new InvalidOperationException("When HidHideTestSignPfx points at an existing file, HidHideTestSignPfxPassword must also be set.");

            string pfxPath;
            string pfxPassword;
            var timestamp = false;
            string? ephemeralPfx = null;

            if (userPfxExists)
            {
                pfxPath = userPfx!;
                pfxPassword = HidHideTestSignPfxPassword!;
                timestamp = true;
            }
            else
            {
                var signDirectory = ArtifactsDirectory / ".ci-testsign";
                signDirectory.CreateDirectory();
                ephemeralPfx = Path.Combine(signDirectory.ToString(), $"hidhide-{NormalizePlatform(Platform)}-{Guid.NewGuid():N}.pfx");
                CreateEphemeralTestPfx(ephemeralPfx, EphemeralTestPfxPassword);
                pfxPath = ephemeralPfx;
                pfxPassword = EphemeralTestPfxPassword;
            }

            try
            {
                var platform = NormalizePlatform(Platform);
                SignWithPfx(pfxPath, pfxPassword, sys, pageHash: true, timestamp);
                var driverDirectory = StageDir.ToString().TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar);
                var inf2Cat = FindInf2Cat(platform);
                var operatingSystem = platform.Equals("ARM64", StringComparison.OrdinalIgnoreCase) ? "10_VB_ARM64" : "10_VB_X64";
                ProcessTasks.StartProcess(
                        inf2Cat,
                        $"/driver:\"{driverDirectory}\" /os:{operatingSystem} /uselocaltime /verbose",
                        logInvocation: false,
                        logger: (_, text) => Log.Information("{Text}", text))
                    .AssertZeroExitCode();
                SignWithPfx(pfxPath, pfxPassword, cat, pageHash: false, timestamp);
            }
            finally
            {
                DeleteIfExists(ephemeralPfx);
            }
        });

    Target PackageMsi => _ => _
        .Description("Stage, test-sign on CI, and build the MSI")
        .DependsOn(TestSignStagingDriver)
        .Executes(() =>
        {
            var platform = NormalizePlatform(Platform);
            var installerProject = RootDirectory / "Installer" / "HidHide.Installer.csproj";
            var outputDirectory = ArtifactsDirectory / "msi" / platform;
            StageDir.CreateDirectory();
            outputDirectory.CreateDirectory();

            var arguments =
                $"run --project \"{installerProject}\" -c {Configuration} -- " +
                $"--staging \"{StageDir}\" --out \"{outputDirectory}\" --platform {platform}";
            ProcessTasks.StartProcess("dotnet", arguments, RootDirectory.ToString())
                .AssertZeroExitCode();

            var built = Directory.GetFiles(outputDirectory.ToString(), "*.msi");
            if (built.Length != 1)
                throw new InvalidOperationException($"Expected exactly one MSI in {outputDirectory}, found {built.Length}.");

            var target = Path.Combine(outputDirectory.ToString(), $"HidHide_{platform}.msi");
            if (File.Exists(target))
                File.Delete(target);
            File.Move(built[0], target);
            Log.Information("MSI: {Msi}", target);
        });

    Target PackageCab => _ => _
        .Description("Build the attestation CAB for one platform")
        .DependsOn(Compile)
        .Executes(() =>
        {
            if (!Configuration.Equals("Release", StringComparison.OrdinalIgnoreCase))
                throw new InvalidOperationException("CAB definitions package bin\\Release. Run PackageCab with --configuration Release.");

            var platform = NormalizePlatform(Platform);
            var definition = platform.Equals("ARM64", StringComparison.OrdinalIgnoreCase) ? "HidHide_ARM64.ddf" : "HidHide_x64.ddf";
            ProcessTasks.StartProcess("makecab.exe", $"/f {definition}", RootDirectory.ToString())
                .AssertZeroExitCode();
        });

    Target Ci => _ => _
        .Description("Restore, build, test where possible, and package MSI and CAB")
        .DependsOn(Test)
        .DependsOn(PackageMsi)
        .DependsOn(PackageCab);

    Target TestVersion => _ => _
        .Description("Verify release-tag and CI version resolution")
        .Executes(ReleaseVersion.Verify);

    Target ResolveVersion => _ => _
        .Description("Resolve the binary version and write GitHub Actions outputs")
        .Executes(() =>
        {
            var version = ReleaseVersion.Resolve(ReleaseRef, RunNumber);
            foreach (var line in ReleaseVersion.FormatGitHubOutput(version))
                Log.Information("{Line}", line);

            if (!string.IsNullOrWhiteSpace(GitHubOutput))
                ReleaseVersion.WriteGitHubOutput(version, GitHubOutput);
        });

    Target Release => _ => _
        .Description("Download MSI and CAB artifacts from the tagged GitHub Actions run and optionally sign them")
        .Executes(() => DownloadAndSignReleaseAsync().GetAwaiter().GetResult());

    public static int Main() => Execute<Build>(x => x.Ci);

    async Task DownloadAndSignReleaseAsync()
    {
        var parsedTag = ReleaseVersion.Resolve(ReleaseTag, 1);
        if (string.IsNullOrWhiteSpace(ReleaseTag) || !parsedTag.IsRelease)
            throw new InvalidOperationException("Release requires --release-tag vMAJOR.MINOR.PATCH, for example v1.8.0.");
        var tag = parsedTag.Tag;

        var token = FirstNonEmpty(GitHubToken, Environment.GetEnvironmentVariable("GH_TOKEN"), Environment.GetEnvironmentVariable("GITHUB_TOKEN"));
        if (string.IsNullOrWhiteSpace(token))
            throw new InvalidOperationException("Set GH_TOKEN or GITHUB_TOKEN, or pass --github-token, to download GitHub Actions artifacts.");

        PrepareReleaseOutput(ReleaseOutput);
        using var http = CreateGitHubClient(token);
        var repository = $"{RepositoryOwner}/{RepositoryName}";
        var run = await FindSuccessfulRunAsync(http, repository, tag);
        Log.Information("Using GitHub Actions run {RunId} ({Url})", run.Id, run.Url);

        foreach (var platform in new[] { "x64", "ARM64" })
        {
            var artifact = run.Artifacts.SingleOrDefault(candidate => candidate.Name.Equals($"hidhide-{platform}", StringComparison.OrdinalIgnoreCase));
            if (artifact == null)
                throw new InvalidOperationException($"Run {run.Id} does not contain artifact 'hidhide-{platform}'. Available: {string.Join(", ", run.Artifacts.Select(candidate => candidate.Name))}.");
            if (artifact.Expired)
                throw new InvalidOperationException($"Artifact '{artifact.Name}' from run {run.Id} has expired.");

            var zip = ReleaseOutput / $"{artifact.Name}.zip";
            await DownloadFileAsync(http, artifact.DownloadUrl, zip.ToString());
            ZipFile.ExtractToDirectory(zip.ToString(), ReleaseOutput.ToString(), overwriteFiles: true);
            File.Delete(zip.ToString());
        }

        var files = new[]
        {
            FindRequiredArtifact(ReleaseOutput, "HidHide_x64.msi"),
            FindRequiredArtifact(ReleaseOutput, "HidHide_ARM64.msi"),
            FindRequiredArtifact(ReleaseOutput, "HidHide_x64.cab"),
            FindRequiredArtifact(ReleaseOutput, "HidHide_ARM64.cab"),
        };

        if (!NoSigning)
        {
            var arguments = new List<string> { "sign", "/v", "/as", "/n", CertificateSubject, "/tr", "http://timestamp.digicert.com", "/fd", "sha256", "/td", "sha256" };
            arguments.AddRange(files.Select(file => file.ToString()));
            RunWdkWhereSigntool(arguments.ToArray());
        }

        Log.Information("HidHide Release {Tag} {Date}", tag, DateTime.Now.ToString("dd.MM.yyyy", System.Globalization.CultureInfo.InvariantCulture));
    }

    static AbsolutePath FindRequiredArtifact(AbsolutePath root, string fileName)
    {
        var matches = Directory.EnumerateFiles(root.ToString(), fileName, SearchOption.AllDirectories).ToArray();
        if (matches.Length != 1)
            throw new FileNotFoundException($"Expected exactly one '{fileName}' under '{root}', found {matches.Length}.");

        return (AbsolutePath)matches[0];
    }

    static void PrepareReleaseOutput(AbsolutePath output)
    {
        var full = Path.GetFullPath(output.ToString());
        var root = Path.GetFullPath(RootDirectory.ToString());
        var comparison = OperatingSystem.IsWindows() ? StringComparison.OrdinalIgnoreCase : StringComparison.Ordinal;
        if (string.Equals(full, root, comparison) || root.StartsWith(full + Path.DirectorySeparatorChar, comparison))
            throw new InvalidOperationException($"Refusing to clean '{full}' because it contains the repository.");

        var relative = Path.GetRelativePath(root, full);
        if (relative.StartsWith("..", StringComparison.Ordinal) || Path.IsPathRooted(relative))
            throw new InvalidOperationException($"Release output must be inside the repository. Got: '{full}'.");

        output.CreateOrCleanDirectory();
    }

    static HttpClient CreateGitHubClient(string token)
    {
        var http = new HttpClient();
        http.Timeout = TimeSpan.FromMinutes(10);
        http.DefaultRequestHeaders.UserAgent.Add(new ProductInfoHeaderValue("HidHide-Build", "1"));
        http.DefaultRequestHeaders.Accept.Add(new MediaTypeWithQualityHeaderValue("application/vnd.github+json"));
        http.DefaultRequestHeaders.Authorization = new AuthenticationHeaderValue("Bearer", token);
        http.DefaultRequestHeaders.Add("X-GitHub-Api-Version", "2022-11-28");
        return http;
    }

    static async Task<ReleaseRun> FindSuccessfulRunAsync(HttpClient http, string repository, string tag)
    {
        var runsUrl = $"https://api.github.com/repos/{repository}/actions/workflows/build.yml/runs?branch={Uri.EscapeDataString(tag)}&event=push&status=success&per_page=20";
        using var runs = await GetJsonAsync(http, runsUrl);
        if (!runs.RootElement.TryGetProperty("workflow_runs", out var items))
            throw new InvalidOperationException($"GitHub did not return workflow runs for {tag}.");

        var expectedCommit = await GetTagCommitAsync(http, repository, tag);
        foreach (var item in items.EnumerateArray())
        {
            var headBranch = item.GetProperty("head_branch").GetString();
            var conclusion = item.GetProperty("conclusion").GetString();
            var path = item.GetProperty("path").GetString();
            var headSha = item.GetProperty("head_sha").GetString();
            if (!string.Equals(headBranch, tag, StringComparison.Ordinal)
                || !string.Equals(conclusion, "success", StringComparison.Ordinal)
                || !string.Equals(path, ".github/workflows/build.yml", StringComparison.Ordinal)
                || !string.Equals(headSha, expectedCommit, StringComparison.OrdinalIgnoreCase))
                continue;

            var id = item.GetProperty("id").GetInt64();
            var artifacts = await ListArtifactsAsync(http, repository, id);
            var url = item.GetProperty("html_url").GetString() ?? string.Empty;
            return new ReleaseRun(id, url, artifacts);
        }

        throw new InvalidOperationException($"No successful Build run for tag {tag} at commit {expectedCommit} was found.");
    }

    static async Task<string> GetTagCommitAsync(HttpClient http, string repository, string tag)
    {
        using var commit = await GetJsonAsync(http, $"https://api.github.com/repos/{repository}/commits/{Uri.EscapeDataString(tag)}");
        return commit.RootElement.GetProperty("sha").GetString()
            ?? throw new InvalidOperationException($"GitHub did not return a commit SHA for {tag}.");
    }

    static async Task<IReadOnlyList<ReleaseArtifact>> ListArtifactsAsync(HttpClient http, string repository, long runId)
    {
        using var document = await GetJsonAsync(http, $"https://api.github.com/repos/{repository}/actions/runs/{runId}/artifacts?per_page=100");
        return document.RootElement.GetProperty("artifacts").EnumerateArray()
            .Select(artifact => new ReleaseArtifact(
                artifact.GetProperty("name").GetString() ?? string.Empty,
                artifact.GetProperty("archive_download_url").GetString() ?? string.Empty,
                artifact.GetProperty("expired").GetBoolean()))
            .ToArray();
    }

    static async Task<JsonDocument> GetJsonAsync(HttpClient http, string url)
    {
        using var response = await http.GetAsync(url);
        var body = await response.Content.ReadAsStringAsync();
        if (!response.IsSuccessStatusCode)
            throw new InvalidOperationException($"GitHub request failed ({(int)response.StatusCode}) for {url}:{Environment.NewLine}{body}");

        return JsonDocument.Parse(body);
    }

    static async Task DownloadFileAsync(HttpClient http, string url, string destination)
    {
        using var response = await http.GetAsync(url, HttpCompletionOption.ResponseHeadersRead);
        if (!response.IsSuccessStatusCode)
        {
            var body = await response.Content.ReadAsStringAsync();
            throw new InvalidOperationException($"GitHub artifact download failed ({(int)response.StatusCode}): {body}");
        }

        await using var source = await response.Content.ReadAsStreamAsync();
        await using var target = File.Create(destination);
        await source.CopyToAsync(target);
    }

    static MSBuildSettings ApplyCiProperties(MSBuildSettings settings)
    {
        var version = Environment.GetEnvironmentVariable("BUILD_VERSION");
        if (!string.IsNullOrWhiteSpace(version))
            settings = settings.SetProperty("BUILD_VERSION", version);

        return settings;
    }

    void EnsureGoogleTestNuGetPackage()
    {
        var packageDirectory = RootDirectory / "packages" / $"Microsoft.googletest.v140.windesktop.msvcstl.static.rt-static.{GoogleTestVersion}";
        var marker = packageDirectory / "build" / "native" / "Microsoft.googletest.v140.windesktop.msvcstl.static.rt-static.targets";
        if (File.Exists(marker.ToString()))
            return;

        Log.Information("Downloading Google Test NuGet package {Version}", GoogleTestVersion);
        var tempZip = RootDirectory / ".fallout" / "temp" / $"googletest.{GoogleTestVersion}.nupkg";
        tempZip.Parent.CreateDirectory();
        var url = $"https://api.nuget.org/v3-flatcontainer/microsoft.googletest.v140.windesktop.msvcstl.static.rt-static/{GoogleTestVersion}/microsoft.googletest.v140.windesktop.msvcstl.static.rt-static.{GoogleTestVersion}.nupkg";

        const int maxAttempts = 3;
        byte[]? payload = null;
        Exception? lastException = null;
        for (var attempt = 0; attempt < maxAttempts; attempt++)
        {
            try
            {
                using var http = new HttpClient();
                payload = http.GetByteArrayAsync(url).GetAwaiter().GetResult();
                break;
            }
            catch (Exception exception) when (attempt < maxAttempts - 1 && exception is HttpRequestException or TaskCanceledException or OperationCanceledException)
            {
                lastException = exception;
                Thread.Sleep(200 * (1 << attempt) + Random.Shared.Next(0, 101));
            }
        }

        if (payload == null)
            throw new InvalidOperationException($"Failed to download Google Test NuGet package after {maxAttempts} attempts from {url}", lastException);

        File.WriteAllBytes(tempZip.ToString(), payload);
        var verify = ProcessTasks.StartProcess("dotnet", $"nuget verify \"{tempZip}\" --verbosity quiet", RootDirectory.ToString(), logInvocation: false);
        verify.WaitForExit();
        if (verify.ExitCode != 0)
            throw new InvalidOperationException($"Google Test NuGet package failed integrity verification (exit {verify.ExitCode}).{Environment.NewLine}{string.Join(Environment.NewLine, verify.Output.Select(line => line.Text))}");

        if (Directory.Exists(packageDirectory.ToString()))
            Directory.Delete(packageDirectory.ToString(), recursive: true);
        packageDirectory.CreateDirectory();
        ZipFile.ExtractToDirectory(tempZip.ToString(), packageDirectory.ToString());
        if (!File.Exists(marker.ToString()))
            throw new InvalidOperationException($"Extracted Google Test package but marker file is missing: {marker}");
    }

    static async Task EnsureNefconwAsync(AbsolutePath stageDirectory, string platform)
    {
        var destination = stageDirectory / "nefconw.exe";
        if (File.Exists(destination.ToString()))
            return;

        var temp = (AbsolutePath)Path.Combine(Path.GetTempPath(), "hidhide-nefcon");
        temp.CreateDirectory();
        var zip = temp / NefconZipName;
        if (!File.Exists(zip.ToString()))
        {
            using var http = new HttpClient();
            using var response = await http.GetAsync(NefconZipUrl);
            response.EnsureSuccessStatusCode();
            await using var file = File.OpenWrite(zip.ToString());
            await response.Content.CopyToAsync(file);
        }

        var extractDirectory = temp / $"nefcon-{NefconVersion}";
        if (Directory.Exists(extractDirectory.ToString()))
            extractDirectory.DeleteDirectory();
        extractDirectory.CreateDirectory();
        ZipFile.ExtractToDirectory(zip.ToString(), extractDirectory.ToString());

        var source = Path.Combine(extractDirectory.ToString(), platform.Equals("ARM64", StringComparison.OrdinalIgnoreCase) ? "ARM64" : "x64", "nefconw.exe");
        if (!File.Exists(source))
            throw new FileNotFoundException($"nefconw.exe was not found for {platform} after extracting {zip}", source);

        ((AbsolutePath)source).CopyToDirectory(stageDirectory, ExistsPolicy.FileOverwrite);
    }

    static void StageVisualCppRuntime(AbsolutePath stageDir, string platform)
    {
        var arch = platform.Equals("ARM64", StringComparison.OrdinalIgnoreCase) ? "arm64" : "x64";
        var redist = FindLatestVc145Redist();
        var crt = Path.Combine(redist, "spectre", arch, "Microsoft.VC145.CRT");
        var mfc = Path.Combine(redist, "spectre", arch, "Microsoft.VC145.MFC");
        if (!Directory.Exists(crt) || !Directory.Exists(mfc))
            throw new DirectoryNotFoundException($"Spectre-mitigated Visual C++ 2026 redistributable for {arch} was not found under '{redist}'.");

        CopyRuntimeDll(Path.Combine(mfc, "mfc140u.dll"), stageDir);
        foreach (var name in new[]
        {
            "vcruntime140.dll",
            "msvcp140.dll",
            "msvcp140_1.dll",
            "msvcp140_2.dll",
            "vcruntime140_1.dll",
            "vcruntime140_threads.dll",
            "concrt140.dll",
            "msvcp140_atomic_wait.dll",
            "msvcp140_codecvt_ids.dll",
        })
        {
            var source = Path.Combine(crt, name);
            var required = name is "vcruntime140.dll" or "msvcp140.dll" or "msvcp140_1.dll" or "msvcp140_2.dll";
            if (!File.Exists(source))
            {
                if (required)
                    throw new FileNotFoundException($"Missing Visual C++ runtime '{source}'.", source);
                continue;
            }

            CopyRuntimeDll(source, stageDir);
        }
    }

    static void CopyRuntimeDll(string source, AbsolutePath destinationDirectory)
    {
        if (!File.Exists(source))
            throw new FileNotFoundException($"Missing Visual C++ runtime '{source}'.", source);

        File.Copy(source, Path.Combine(destinationDirectory.ToString(), Path.GetFileName(source)), overwrite: false);
    }

    static string FindLatestVc145Redist()
    {
        var vswhere = Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.ProgramFilesX86),
            "Microsoft Visual Studio",
            "Installer",
            "vswhere.exe");
        if (!File.Exists(vswhere))
            throw new FileNotFoundException("vswhere.exe was not found.", vswhere);

        var start = new ProcessStartInfo(vswhere, "-latest -products * -requires Microsoft.Component.MSBuild -property installationPath")
        {
            RedirectStandardOutput = true,
            RedirectStandardError = true,
            UseShellExecute = false,
            CreateNoWindow = true,
        };
        using var process = System.Diagnostics.Process.Start(start) ?? throw new InvalidOperationException("Failed to start vswhere.exe.");
        var output = process.StandardOutput.ReadToEnd();
        process.WaitForExit();
        var installPath = output.Split(['\r', '\n'], StringSplitOptions.RemoveEmptyEntries).FirstOrDefault()?.Trim();
        if (process.ExitCode != 0 || string.IsNullOrWhiteSpace(installPath))
            throw new InvalidOperationException("Visual Studio installation was not found.");

        var redistRoot = Path.Combine(installPath, "VC", "Redist", "MSVC");
        var selected = Directory.Exists(redistRoot)
            ? Directory.GetDirectories(redistRoot)
                .Select(Path.GetFileName)
                .Where(name => name != null && name.StartsWith("14.5", StringComparison.Ordinal))
                .OrderByDescending(name => name, StringComparer.Ordinal)
                .FirstOrDefault()
            : null;
        if (selected == null)
            throw new DirectoryNotFoundException($"No v145 redistributable was found under '{redistRoot}'.");

        return Path.Combine(redistRoot, selected);
    }

    static void CopyRequired(AbsolutePath source, AbsolutePath destinationDirectory)
    {
        if (!File.Exists(source.ToString()))
            throw new FileNotFoundException($"Missing installer payload '{source}'.", source.ToString());

        source.CopyToDirectory(destinationDirectory, ExistsPolicy.FileFail);
    }

    static string NormalizePlatform(string platform)
    {
        if (string.IsNullOrWhiteSpace(platform))
            throw new ArgumentNullException(nameof(platform));

        if (platform.Equals("x64", StringComparison.OrdinalIgnoreCase))
            return "x64";
        if (platform.Equals("ARM64", StringComparison.OrdinalIgnoreCase))
            return "ARM64";

        throw new ArgumentException($"Unsupported platform: {platform}. Expected x64 or ARM64.", nameof(platform));
    }

    string FindInf2Cat(string platform)
    {
        var onPath = FindOnPath(OperatingSystem.IsWindows() ? "Inf2Cat.exe" : "inf2cat");
        if (onPath != null)
            return onPath;

        var packages = RootDirectory / "packages";
        if (Directory.Exists(packages.ToString()))
        {
            var matches = Directory.EnumerateFiles(packages.ToString(), "Inf2Cat.exe", SearchOption.AllDirectories).ToArray();
            var platformMatch = matches.FirstOrDefault(candidate => candidate.Contains($"{Path.DirectorySeparatorChar}{platform}{Path.DirectorySeparatorChar}", StringComparison.OrdinalIgnoreCase));
            if (platformMatch != null)
                return platformMatch;
            if (matches.Length != 0)
                return matches[0];
        }

        throw new FileNotFoundException("Inf2Cat.exe was not found on PATH or in the restored Windows kit packages.");
    }

    static string? FindOnPath(string executable)
    {
        var paths = Environment.GetEnvironmentVariable("PATH")?.Split(Path.PathSeparator) ?? [];
        return paths
            .Where(path => !string.IsNullOrWhiteSpace(path))
            .Select(path => Path.Combine(path.Trim(), executable))
            .FirstOrDefault(File.Exists);
    }

    void SignWithPfx(string pfxPath, string password, AbsolutePath file, bool pageHash, bool timestamp)
    {
        var arguments = new List<string> { "sign", "/v", "/fd", "SHA256", "/f", pfxPath, "/p", password };
        if (timestamp)
            arguments.AddRange(["/tr", "http://timestamp.digicert.com", "/td", "SHA256"]);
        if (pageHash)
            arguments.Add("/ph");
        arguments.Add(file.ToString());
        RunWdkWhereSigntool(arguments.ToArray());
    }

    void RunWdkWhereSigntool(params string[] signtoolArguments)
    {
        if (signtoolArguments.Length == 0)
            throw new ArgumentException("At least one signtool argument is required.", nameof(signtoolArguments));

        var arguments = "run signtool " + string.Join(" ", signtoolArguments.Select(EscapeForCmdToken));
        WdkWhere(arguments, logInvocation: false);
    }

    static string EscapeForCmdToken(string value)
    {
        if (value.Length == 0)
            return "\"\"";

        return value.Any(character => char.IsWhiteSpace(character) || character == '"')
            ? "\"" + value.Replace("\"", "\\\"") + "\""
            : value;
    }

    static void CreateEphemeralTestPfx(string path, string password)
    {
        using var rsa = RSA.Create(2048);
        var request = new CertificateRequest("CN=HidHide CI test signing", rsa, HashAlgorithmName.SHA256, RSASignaturePadding.Pkcs1);
        request.CertificateExtensions.Add(new X509KeyUsageExtension(X509KeyUsageFlags.DigitalSignature, critical: true));
        request.CertificateExtensions.Add(new X509EnhancedKeyUsageExtension(
            new OidCollection { new Oid("1.3.6.1.5.5.7.3.3") },
            critical: true));
        request.CertificateExtensions.Add(new X509BasicConstraintsExtension(certificateAuthority: false, hasPathLengthConstraint: false, pathLengthConstraint: 0, critical: true));

        var now = DateTimeOffset.UtcNow;
        using var certificate = request.CreateSelfSigned(now.AddDays(-1), now.AddDays(42));
        File.WriteAllBytes(path, certificate.Export(X509ContentType.Pfx, password));
    }

    static void DeleteIfExists(string? path)
    {
        if (path != null && File.Exists(path))
        {
            try { File.Delete(path); }
            catch { /* The ephemeral signing material is removed on a best-effort basis. */ }
        }
    }

    static string? FirstNonEmpty(params string?[] values) => values.FirstOrDefault(value => !string.IsNullOrWhiteSpace(value));

    sealed record ReleaseRun(long Id, string Url, IReadOnlyList<ReleaseArtifact> Artifacts);
    sealed record ReleaseArtifact(string Name, string DownloadUrl, bool Expired);
}
