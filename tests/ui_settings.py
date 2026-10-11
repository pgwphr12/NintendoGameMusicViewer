"""Restart the real desktop app using isolated preferences and a dummy audio device."""
import ctypes, os, subprocess, sys, tempfile, time, uuid
from ctypes import wintypes
from pathlib import Path
exe=Path(sys.argv[1]).resolve();fixtures=Path(sys.argv[2]).resolve()
user=ctypes.WinDLL('user32',use_last_error=True)
callback=ctypes.WINFUNCTYPE(wintypes.BOOL,wintypes.HWND,wintypes.LPARAM)
user.EnumWindows.argtypes=[callback,wintypes.LPARAM]
user.GetWindowThreadProcessId.argtypes=[wintypes.HWND,ctypes.POINTER(wintypes.DWORD)]
user.GetClassNameW.argtypes=[wintypes.HWND,wintypes.LPWSTR,ctypes.c_int]
user.SendMessageW.argtypes=[wintypes.HWND,wintypes.UINT,wintypes.WPARAM,wintypes.LPARAM]
user.SendMessageW.restype=wintypes.LPARAM
class Rect(ctypes.Structure):_fields_=[('left',ctypes.c_long),('top',ctypes.c_long),('right',ctypes.c_long),('bottom',ctypes.c_long)]
user.GetClientRect.argtypes=[wintypes.HWND,ctypes.POINTER(Rect)]
def session(path,click=False):
    env=dict(os.environ,SDL_AUDIODRIVER='dummy',NGMV_TEST_SESSION=str(uuid.uuid4()),NGMV_SETTINGS_TEST_FILE=str(path))
    process=subprocess.Popen([str(exe),str(fixtures/'channels.nsf')],env=env,creationflags=subprocess.CREATE_NO_WINDOW)
    try:
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
            assert process.poll() is None,'App exited early'
            time.sleep(.05)
        assert hwnd,'Window not found'
        time.sleep(1.3)
        if click:
            r=Rect();user.GetClientRect(hwnd,ctypes.byref(r));scale=min(r.right/1920,r.bottom/1080)
            x=int((r.right-1920*scale)/2+1820*scale);y=int((r.bottom-1080*scale)/2+178*scale)
            user.SendMessageW(hwnd,0x201,1,(y<<16)|x)
            user.SendMessageW(hwnd,0x202,0,(y<<16)|x)
        user.SendMessageW(hwnd,0x10,0,0)
        assert process.wait(timeout=15)==0
    finally:
        if process.poll() is None:process.kill();process.wait()
def read(path):return dict(line.split('=',1) for line in path.read_text().splitlines() if '=' in line)
with tempfile.TemporaryDirectory(prefix='ngmv-settings-ui-') as folder:
    path=Path(folder)/'설정.ini'
    session(path);data=read(path)
    keys=[k for k in data if k.startswith('channel.') and k.endswith('.volume')]
    assert keys,'Loaded channels were not saved'
    channel=keys[0]
    data.update(master='37',speed='123',speedStep='1',rate='44100',reverb='1',reverbAmount='63',reverbDecay='23',repeat='1',autoplay='0',recording='1',windowSamples='1024')
    data[channel]='175';data[channel.replace('.volume','.mute')]='1'
    path.write_text(''.join(f'{k}={v}\n' for k,v in data.items()))
    session(path,True);restored=read(path)
    assert restored['master']=='42',restored
    for k in ['speed','speedStep','rate','reverb','reverbAmount','reverbDecay','repeat','autoplay','recording','windowSamples',channel,channel.replace('.volume','.mute')]:assert restored[k]==data[k],(k,data[k],restored[k])
    session(path);again=read(path)
    assert again['master']=='42' and again[channel]=='175'
print('Real app restart: global settings, recording view, +/- volume edit, per-system channel gain/mute: PASS')
