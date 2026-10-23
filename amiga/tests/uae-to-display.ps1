# Moves a WinUAE window onto the given display (\\.\DISPLAY1, \\.\DISPLAY2...), not activating it.
param([int]$UaeProcessId, [string]$Display = '\\.\DISPLAY2')
Add-Type -AssemblyName System.Windows.Forms
Add-Type @"
using System; using System.Runtime.InteropServices;
public static class PfD { [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr a, int x, int y, int cx, int cy, uint f); }
"@
$s = [System.Windows.Forms.Screen]::AllScreens | Where-Object { $_.DeviceName -eq $Display } | Select-Object -First 1
$p = Get-Process -Id $UaeProcessId
[void][PfD]::SetWindowPos($p.MainWindowHandle, [IntPtr]::Zero, $s.Bounds.X + 40, $s.Bounds.Y + 12, 0, 0, 0x0015)
