#nullable enable
using System;
using System.IO;

using Nefarius.Utilities.DeviceManagement.Drivers;
using Nefarius.Utilities.DeviceManagement.Exceptions;
using Nefarius.Utilities.DeviceManagement.Extensions;
using Nefarius.Utilities.DeviceManagement.PnP;
using Nefarius.Utilities.WixSharp.Util;

using WixToolset.Dtf.WindowsInstaller;

namespace Nefarius.HidHide.Setup;

public static class CustomProperties
{
    public static readonly string DoNotTouchDriver = "DO_NOT_TOUCH_DRIVER";
    public static readonly string HhDriverVersion = "HH_DRIVER_VERSION";
}

public static class CustomActions
{
    /// <summary>
    ///     Helper action to set IS_ARM64 if we are on an ARM64 machine.
    /// </summary>
    [CustomAction]
    public static ActionResult DetectArm64(Session session)
    {
        if (ArchitectureInfo.IsArm64)
        {
            session["IS_ARM64"] = "1";
        }

        return ActionResult.Success;
    }

    /// <summary>
    ///     Helper action to pass along the HH_DRIVER_VERSION property to <see cref="CheckIfUpgrading"/>.
    /// </summary>
    [CustomAction]
    public static ActionResult SetCustomActionData(Session session)
    {
        // Set data into CustomActionData for the deferred custom action
        session["CustomActionData"] =
            $"{CustomProperties.HhDriverVersion}=" + session[CustomProperties.HhDriverVersion];
        session.Log("Setting CustomActionData for deferred action: " + session["CustomActionData"]);
        return ActionResult.Success;
    }

    /// <summary>
    ///     Performs special checks if we are upgrading from an older installation.
    /// </summary>
    [CustomAction]
    public static ActionResult CheckIfUpgrading(Session session)
    {
        // The UPGRADINGPRODUCTCODE property is set when the uninstall is part of an upgrade.
        string upgradingProductCode = session["UPGRADINGPRODUCTCODE"];
        if (!string.IsNullOrEmpty(upgradingProductCode))
        {
            session.Log("Uninstallation is part of an upgrade.");

            try
            {
                Version newDriverVersion = Version.Parse(session.CustomActionData[CustomProperties.HhDriverVersion]);

                session.Log($"Included driver version: {newDriverVersion}");

                // found at least one active device driver instance
                if (Devcon.FindByInterfaceGuid(HidHideDriver.HidHideInterfaceGuid, out PnPDevice device))
                {
                    DriverMeta? driver = device.GetCurrentDriver();

                    if (driver is not null)
                    {
                        session.Log($"Found existing driver with version: {driver.DriverVersion}");

                        if (driver.DriverVersion >= newDriverVersion)
                        {
                            session.Log("Driver is recent, flagging as do-not-touch");
                            // do not remove and re-create if the shipped version is not newer than what is active
                            session[CustomProperties.DoNotTouchDriver] = true.ToString();
                        }
                    }
                }
                else
                {
                    session.Log("No active driver found, continuing as fresh installation");
                }
            }
            catch (Exception ex)
            {
                session.Log($"FTL: failed to check existing driver version: {ex}");
            }
        }
        else
        {
            session.Log("Uninstallation is not part of an upgrade.");
            // Nothing to do here since this Action is not elevated
        }

        return ActionResult.Success;
    }

    /// <summary>
    ///     Put install logic here.
    /// </summary>
    /// <remarks>Requires elevated permissions.</remarks>
    [CustomAction]
    public static ActionResult InstallDrivers(Session session)
    {
        if (bool.Parse(session.CustomActionData[CustomProperties.DoNotTouchDriver]))
        {
            session.Log("Skipping driver installation requested");
            return ActionResult.Success;
        }

        session.Log("Starting driver installation");

        DirectoryInfo installDir = new(session.CustomActionData["INSTALLDIR"]);
        string driversDir = Path.Combine(installDir.FullName, "drivers", ArchitectureInfo.PlatformShortName, "HidHide");
        string infPath = Path.Combine(driversDir, "HidHide.inf");

        if (!Devcon.Create(HidHideDriver.HidHideClassName, HidHideDriver.HidHideClassGuid,
                HidHideDriver.HidHideHardwareId))
        {
            session.Log($"Device node creation failed, win32Error: {Win32Exception.GetMessageFor()}");
            return ActionResult.Failure;
        }

        if (!Devcon.Install(infPath, out bool rebootRequired))
        {
            session.Log($"Driver installation failed, win32Error: {Win32Exception.GetMessageFor()}");
            return ActionResult.Failure;
        }

        try
        {
            DeviceClassFilters.AddUpper(DeviceClassIds.HumanInterfaceDevices, HidHideDriver.HidHideServiceName);
            DeviceClassFilters.AddUpper(DeviceClassIds.XnaComposite, HidHideDriver.HidHideServiceName);
            DeviceClassFilters.AddUpper(DeviceClassIds.XboxComposite, HidHideDriver.HidHideServiceName);
        }
        catch (Exception ex)
        {
            session.Log($"FTL: class filter updates failed: {ex}");
            return ActionResult.Failure;
        }

        if (rebootRequired)
        {
            Record record = new(1);
            record[1] = "9000";

            session.Message(
                InstallMessage.User | (InstallMessage)MessageButtons.OK | (InstallMessage)MessageIcon.Information,
                record);
        }

        return ActionResult.Success;
    }

    /// <summary>
    ///     Elevated driver removal actions.
    /// </summary>
    public static bool UninstallDrivers(Session session)
    {
        if (bool.Parse(session.CustomActionData[CustomProperties.DoNotTouchDriver]))
        {
            session.Log("Skipping driver removal requested");
            return false;
        }

        session.Log("!!! Would uninstall now");

        return false;
    }
}