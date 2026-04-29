using System.Diagnostics;
using System.Diagnostics.CodeAnalysis;
using System.Globalization;
using IOFile = System.IO.File;
using WixSharp;

namespace HidHide.Installer;

/// <summary>
/// Builds HidHide.msi via WixSharp targeting WiX 5 (same package family as WinDbgSymbolsCachingProxy.Installer).
/// Managed hooks install/remove the driver via <c>nefconw.exe</c> (<see href="https://github.com/nefarius/nefcon">nefcon</see>,
/// windowless), ETW via wevtutil. Class GUIDs match Watchdog/App.cpp.
/// </summary>
public static class Program
{
    const string Manufacturer = "Nefarius Software Solutions";

    /// <summary>SETUPAPI GUID_DEVCLASS_HIDCLASS</summary>
    const string ClassGuidHid = "{745a17a0-74d3-11d0-b6fe-00a0c90f57da}";

    /// <summary>GUID_DEVCLASS_XNACOMPOSITE — Watchdog/App.cpp</summary>
    const string ClassGuidXnaComposite = "{d61ca365-5af4-4486-998b-9db4734c6ca3}";

    /// <summary>GUID_DEVCLASS_XBOXCOMPOSITE — Watchdog/App.cpp</summary>
    const string ClassGuidXboxComposite = "{05f5cfe2-4733-4950-a6bb-07aad01a3a84}";

    /// <summary>ERROR_SUCCESS_REBOOT_REQUIRED — nefcon may return this; treat as success for setup.</summary>
    const int ExitSuccessRebootRequired = 3010;

    /// <summary>Must match dep:Provides Key suffix expectations / upgrade story.</summary>
    static readonly Guid UpgradeCode = new("8822CC70-E2A5-4CB7-8F14-E27101150A1D");

    internal const string EnvStaging = "HIDHIDE_INSTALLER_STAGING";
    internal const string EnvOut = "HIDHIDE_INSTALLER_OUT";

    static int Main(string[] args)
    {
        try
        {
            var options = Options.Parse(args);
            options.Validate();

            string licensePath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "LICENSE.rtf"));
            if (!IOFile.Exists(licensePath))
                throw new FileNotFoundException($"License file not found: {licensePath}");

            string installRel = @"%ProgramFiles64Folder%\Nefarius Software Solutions\HidHide";
            string sd = options.StagingDir;
            var dir = new Dir(
                installRel,
                new WixSharp.File(Path.Combine(sd, "HidHide.cat")),
                new WixSharp.File(Path.Combine(sd, "nefconw.exe")),
                new WixSharp.File(Path.Combine(sd, "HidHide.inf")),
                new WixSharp.File(Path.Combine(sd, "HidHide.sys")),
                new WixSharp.File(Path.Combine(sd, "HidHide.man")),
                new WixSharp.File(Path.Combine(sd, "HidHide.wprp")),
                new WixSharp.File(
                    Path.Combine(sd, "HidHideClient.exe"),
                    new FileShortcut("HidHide Configuration Client", @"%ProgramMenu%\Nefarius Software Solutions\HidHide")),
                new WixSharp.File(Path.Combine(sd, "HidHideClient.man")),
                new WixSharp.File(Path.Combine(sd, "HidHideClient.wprp")),
                new WixSharp.File(Path.Combine(sd, "HidHideCLI.exe")),
                new WixSharp.File(Path.Combine(sd, "HidHideCLI.man")),
                new WixSharp.File(Path.Combine(sd, "HidHideCLI.wprp")));

            var project = new ManagedProject(
                "HidHide",
                dir)
            {
                GUID = new Guid("B7E9D4A2-6F31-4E88-9C0D-1A2B3C4D5E6F"),
                UpgradeCode = UpgradeCode,
                Platform = options.Platform,
                Scope = InstallScope.perMachine,
                UI = WUI.WixUI_FeatureTree,
                LicenceFile = licensePath,
                Version = ReadProductVersion(Path.Combine(options.StagingDir, "HidHideClient.exe")),
                OutDir = options.OutputDir,
                OutFileName = "HidHide",
            };

            project.ControlPanelInfo.Manufacturer = Manufacturer;

            // Align with WiX 5.x + WixToolset.UI.wixext/5.0.x; WiX 6 defaults are not compatible with WixSharp + WixUI without tweaks.
            WixExtension.UI.PreferredVersion = "5.0.2";

            project.BeforeInstall += OnBeforeInstall;
            project.AfterInstall += OnAfterInstall;

            string msiPath = project.BuildMsi();
            Console.WriteLine(msiPath);
            return 0;
        }
        catch (Exception ex)
        {
            Console.Error.WriteLine(ex.Message);
            Console.Error.WriteLine(ex);
            return 1;
        }
    }

    public static void OnBeforeInstall(SetupEventArgs e)
    {
        if (!e.IsUninstalling)
            return;

        string root = InstallRoot(e);
        string nefcon = Path.Combine(root, "nefconw.exe");
        string wevt = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.Windows), "System32", "wevtutil.exe");

        TryRun(nefcon, $"--remove-class-filter --position upper --service-name HidHide --class-guid {ClassGuidXboxComposite}");
        TryRun(nefcon, $"--remove-class-filter --position upper --service-name HidHide --class-guid {ClassGuidXnaComposite}");
        TryRun(nefcon, $"--remove-class-filter --position upper --service-name HidHide --class-guid {ClassGuidHid}");
        TryRun(nefcon, @"remove ""root\HidHide""");

        TryRun(wevt, $@"um ""{Path.Combine(root, "HidHideCLI.man")}""");
        TryRun(wevt, $@"um ""{Path.Combine(root, "HidHideClient.man")}""");
        TryRun(wevt, $@"um ""{Path.Combine(root, "HidHide.man")}""");
    }

    public static void OnAfterInstall(SetupEventArgs e)
    {
        if (e.IsUninstalling)
            return;

        string root = InstallRoot(e);
        string nefcon = Path.Combine(root, "nefconw.exe");
        string wevt = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.Windows), "System32", "wevtutil.exe");
        string infPath = Path.Combine(root, "HidHide.inf");

        // Devcon-compatible install (see nefcon readme); --remove-duplicates requires nefcon v1.17.40+.
        Run(nefcon, $@"install ""{infPath}"" ""root\HidHide"" --no-duplicates --remove-duplicates");
        TryRun(nefcon, $"--add-class-filter --position upper --service-name HidHide --class-guid {ClassGuidHid}");
        TryRun(nefcon, $"--add-class-filter --position upper --service-name HidHide --class-guid {ClassGuidXnaComposite}");
        TryRun(nefcon, $"--add-class-filter --position upper --service-name HidHide --class-guid {ClassGuidXboxComposite}");

        Run(wevt, $@"im ""{Path.Combine(root, "HidHide.man")}""");
        Run(wevt, $@"im ""{Path.Combine(root, "HidHideClient.man")}""");
        Run(wevt, $@"im ""{Path.Combine(root, "HidHideCLI.man")}""");
    }

    static string InstallRoot(SetupEventArgs e)
    {
        string d = e.InstallDir?.TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar)
                   ?? throw new InvalidOperationException("INSTALLDIR missing.");
        return d;
    }

    static void Run(string path, string args)
    {
        using var p = Process.Start(new ProcessStartInfo(path, args)
        {
            UseShellExecute = false,
            CreateNoWindow = true,
            RedirectStandardOutput = true,
            RedirectStandardError = true,
        });
        if (p is null)
            throw new InvalidOperationException($"Failed to start: {path}");
        p.WaitForExit();
        if (p.ExitCode != 0 && p.ExitCode != ExitSuccessRebootRequired)
            throw new InvalidOperationException($"{path} {args} exited with code {p.ExitCode}. stderr: {p.StandardError.ReadToEnd()}");
    }

    /// <summary>Matches legacy MSI Return="ignore" optional class-filter actions.</summary>
    static void TryRun(string path, string args)
    {
        try
        {
            using var p = Process.Start(new ProcessStartInfo(path, args)
            {
                UseShellExecute = false,
                CreateNoWindow = true,
            });
            p?.WaitForExit();
        }
        catch
        {
            // ignored
        }
    }

    static Version ReadProductVersion(string clientExe)
    {
        if (!IOFile.Exists(clientExe))
            return new Version(1, 0, 0, 0);
        var info = FileVersionInfo.GetVersionInfo(clientExe);
        string? raw = info.ProductVersion ?? info.FileVersion;
        return TryParseVersion(raw, out Version? v) ? v : new Version(1, 0, 0, 0);
    }

    static bool TryParseVersion(string? raw, [NotNullWhen(true)] out Version? version)
    {
        version = null;
        if (string.IsNullOrWhiteSpace(raw))
            return false;
        string numeric = raw.Split('+', 2)[0].Split('-', 2)[0].Trim();
        string[] parts = numeric.Split('.');
        try
        {
            int major = parts.Length > 0 ? int.Parse(parts[0], CultureInfo.InvariantCulture) : 0;
            int minor = parts.Length > 1 ? int.Parse(parts[1], CultureInfo.InvariantCulture) : 0;
            int build = parts.Length > 2 ? int.Parse(parts[2], CultureInfo.InvariantCulture) : 0;
            int revision = parts.Length > 3 ? int.Parse(parts[3], CultureInfo.InvariantCulture) : 0;
            version = new Version(major, minor, build, revision);
            return true;
        }
        catch (ArgumentException)
        {
            return false;
        }
        catch (FormatException)
        {
            return false;
        }
    }

    sealed class Options
    {
        public required string StagingDir { get; init; }
        public required string OutputDir { get; init; }
        public required Platform Platform { get; init; }

        public static Options Parse(string[] args)
        {
            string cwd = Directory.GetCurrentDirectory();
            string staging = Path.GetFullPath(Path.Combine(cwd, "staging"));
            string output = Path.GetFullPath(Path.Combine(cwd, "msi-out"));
            string arch = "x64";

            ApplyEnvironmentOverrides(ref staging, ref output);

            for (var i = 0; i < args.Length; i++)
            {
                string a = args[i];
                if (a is "--staging" or "-s")
                    staging = RequirePath(args, ref i, "staging");
                else if (a is "--out" or "-o")
                    output = RequirePath(args, ref i, "out");
                else if (a is "--platform" or "-p")
                    arch = RequireArg(args, ref i, "platform");
                else if (a is "--help" or "-h")
                    PrintHelp();
                else
                    throw new ArgumentException($"Unknown argument: {a}");
            }

            return new Options
            {
                StagingDir = staging,
                OutputDir = output,
                Platform = arch.Equals("ARM64", StringComparison.OrdinalIgnoreCase) ? Platform.arm64 : Platform.x64,
            };
        }

        static void ApplyEnvironmentOverrides(ref string staging, ref string output)
        {
            string? v = Environment.GetEnvironmentVariable(EnvStaging);
            if (!string.IsNullOrWhiteSpace(v))
                staging = Path.GetFullPath(v);

            v = Environment.GetEnvironmentVariable(EnvOut);
            if (!string.IsNullOrWhiteSpace(v))
                output = Path.GetFullPath(v);
        }

        static string RequirePath(string[] args, ref int i, string name)
        {
            if (i + 1 >= args.Length)
                throw new ArgumentException($"Missing value for --{name}.");
            i++;
            return Path.GetFullPath(args[i]);
        }

        static string RequireArg(string[] args, ref int i, string name)
        {
            if (i + 1 >= args.Length)
                throw new ArgumentException($"Missing value for --{name}.");
            i++;
            return args[i];
        }

        static void PrintHelp()
        {
            Console.WriteLine(
                """
                HidHide — MSI builder (WixSharp / WiX 5)

                Options:
                  --staging, -s   Flat folder with all payload files (see INSTALL_LAYOUT.md)
                  --out, -o       MSI output directory
                  --platform, -p  x64 | ARM64 (default: x64)

                Environment (optional):
                  HIDHIDE_INSTALLER_STAGING, HIDHIDE_INSTALLER_OUT

                Staging must include nefconw.exe (windowless) from nefcon v1.17.40 or newer.

                Requires Windows, WiX 5.0.2 global tool + WixToolset.UI.wixext/5.0.2:
                  dotnet tool install --global wix --version 5.0.2
                  wix extension add -g WixToolset.UI.wixext/5.0.2
                """);
            Environment.Exit(0);
        }

        public void Validate()
        {
            if (!Directory.Exists(StagingDir))
                throw new DirectoryNotFoundException($"Staging directory not found: {StagingDir}");

            foreach (string name in RequiredPayloadFiles)
            {
                string p = Path.Combine(StagingDir, name);
                if (!IOFile.Exists(p))
                    throw new FileNotFoundException($"Staging payload incomplete; missing: {p}");
            }

            Directory.CreateDirectory(OutputDir);
        }

        static readonly string[] RequiredPayloadFiles =
        [
            "HidHide.cat",
            "nefconw.exe",
            "HidHide.inf",
            "HidHide.sys",
            "HidHide.man",
            "HidHide.wprp",
            "HidHideClient.exe",
            "HidHideClient.man",
            "HidHideClient.wprp",
            "HidHideCLI.exe",
            "HidHideCLI.man",
            "HidHideCLI.wprp",
        ];
    }
}
