param(
    [string]$Port = "COM24",
    [int]$Seconds = 4
)
$sp = New-Object System.IO.Ports.SerialPort($Port, 115200, "None", 8, "One")
$sp.DtrEnable = $false
$sp.RtsEnable = $false
$sp.ReadTimeout = 500
$sp.Open()
$end = (Get-Date).AddSeconds($Seconds)
while ((Get-Date) -lt $end) {
    try {
        Write-Output ($sp.ReadLine())
    } catch [System.TimeoutException] {
    }
}
$sp.Close()
