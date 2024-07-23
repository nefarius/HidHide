using System;
using System.Windows.Forms;
using WixSharp;
using WixSharp.Forms;

namespace Nefarius.HidHide.Setup
{
	internal class InstallScript
	{
		static void Main()
		{
			var project = new ManagedProject("MyProduct",
							 new Dir(@"%ProgramFiles%\My Company\My Product",
								 new File("InstallScript.cs")));

			project.Platform = Platform.x64;

			project.GUID = new Guid("FE535A74-4501-40CA-9E9E-8DCA6B122095");

			project.ManagedUI = ManagedUI.Empty;    //no standard UI dialogs
			project.ManagedUI = ManagedUI.Default;  //all standard UI dialogs

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

			//project.SourceBaseDir = "<input dir path>";
			//project.OutDir = "<output dir path>";

			project.BuildMsi();
		}

		static void Msi_Load(SetupEventArgs e)
		{
			if (!e.IsUISupressed && !e.IsUninstalling)
				MessageBox.Show(e.ToString(), "Load");
		}

		static void Msi_BeforeInstall(SetupEventArgs e)
		{
			if (!e.IsUISupressed && !e.IsUninstalling)
				MessageBox.Show(e.ToString(), "BeforeInstall");
		}

		static void Msi_AfterInstall(SetupEventArgs e)
		{
			if (!e.IsUISupressed && !e.IsUninstalling)
				MessageBox.Show(e.ToString(), "AfterExecute");
		}
	}
}