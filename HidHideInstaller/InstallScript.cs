#nullable enable
using System;
using System.Buffers;
using System.Collections.Generic;
using System.Diagnostics;
using System.Diagnostics.CodeAnalysis;
using System.IO;
using System.Linq;
using System.Net;
using System.Reflection;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Threading.Tasks;

using CliWrap;
using CliWrap.Buffered;

using System.Windows.Forms;

using Nefarius.Utilities.DeviceManagement.PnP;

using Newtonsoft.Json;

using WixSharp;
using WixSharp.Forms;

using File = WixSharp.File;

namespace Nefarius.HidHide.Setup;

internal class InstallScript
{
    private static void Main()
    {
        Version version = Version.Parse(BuildVariables.SetupVersion);

        ManagedProject project = new ManagedProject("MyProduct",
            new Dir(@"%ProgramFiles%\My Company\My Product",
                new File("InstallScript.cs")))
        {
            GUID = new Guid("57F3646E-E9AB-4AA5-BD4E-2A4E01DDAD94"),
            ManagedUI = ManagedUI.Empty, //no standard UI dialogs
            Version = version,
            Platform = Platform.x64,
            WildCardDedup = Project.UniqueFileNameDedup,
            MajorUpgradeStrategy = MajorUpgradeStrategy.Default
        };

        project.ManagedUI = ManagedUI.Default; //all standard UI dialogs

        //custom set of standard UI dialogs
        project.ManagedUI = new ManagedUI();

        project.ManagedUI.InstallDialogs.Add(Dialogs.Welcome)
            .Add(Dialogs.Licence)
            .Add(Dialogs.SetupType)
            .Add(Dialogs.Features)
            .Add(Dialogs.InstallDir)
            .Add(Dialogs.Progress)
            .Add(Dialogs.Exit);

        project.ManagedUI.ModifyDialogs.Add(Dialogs.MaintenanceType)
            .Add(Dialogs.Features)
            .Add(Dialogs.Progress)
            .Add(Dialogs.Exit);

        project.Load += Msi_Load;
        project.BeforeInstall += Msi_BeforeInstall;
        project.AfterInstall += Msi_AfterInstall;

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


        //project.SourceBaseDir = "<input dir path>";
        //project.OutDir = "<output dir path>";

        project.MajorUpgradeStrategy.PreventDowngradingVersions.OnlyDetect = false;

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