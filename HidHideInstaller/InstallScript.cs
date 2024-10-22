#nullable enable
using System;
using System.Buffers;
using System.Diagnostics;
using System.IO;
using System.Runtime.CompilerServices;
using System.Threading.Tasks;

using CliWrap;

using Nefarius.Utilities.DeviceManagement.PnP;
using Nefarius.Utilities.WixSharp.Util;

using WixSharp;
using WixSharp.CommonTasks;
using WixSharp.Forms;

using File = WixSharp.File;

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
                new Dir(driversFeature, "drivers")
                {
                    Dirs = WixExt.GetSubDirectories(driversFeature, DriversRoot).ToArray()
                },
                // manifest files
                new Dir(driversFeature, ManifestsDir,
                    // driver
                    new File(driversFeature, @"..\HidHide\HidHide.man"),
                    // CLI
                    new File(driversFeature, @"..\HidHideCLI\HidHideCLI.man"),
                    // cfg UI
                    new File(driversFeature, @"..\HidHideClient\HidHideClient.man")
                ),
                // updater
                new File(driversFeature, "nefarius_HidHide_Updater.exe"),
                // x64 cfg UI
                new File(driversFeature, Path.Combine(ArtifactsDir, "x64", "HidHideClient.exe"),
                    new FileShortcut("HidHide Configuration Client") { WorkingDirectory = "[INSTALLDIR]" })
                {
                    Condition = new Condition("VersionNT64 AND NOT IS_ARM64")
                },
                // x64 CLI
                new File(driversFeature, Path.Combine(ArtifactsDir, "x64", "HidHideCLI.exe"))
                {
                    Condition = new Condition("VersionNT64 AND NOT IS_ARM64")
                },
                // TODO: add ARM64 binaries
                // start menu shortcuts
                new Dir(@"%ProgramMenu%\Nefarius Software Solutions\HidHide",
                    new ExeFileShortcut("Uninstall HidHide", "[System64Folder]msiexec.exe", "/x [ProductCode]"),
                    new ExeFileShortcut("HidHide Configuration Client", @"[INSTALLDIR]HidHideClient.exe", "")
                    {
                        WorkingDirectory = "[INSTALLDIR]"
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
            ) { Win64 = true },
            // ARM64 detection action
            new ManagedAction(
                CustomActions.DetectArm64,
                Return.check,
                When.Before,
                Step.LaunchConditions,
                Condition.Always
            ),
            // passing around of custom properties
            new ManagedAction(
                CustomActions.SetCustomActionData,
                Return.check,
                When.Before,
                Step.InstallInitialize,
                Condition.Always
            ),
            // older to new version transition checks
            new ManagedAction(
                CustomActions.CheckIfUpgrading,
                Return.check,
                When.Before,
                Step.RemoveFiles,
                Condition.Installed
            ),
            // install drivers
            new ElevatedManagedAction(
                CustomActions.InstallDrivers,
                Return.check,
                When.After,
                Step.InstallFiles,
                Condition.NOT_Installed
            )
            {
                // elevated actions and events need these properties
                UsesProperties = $"{CustomProperties.HhDriverVersion},{CustomProperties.DoNotTouchDriver}"
            },
            // custom reboot prompt message
            new Error("9000",
                "Driver installation succeeded but a reboot is required to be fully operational. " +
                "After the setup is finished, please reboot the system before using the software.")
        )
        {
            GUID = new Guid("8822CC70-E2A5-4CB7-8F14-E27101150A1D"),
            CAConfigFile = "CustomActions.config",
            OutFileName = $"Nefarius_HidHide_Drivers_x64_v{version}",
            //custom set of standard UI dialogs
            ManagedUI = new ManagedUI(),
            Version = version,
            Platform = Platform.x64,
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

        // passes along packaged driver version through session
        project.AddProperty(new Property(CustomProperties.HhDriverVersion, driverVersion.ToString()));
        // passes boolean flag whether to not change the driver
        project.AddProperty(new Property(CustomProperties.DoNotTouchDriver, false.ToString()));

        // elevated actions and events need these properties
        project.DefaultDeferredProperties += $",{CustomProperties.HhDriverVersion},{CustomProperties.DoNotTouchDriver}";

        project.AfterInstall += ProjectOnAfterInstall;

        #region Fixes for setups < v1.6.x

        // suppresses reboot dialogs from removing older versions
        project.AddProperty(new Property("REBOOT", "ReallySuppress"));

        #endregion

        #region Dialogs

        project.ManagedUI.InstallDialogs.Add(Dialogs.Welcome)
            .Add(Dialogs.Licence)
            .Add(Dialogs.Features)
            .Add(Dialogs.Progress)
            .Add(Dialogs.Exit);

        project.ManagedUI.ModifyDialogs.Add(Dialogs.MaintenanceType)
            .Add(Dialogs.Features)
            .Add(Dialogs.Progress)
            .Add(Dialogs.Exit);

        #endregion

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
        project.DefaultRefAssemblies.Add(typeof(WixExt).Assembly.Location);

        #endregion

        #region Control Panel

        project.ControlPanelInfo.ProductIcon = @"..\HidHideClient\src\Application.ico";
        project.ControlPanelInfo.Manufacturer = "Nefarius Software Solutions e.U.";
        project.ControlPanelInfo.HelpLink = "https://docs.nefarius.at/Community-Support/";
        project.ControlPanelInfo.UrlUpdateInfo = "https://github.com/nefarius/HidHide/releases";
        project.ControlPanelInfo.UrlInfoAbout = "https://github.com/nefarius/HidHide";
        project.ControlPanelInfo.NoModify = true;

        #endregion

        project.BuildMsi();
    }

    private static void ProjectOnAfterInstall(SetupEventArgs e)
    {
        try
        {
            if (e.IsUninstalling)
            {
                CustomActions.UninstallDrivers(e.Session);
            }
        }
        catch (Exception ex)
        {
            e.Session.Log($"{nameof(ProjectOnAfterInstall)} FTL: {ex}");
        }
    }
}