param([int]$UaeProcessId)
Add-Type -AssemblyName System.Windows.Forms
Add-Type @"
using System; using System.Runtime.InteropServices;
public static class PfW { [StructLayout(LayoutKind.Sequential)] public struct R { public int L,T,Rt,B; }
 [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out R r); }
"@
[System.Windows.Forms.Screen]::AllScreens | ForEach-Object { "screen $($_.DeviceName) primary=$($_.Primary) $($_.Bounds)" }
$p = Get-Process -Id $UaeProcessId; $r = New-Object PfW+R; [void][PfW]::GetWindowRect($p.MainWindowHandle, [ref]$r); "window $($r.L),$($r.T) - $($r.Rt),$($r.B) handle $($p.MainWindowHandle)"
