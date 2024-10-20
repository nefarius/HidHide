#nullable enable
using System;

using Nefarius.Utilities.DeviceManagement.Drivers;
using Nefarius.Utilities.DeviceManagement.Extensions;
using Nefarius.Utilities.DeviceManagement.PnP;
using Nefarius.Utilities.WixSharp.Util;

using WixToolset.Dtf.WindowsInstaller;

namespace Nefarius.HidHide.Setup;

public static class CustomActions
{
    public static readonly string DoNotTouchDriver = "DO_NOT_TOUCH_DRIVER";
    public static readonly string HhDriverVersion = "HH_DRIVER_VERSION";

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
        session["CustomActionData"] = $"{HhDriverVersion}=" + session[HhDriverVersion];
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
            // You can add any logic you need to handle during the upgrade.
            string customProperty = session["CustomActionData"];
            session.Log("Received property from new version: " + customProperty);

            try
            {
                Version newDriverVersion = Version.Parse(customProperty.Split('=')[1]);

                // found at least one active device driver instance
                if (Devcon.FindByInterfaceGuid(InstallScript.HidHideInterfaceGuid, out PnPDevice device))
                {
                    DriverMeta? driver = device.GetCurrentDriver();

                    if (driver is not null)
                    {
                        session.Log($"Found existing driver with version: {driver.DriverVersion}");

                        if (driver.DriverVersion >= newDriverVersion)
                        {
                            session.Log("Driver is recent, flagging as do-not-touch");
                            // do not remove and re-create if the shipped version is not newer than what is active
                            session[DoNotTouchDriver] = true.ToString();
                        }
                    }
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
            // Handle normal uninstallation here.
        }

        return ActionResult.Success;
    }

    public static bool UninstallDrivers(Session session)
    {
        if (bool.Parse(session.CustomActionData[DoNotTouchDriver]))
        {
            session.Log("Skipping driver removal requested");
            return false;
        }

        session.Log("!!! Would uninstall now");

        return false;
    }
}