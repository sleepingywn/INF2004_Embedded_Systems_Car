# Simple serial monitor for the Pico (USB CDC).
# Usage:  powershell -ExecutionPolicy Bypass -File monitor.ps1 [-Port COM3]
# Press 'r' to reset the IR test's min/max, 'q' to quit.
param([string]$Port = "COM3")

$sp = New-Object System.IO.Ports.SerialPort $Port, 115200, None, 8, One
$sp.DtrEnable = $true      # Pico only sends USB serial output when DTR is set
$sp.RtsEnable = $true
$sp.ReadTimeout = 50

try {
    $sp.Open()
} catch {
    Write-Host "Could not open $Port : $($_.Exception.Message)" -ForegroundColor Red
    Write-Host "Close MicroPico / any other serial monitor, check the port in Device Manager, then retry."
    exit 1
}

Write-Host "Connected to $Port. Press 'r' to reset, 'q' to quit." -ForegroundColor Green
try {
    while ($true) {
        try { $line = $sp.ReadLine(); Write-Host $line.TrimEnd() } catch [System.TimeoutException] { }
        if ([Console]::KeyAvailable) {
            $k = [Console]::ReadKey($true).KeyChar
            if ($k -eq 'q') { break }
            $sp.Write([string]$k)
        }
    }
} finally {
    $sp.Close()
    Write-Host "Disconnected."
}
