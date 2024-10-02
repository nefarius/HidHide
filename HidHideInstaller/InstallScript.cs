#nullable enable
using System;
using System.Buffers;
using System.Net;
using System.Reflection;
using System.Runtime.CompilerServices;
using System.Threading.Tasks;

using CliWrap;

using System.Windows.Forms;

using Nefarius.Utilities.DeviceManagement.PnP;

using Newtonsoft.Json;

using WixSharp;
using WixSharp.Forms;

using File = WixSharp.File;

namespace Nefarius.HidHide.Setup;

internal class InstallScript
{
    public const string ProductName = "Nefarius HidHide";

    private static void Main()
    {
        Version version = Version.Parse(BuildVariables.SetupVersion);

        ManagedProject project = new(ProductName,
            new Dir(@"%ProgramFiles%\Nefarius Software Solutions\HidHide",
                new File("InstallScript.cs")))
        {
            GUID = new Guid("8822CC70-E2A5-4CB7-8F14-E27101150A1D"),
            CAConfigFile = "CustomActions.config",
            OutFileName = $"Nefarius_HidHide_Drivers_x64_v{version}",
            //custom set of standard UI dialogs
            ManagedUI = new ManagedUI(),
            Version = version,
            Platform = Platform.x64,
            WildCardDedup = Project.UniqueFileNameDedup,
            // TODO: finish me
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

        project.Load += Msi_Load;
        project.BeforeInstall += Msi_BeforeInstall;
        project.AfterInstall += Msi_AfterInstall;

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
        // embed types for web calls
        project.DefaultRefAssemblies.Add(typeof(WebClient).Assembly.Location);
        project.DefaultRefAssemblies.Add(typeof(JsonSerializer).Assembly.Location);
        project.DefaultRefAssemblies.Add(typeof(Binder).Assembly.Location);

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

    private static void Msi_Load(SetupEventArgs e)
    {
        if (!e.IsUISupressed && !e.IsUninstalling)
        {
            MessageBox.Show(e.ToString(), "Load");
        }
    }

    private static void Msi_BeforeInstall(SetupEventArgs e)
    {
        if (!e.IsUISupressed && !e.IsUninstalling)
        {
            MessageBox.Show(e.ToString(), "BeforeInstall");
        }
    }

    private static void Msi_AfterInstall(SetupEventArgs e)
    {
        if (!e.IsUISupressed && !e.IsUninstalling)
        {
            MessageBox.Show(e.ToString(), "AfterExecute");
        }
    }
}