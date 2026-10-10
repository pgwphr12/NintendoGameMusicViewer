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

# Last channel in the right column must remain visible and clickable in both views.
for mode,recording in [('mute',False),('mute',True),('plus',False),('drag',False)]:
 target=fixtures/'screens'/f'sega-right-{mode}-{recording}.bmp'
 args=[str(exe),'--snapshot',str(fixtures/'sms-fm.vgm'),str(target),'--size=1920x1080']
 if recording:args.append('--recording')
 process=subprocess.Popen(args,env=dict(os.environ,SDL_AUDIODRIVER='dummy'),creationflags=subprocess.CREATE_NO_WINDOW)
 hwnd=None
 for _ in range(100):
  matches=[]
  @callback
  def visit(w,_):
   pid=wintypes.DWORD();user.GetWindowThreadProcessId(w,ctypes.byref(pid))
   name=ctypes.create_unicode_buffer(128);user.GetClassNameW(w,name,128)
   if pid.value==process.pid and name.value=='NintendoGameMusicViewerWindow':matches.append(w)
   return True
  user.EnumWindows(visit,0)
  if matches:hwnd=matches[0];break
  time.sleep(.01)
 assert hwnd
 time.sleep(.15)
 top,bottom=(205,1064) if recording else (276,1000)
 height=(bottom-top)//9;y=top+8*height
 # Channel 18 (PSG noise) is right column row 9, never a page index.
 x,click_y=(1020,y+12) if mode=='mute' else (1112,y+height-28) if mode=='plus' else (1040,y+height-28)
 user.SendMessageW(hwnd,0x201,1,x|(click_y<<16))
 if mode=='drag':user.SendMessageW(hwnd,0x200,1,1076|(click_y<<16))
 user.SendMessageW(hwnd,0x202,0,x|(click_y<<16))
 assert process.wait(timeout=15)==0
 data=target.read_bytes();offset=struct.unpack_from('<I',data,10)[0]
 width,signed_height=struct.unpack_from('<ii',data,18);image_height=abs(signed_height)
 def crop_colors(top,bottom):
  colors=[]
  for row in range(top,bottom):
   physical=row if signed_height<0 else image_height-1-row
   begin=offset+(physical*width+1152)*4
   colors.extend(data[i:i+3] for i in range(begin,begin+704*4,4))
  return colors
 colors=crop_colors(y+8,y+height-8)
 if mode=='mute':
  assert colors.count(bytes((119,103,96)))>500,(recording,colors.count(bytes((119,103,96))))
  assert bytes((190,222,126)) not in colors,'Right-column mute targeted the wrong channel'
 else:
  assert colors.count(bytes((190,222,126)))>500,'Volume operation incorrectly muted channel'
  slider_y=y+height-28;pixel_x=1062 if mode=='plus' else 1075
  physical=slider_y if signed_height<0 else image_height-1-slider_y
  position=offset+(physical*width+pixel_x)*4
  assert data[position:position+3]==bytes((190,222,126)),f'Right-column {mode} did not change slider'
 # Channel 17 immediately above must retain its active colored waveform.
 above=crop_colors(y-height+8,y-8)
 assert above.count(bytes((87,205,247)))>500
 print(f'18 channels, final right-column {mode} and adjacent retention, recording={recording}: PASS')
