#nullable enable
using System;
using System.Buffers;
using System.Diagnostics;
using System.IO;
using System.Runtime.CompilerServices;
using System.Threading.Tasks;

using CliWrap;

using Nefarius.Utilities.DeviceManagement.PnP;


using WixSharp;
using WixSharp.Forms;

using File = WixSharp.File;

using Nefarius.Utilities.WixSharp.Util;

using WixToolset.Dtf.WindowsInstaller;

namespace Nefarius.HidHide.Setup;

internal class InstallScript
{
    public const string ProductName = "Nefarius HidHide";
    public const string DriversRoot = @"..\drivers";
    public const string ManifestsDir = "manifests";
    public const string ArtifactsDir = @"..\artifacts\bin\Release";

    private static void Main()
    {
        Version version = Version.Parse(BuildVariables.SetupVersion);

        string driverPath = Path.Combine(DriversRoot, @"x64\HidHide\HidHide.sys");
        Version driverVersion = Version.Parse(FileVersionInfo.GetVersionInfo(driverPath).FileVersion);

        Console.WriteLine($"Setup version: {version}");
        Console.WriteLine($"Driver version: {driverVersion}");

        Feature driversFeature = new("HidHide Core Components", true, false)
        {
            Description = "Installs the Nefarius HidHide software. " +
                          "This is a mandatory core component and can't be de-selected.",
            Display = FeatureDisplay.expand
        };

        ManagedProject project = new(ProductName,
            new Dir(@"%ProgramFiles%\Nefarius Software Solutions\HidHide",
                // driver binaries
                new Dir(driversFeature, "drivers") { Dirs = WixExt.GetSubDirectories(driversFeature, DriversRoot).ToArray() },
                // manifest files
                new Dir(driversFeature, ManifestsDir,
                    new File(driversFeature, @"..\HidHide\HidHide.man")
                ),
                // updater
                new File(driversFeature, "nefarius_HidHide_Updater.exe"),
                // x64 utilities
                new Dir(new Id("x64apps"), "x64",
                    // cfg UI
                    new File(driversFeature, Path.Combine(ArtifactsDir, "x64", "HidHideClient.exe"),
                        new FileShortcut("HidHide Configuration Client") { WorkingDirectory = "[x64apps]" }),
                    // CLI
                    new File(driversFeature, Path.Combine(ArtifactsDir, "x64", "HidHideCLI.exe"))
                ),
                // start menu shortcuts
                new Dir(@"%ProgramMenu%\Nefarius Software Solutions\HidHide",
                    new ExeFileShortcut("Uninstall HidHide", "[System64Folder]msiexec.exe", "/x [ProductCode]"),
                    new ExeFileShortcut("HidHide Configuration Client", @"[x64apps]\HidHideClient.exe", "")
                    {
                        WorkingDirectory = "[x64apps]"
                    }
                )
            ),
            // registry values
            new RegKey(driversFeature, RegistryHive.LocalMachine,
                $@"Software\Nefarius Software Solutions e.U.\{ProductName}",
                new RegValue("Path", "[INSTALLDIR]") { Win64 = true },
                new RegValue("Version", version.ToString()) { Win64 = true },
                new RegValue("DriverVersion", driverVersion.ToString()) { Win64 = true }
            ) { Win64 = true },
            // registry values for backwards compatibility (updater)
            new RegKey(driversFeature, RegistryHive.LocalMachine,
                @"Software\Nefarius Software Solutions e.U.\HidHide",
                new RegValue("Path", "[INSTALLDIR]") { Win64 = true },
                new RegValue("Version", version.ToString()) { Win64 = true }
            ) { Win64 = true }
        )
        {
            GUID = new Guid("8822CC70-E2A5-4CB7-8F14-E27101150A1D"),
            CAConfigFile = "CustomActions.config",
            OutFileName = $"Nefarius_HidHide_Drivers_x64_v{version}",
            //custom set of standard UI dialogs
            ManagedUI = new ManagedUI(),
            Version = version,
            Platform = Platform.x64,
            WildCardDedup = Project.UniqueFileNameDedup,
            DefaultFeature = driversFeature,
            LicenceFile = "EULA.rtf",
            BackgroundImage = "left-banner.png",
            BannerImage = "top-banner.png",
            MajorUpgrade = new MajorUpgrade
            {
                Schedule = UpgradeSchedule.afterInstallInitialize,
                DowngradeErrorMessage =
                    "A later version of [ProductName] is already installed. Setup will now exit."
            }
        };

        project.ManagedUI.InstallDialogs.Add(Dialogs.Welcome)
            .Add(Dialogs.Licence)
            .Add(Dialogs.Features)
            .Add(Dialogs.Progress)
            .Add(Dialogs.Exit);

        project.ManagedUI.ModifyDialogs.Add(Dialogs.MaintenanceType)
            .Add(Dialogs.Features)
            .Add(Dialogs.Progress)
            .Add(Dialogs.Exit);

        project.AfterInstall += ProjectOnAfterInstall;

        #region Embed types of dependencies

        // embed types of Nefarius.Utilities.DeviceManagement
        project.DefaultRefAssemblies.Add(typeof(Devcon).Assembly.Location);
        // embed types of CliWrap
        project.DefaultRefAssemblies.Add(typeof(Cli).Assembly.Location);
        project.DefaultRefAssemblies.Add(typeof(ValueTask).Assembly.Location);
        project.DefaultRefAssemblies.Add(typeof(IAsyncDisposable).Assembly.Location);
        project.DefaultRefAssemblies.Add(typeof(Unsafe).Assembly.Location);
        project.DefaultRefAssemblies.Add(typeof(BuffersExtensions).Assembly.Location);
        project.DefaultRefAssemblies.Add(typeof(ArrayPool<>).Assembly.Location);

        #endregion

        #region Control Panel

        project.ControlPanelInfo.ProductIcon = @"..\HidHideClient\src\Application.ico";
        project.ControlPanelInfo.Manufacturer = "Nefarius Software Solutions e.U.";
        project.ControlPanelInfo.HelpLink = "https://docs.nefarius.at/Community-Support/";
        project.ControlPanelInfo.UrlUpdateInfo = "https://github.com/nefarius/HidHide/releases";
        project.ControlPanelInfo.UrlInfoAbout = "https://github.com/nefarius/HidHide";
        project.ControlPanelInfo.NoModify = true;

        #endregion

        project.ResolveWildCards();

        project.BuildMsi();
    }

    private static void ProjectOnAfterInstall(SetupEventArgs e)
    {
        if (e.IsUninstalling)
        {
            CustomActions.UninstallDrivers(e.Session);
        }
    }
}

public static class CustomActions
{
    public static bool UninstallDrivers(Session session)
    {
        return false;
    }
}