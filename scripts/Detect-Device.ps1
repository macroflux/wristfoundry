param([string] $Port)

. (Join-Path $PSScriptRoot 'Common.ps1')
Enter-WristFoundryIdf

Write-Host 'Present serial ports:'
$ports = Get-CimInstance Win32_SerialPort -ErrorAction SilentlyContinue |
    Select-Object DeviceID, Name, Description, PNPDeviceID
if ($ports) {
    $ports | Format-Table -AutoSize
} else {
    Write-Host '  None detected.'
}

Write-Host 'Present USB devices that may expose ESP32-S3 native USB:'
$usbDevices = Get-PnpDevice -PresentOnly -ErrorAction SilentlyContinue |
    Where-Object {
        $_.Class -in @('Ports', 'USB') -and
        ($_.FriendlyName -match 'ESP|JTAG|Serial|USB')
    } |
    Select-Object Status, Class, FriendlyName, InstanceId
if ($usbDevices) {
    $usbDevices | Format-Table -AutoSize
} else {
    Write-Host '  None detected.'
}

if ($Port) {
    Write-Host "Probing $Port (read-only)..."
    Invoke-Checked -Command python -Arguments @('-m', 'esptool', '--port', $Port, 'chip_id')
    Invoke-Checked -Command python -Arguments @('-m', 'esptool', '--port', $Port, 'flash_id')
}
