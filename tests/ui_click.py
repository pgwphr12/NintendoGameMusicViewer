"""Exercise the native application's own hit testing via window messages."""
import ctypes, os, struct, subprocess, sys, time
from ctypes import wintypes
from pathlib import Path
exe=Path(sys.argv[1]).resolve(); fixtures=Path(sys.argv[2]).resolve()
user=ctypes.WinDLL('user32',use_last_error=True)
callback=ctypes.WINFUNCTYPE(wintypes.BOOL,wintypes.HWND,wintypes.LPARAM)
user.EnumWindows.argtypes=[callback,wintypes.LPARAM]
user.GetWindowThreadProcessId.argtypes=[wintypes.HWND,ctypes.POINTER(wintypes.DWORD)]
user.SendMessageW.argtypes=[wintypes.HWND,wintypes.UINT,wintypes.WPARAM,wintypes.LPARAM]
user.SendMessageW.restype=wintypes.LPARAM
user.GetClassNameW.argtypes=[wintypes.HWND,wintypes.LPWSTR,ctypes.c_int]
class Rect(ctypes.Structure):_fields_=[('left',ctypes.c_long),('top',ctypes.c_long),('right',ctypes.c_long),('bottom',ctypes.c_long)]
user.GetClientRect.argtypes=[wintypes.HWND,ctypes.POINTER(Rect)]
for clicks in [1,2,3]:
 target=fixtures/'screens'/f'click-mute-{clicks}.bmp'
 process=subprocess.Popen([str(exe),'--snapshot',str(fixtures/'fds.nsf'),str(target)],env=dict(os.environ,SDL_AUDIODRIVER='dummy'),creationflags=subprocess.CREATE_NO_WINDOW)
 hwnd=None
 for _ in range(50):
  matches=[]
  @callback
  def visit(w,_):
   pid=wintypes.DWORD();user.GetWindowThreadProcessId(w,ctypes.byref(pid))
   name=ctypes.create_unicode_buffer(128);user.GetClassNameW(w,name,128)
   if pid.value==process.pid and name.value=="NintendoGameMusicViewerWindow":matches.append(w)
   return True
  user.EnumWindows(visit,0)
  if matches:hwnd=matches[0];break
  time.sleep(.01)
 assert hwnd,'Native test window not found'
 time.sleep(.15)
 # Resize into/out of fullscreen to exercise buffer replacement and restored hit testing.
 user.SendMessageW(hwnd,0x100,0x7a,0) # F11
 user.SendMessageW(hwnd,0x100,0x1b,0) # Escape
 user.SendMessageW(hwnd,0xf,0,0) # Paint the restored size
 rect=Rect();user.GetClientRect(hwnd,ctypes.byref(rect))
 scale=min(rect.right/1920,rect.bottom/1080)
 x=int((rect.right-1920*scale)/2+100*scale)
 y=int((rect.bottom-1080*scale)/2+(880 if clicks==3 else 310)*scale)
 for _ in range(1 if clicks==3 else clicks):
  user.SendMessageW(hwnd,0x201,1,x|(y<<16))
  user.SendMessageW(hwnd,0x202,0,x|(y<<16))
 assert process.wait(timeout=15)==0
 data=target.read_bytes();offset=struct.unpack_from('<I',data,10)[0]
 width,height=struct.unpack_from('<ii',data,18);height=abs(height)
 colors=[]
 # Count exact first-lane waveform colors, excluding labels and volume slider.
 for row in range(int((860 if clicks==3 else 288)*scale),int((990 if clicks==3 else 374)*scale)):
  begin=offset+row*width*4+int(280*scale)*4
  end=offset+row*width*4+int(1856*scale)*4
  colors.extend(data[i:i+3] for i in range(begin,end,4))
 expected=bytes((119,103,96)) if clicks in (1,3) else bytes((87,205,247))
 assert colors.count(expected)>100,(clicks,colors.count(expected))
 assert colors.count(bytes((87,205,247)))==0 if clicks==1 else True
 print(f'Fullscreen restore + channel click case {clicks}: mute/unmute and hidden-DMC index mapping PASS')
