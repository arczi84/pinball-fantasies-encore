# Moves a WinUAE window to the primary monitor (WinUAE reopens where it was last).
param([int]$UaeProcessId)
Add-Type -AssemblyName System.Windows.Forms
Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class PfWin {
  [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr after, int x, int y, int cx, int cy, uint flags);
}
"@
$screen = [System.Windows.Forms.Screen]::PrimaryScreen
$p = Get-Process -Id $UaeProcessId -ErrorAction Stop
[void][PfWin]::SetWindowPos($p.MainWindowHandle, [IntPtr]::Zero, $screen.Bounds.X + 40, $screen.Bounds.Y + 40, 0, 0, 0x0015)
