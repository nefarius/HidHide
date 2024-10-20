Param(
    [Parameter(Mandatory=$true)]
    [string]$SetupVersion
) #end param

$signTool = "$(wdkwhere)\signtool.exe"
$timestampUrl = "http://timestamp.digicert.com"
$certName = "Nefarius Software Solutions e.U."

dotnet build -c:Release -p:SetupVersion=$SetupVersion .\HidHideInstaller\HidHideInstaller.csproj

# sign with only one certificate
Invoke-Expression "& `"$signTool`" sign /v /n `"$certName`" /tr $timestampUrl /fd sha256 /td sha256 .\HidHideInstaller\Nefarius_HidHide_Drivers_x64_v$SetupVersion.msi"
