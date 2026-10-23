# Pinball's own copy of ogles2-uae's tests/launch-uae.ps1: the test WinUAE (32-bit,
# Tests-folder.uae) with the volume OGLTEST: = D:\dev\ogles2-uae\tests\run, whose startup it
# runs, the window moved to the second monitor -- and the mouse NOT captured
# (win32.start_not_captured), so the user's pointer stays theirs.
param([string]$Startup = '')
if ($Startup -ne '') { Copy-Item -Force $Startup 'D:\dev\ogles2-uae\tests\run\startup' }
$p = Start-Process -FilePath 'D:\Amiga\AmiKit X\WinUAE\winuae.exe' -WorkingDirectory 'D:\Amiga\AmiKit X\WinUAE' -ArgumentList @('-f','Tests-folder.uae','-s','filesystem2=rw,ogltest:OGLTEST:D:\dev\ogles2-uae\tests\run,-128','-s','win32.start_not_captured=true') -PassThru
Start-Process -WindowStyle Hidden -FilePath 'powershell.exe' -ArgumentList @('-NoProfile','-ExecutionPolicy','Bypass','-File','D:\dev\nfs3recompiled\build-amiga\move-uae-window.ps1','-UaeProcessId',$p.Id)
$p.Id
