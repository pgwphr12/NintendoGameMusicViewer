"""Exercise automatic folder advance and native keyboard navigation."""
import ctypes, os, subprocess, sys, time
from ctypes import wintypes
from pathlib import Path
exe=Path(sys.argv[1]).resolve(); root=Path(sys.argv[2]).resolve()
user=ctypes.WinDLL('user32',use_last_error=True)
callback=ctypes.WINFUNCTYPE(wintypes.BOOL,wintypes.HWND,wintypes.LPARAM)
user.EnumWindows.argtypes=[callback,wintypes.LPARAM]
user.GetWindowThreadProcessId.argtypes=[wintypes.HWND,ctypes.POINTER(wintypes.DWORD)]
user.GetClassNameW.argtypes=[wintypes.HWND,wintypes.LPWSTR,ctypes.c_int]
user.GetWindowTextW.argtypes=[wintypes.HWND,wintypes.LPWSTR,ctypes.c_int]
user.SendMessageW.argtypes=[wintypes.HWND,wintypes.UINT,wintypes.WPARAM,wintypes.LPARAM]
user.SendMessageW.restype=wintypes.LPARAM
process=subprocess.Popen([str(exe),'--snapshot',str(root/'playlist-ds/01 Track.mini2sf'),str(root/'screens/folder-playlist.bmp')],env=dict(os.environ,SDL_AUDIODRIVER='dummy'),creationflags=subprocess.CREATE_NO_WINDOW)
try:
    hwnd=None
    for _ in range(100):
        matches=[]
        @callback
        def visit(w,_):
            pid=wintypes.DWORD();user.GetWindowThreadProcessId(w,ctypes.byref(pid))
            name=ctypes.create_unicode_buffer(128);user.GetClassNameW(w,name,128)
            if pid.value==process.pid and name.value=='NintendoGameMusicViewerWindow': matches.append(w)
            return True
        user.EnumWindows(visit,0)
        if matches: hwnd=matches[0];break
        time.sleep(.01)
    assert hwnd,'Test window missing'
    def title():
        value=ctypes.create_unicode_buffer(2048);user.GetWindowTextW(hwnd,value,2048);return value.value
    for _ in range(100):
        if '02 Track.mini2sf' in title(): break
        assert process.poll() is None,'Application exited during automatic advance'
        time.sleep(.01)
    assert '02 Track.mini2sf' in title(),title()
    user.SendMessageW(hwnd,0x100,0x20,0) # pause before manual navigation
    for key,expected in [(0x27,'10'),(0x27,'01'),(0x25,'10')]:
        user.SendMessageW(hwnd,0x100,key,0)
        user.SendMessageW(hwnd,0xf,0,0) # paint immediately after each transition
        assert expected+' Track.mini2sf' in title(),title()
    class Rect(ctypes.Structure):
        _fields_=[('left',ctypes.c_long),('top',ctypes.c_long),('right',ctypes.c_long),('bottom',ctypes.c_long)]
    user.GetClientRect.argtypes=[wintypes.HWND,ctypes.POINTER(Rect)]
    rect=Rect();user.GetClientRect(hwnd,ctypes.byref(rect));scale=min(rect.right/1920,rect.bottom/1080)
    for logical_x,expected in [(860,'01'),(430,'10')]:
        x=int((rect.right-1920*scale)/2+logical_x*scale);y=int((rect.bottom-1080*scale)/2+166*scale)
        user.SendMessageW(hwnd,0x201,1,x|(y<<16));user.SendMessageW(hwnd,0x202,0,x|(y<<16))
        assert expected+' Track.mini2sf' in title(),title()
    user.SendMessageW(hwnd,0x100,0x22,0) # page down: channels 9-16
    user.SendMessageW(hwnd,0x100,0x78,0) # recording view
    user.SendMessageW(hwnd,0xf,0,0)
    assert process.wait(timeout=15)==0
    print('DS automatic folder advance, natural order, previous/next keys/buttons and wrap and page/recording repaint: PASS')
finally:
    if process.poll() is None: process.terminate();process.wait(timeout=5)
