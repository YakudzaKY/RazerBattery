# List all Razer devices (VID 1532)
Write-Host "Searching for Razer devices (VID 1532)..." -ForegroundColor Cyan

$devices = Get-PnpDevice | Where-Object { $_.InstanceId -match "VID_1532" }

if ($devices) {
    $devices | Select-Object Status, Class, FriendlyName, InstanceId | Format-Table -AutoSize -Wrap
} else {
    Write-Host "No Razer devices found." -ForegroundColor Yellow
}

Write-Host "`nPress any key to exit..."
$null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")
