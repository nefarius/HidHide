Param(
    [Parameter(Mandatory = $true)]
    [string]$BuildVersion,
    [Parameter(Mandatory = $true)]
    [string]$Token,
    [Parameter(Mandatory = $false)]
    [string]$Path = "./artifacts",
    [Parameter(Mandatory = $false)]
    [switch]$NoSigning,
    [Parameter(Mandatory = $false)]
    [string]$CertName = "Nefarius Software Solutions"
) #end param

$signTool = "$(wdkwhere)\signtool.exe"
$timestampUrl = "http://timestamp.digicert.com"

function Get-AppVeyorArtifacts
{
    [CmdletBinding(SupportsShouldProcess = $true, ConfirmImpact = 'Low')]
    param(
        [parameter(Mandatory = $true)]
        [string]$Account,
        [parameter(Mandatory = $true)]
        [string]$Project,
        [alias("DownloadDirectory")][string]$Path = '.',
        [string]$Token,
        [string]$Branch,
        [string]$JobName,
        [switch]$Flat,
        $apiUrl = 'https://ci.appveyor.com/api'
    )

    $headers = @{
        'Content-type' = 'application/json'
    }

    if ($Token) { $headers.'Authorization' = "Bearer $token" }

    $errorActionPreference = 'Stop'
    $projectURI = "$apiUrl/projects/$account/$project"
    if ($Branch) { $projectURI = $projectURI + "/build/$Branch" }

    $projectObject = Invoke-RestMethod -Method Get -Uri $projectURI -Headers $headers

    if (-not $projectObject.build.jobs) { throw "No jobs found for this project or the project and/or account name was incorrectly specified" }

    if (($projectObject.build.jobs.count -gt 1) -and -not $jobName) {
        throw "Multiple Jobs found for the latest build. Please specify the -JobName parameter to select which job you want the artifacts for"
    }

    if ($JobName) {
        $jobid = ($projectObject.build.jobs | Where-Object name -eq "$JobName" | Select-Object -first 1).jobid
        if (-not $jobId) { throw "Unable to find a job named $JobName within the latest specified build. Did you spell it correctly?" }
    }
    else {
        $jobid = $projectObject.build.jobs[0].jobid
    }

    $artifacts = Invoke-RestMethod -Method Get -Uri "$apiUrl/buildjobs/$jobId/artifacts" -Headers $headers
    $artifacts `
    | ? { $psCmdlet.ShouldProcess($_.fileName) } `
    | % {
        $localArtifactPath = $_.fileName -split '/' | % { [Uri]::UnescapeDataString($_) }
        if ($flat.IsPresent) {
            $localArtifactPath = ($localArtifactPath | select -Last 1)
        }
        else {
            $localArtifactPath = $localArtifactPath -join [IO.Path]::DirectorySeparatorChar
        }
        $localArtifactPath = Join-Path $path $localArtifactPath

        $artifactUrl = "$apiUrl/buildjobs/$jobId/artifacts/$($_.fileName)"

        New-Item -ItemType Directory -Force -Path (Split-Path -Path $localArtifactPath) | Out-Null
        Invoke-RestMethod -Method Get -Uri $artifactUrl -OutFile $localArtifactPath -Headers $headers
    }
}

# Fresh download
if (Test-Path $Path) { Remove-Item -Recurse -Force $Path }
New-Item -ItemType Directory -Force -Path $Path | Out-Null

Get-AppVeyorArtifacts -Account "nefarius" -Project "HidHide" -Path $Path -Token $Token -Branch $BuildVersion

if ($NoSigning -eq $false) {
    $msiFiles = Get-ChildItem -Path $Path -Recurse -Filter "*.msi" | % { "`"$($_.FullName)`"" }
    $cabFiles = Get-ChildItem -Path (Join-Path $Path "disk1") -Filter "*.cab" -ErrorAction SilentlyContinue | % { "`"$($_.FullName)`"" }

    $files = @($msiFiles + $cabFiles) -join " "
    if ([string]::IsNullOrWhiteSpace($files)) {
        throw "No MSI or CAB files found under $Path to sign."
    }

    Invoke-Expression "& `"$signTool`" sign /v /as /n `"$CertName`" /tr $timestampUrl /fd sha256 /td sha256 $files"
}

"HidHide Release v$BuildVersion $(Get-Date -Format ""dd.MM.yyyy"")"

