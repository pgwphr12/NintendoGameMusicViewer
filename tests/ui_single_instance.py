"""Real process handoff, simultaneous launch, icon and activation checks."""
import ctypes, os, shutil, subprocess, sys, time, uuid
from ctypes import wintypes as w
from pathlib import Path
exe=Path(sys.argv[1]).resolve(); root=Path(sys.argv[2]).resolve()
user=ctypes.WinDLL('user32',use_last_error=True)
callback=ctypes.WINFUNCTYPE(w.BOOL,w.HWND,w.LPARAM)
user.EnumWindows.argtypes=[callback,w.LPARAM]
user.GetWindowThreadProcessId.argtypes=[w.HWND,ctypes.POINTER(w.DWORD)]
user.GetClassNameW.argtypes=[w.HWND,w.LPWSTR,ctypes.c_int]
user.GetWindowTextW.argtypes=[w.HWND,w.LPWSTR,ctypes.c_int]
user.SendMessageW.argtypes=[w.HWND,w.UINT,w.WPARAM,w.LPARAM];user.SendMessageW.restype=w.LPARAM
user.PostMessageW.argtypes=[w.HWND,w.UINT,w.WPARAM,w.LPARAM]
user.ShowWindow.argtypes=[w.HWND,ctypes.c_int]
user.IsIconic.argtypes=[w.HWND];user.IsIconic.restype=w.BOOL
class CopyData(ctypes.Structure):
    _fields_=[('tag',ctypes.c_size_t),('size',w.DWORD),('data',ctypes.c_void_p)]
def windows(pid):
    found=[]
    @callback
    def visit(hwnd,_):
        p=w.DWORD();user.GetWindowThreadProcessId(hwnd,ctypes.byref(p))
        name=ctypes.create_unicode_buffer(128);user.GetClassNameW(hwnd,name,128)
        if p.value==pid and name.value=='NintendoGameMusicViewerWindow':found.append(hwnd)
        return True
    user.EnumWindows(visit,0);return found
def wait_for(test, message, timeout=15):
    end=time.monotonic()+timeout
    while time.monotonic()<end:
        result=test()
        if result:return result
        time.sleep(.025)
    raise AssertionError(message)
def title(hwnd):
    text=ctypes.create_unicode_buffer(2048);user.GetWindowTextW(hwnd,text,2048);return text.value
folder=root/('handoff-'+uuid.uuid4().hex);folder.mkdir()
a=folder/'첫 번째 & 음악.nsf';b=folder/'두 번째 음악.nsf'
source=next(root.glob('*.nsf'));shutil.copyfile(source,a);shutil.copyfile(source,b)
env=dict(os.environ,SDL_AUDIODRIVER='dummy',NGMV_TEST_SESSION=uuid.uuid4().hex)
processes=[]
def launch(*args):
    p=subprocess.Popen([str(exe),*map(str,args)],env=env);processes.append(p);return p
try:
    primary=launch(a);hwnd=wait_for(lambda:windows(primary.pid),'Primary window missing')[0]
    wait_for(lambda:a.name in title(hwnd),'Initial Unicode file missing')
    assert user.SendMessageW(hwnd,0x7f,0,0),'Small embedded icon missing'
    assert user.SendMessageW(hwnd,0x7f,1,0),'Large embedded icon missing'
    bad=ctypes.create_unicode_buffer(str(b));request=CopyData(0xdead,ctypes.sizeof(bad),ctypes.cast(bad,ctypes.c_void_p))
    assert user.SendMessageW(hwnd,0x4a,0,ctypes.addressof(request))==0,'Invalid protocol accepted'
    assert a.name in title(hwnd),'Invalid message changed file'
    user.ShowWindow(hwnd,6)
    secondary=launch('--play',b);assert secondary.wait(timeout=20)==0,'Forwarding process failed'
    wait_for(lambda:b.name in title(hwnd),'File not transferred to existing window')
    wait_for(lambda:not user.IsIconic(hwnd),'Minimized window not restored')
    assert primary.poll() is None and not windows(secondary.pid),'Duplicate player appeared'
    activation=launch();assert activation.wait(timeout=20)==0
    assert b.name in title(hwnd),'Activation changed loaded track'
    for path in (a,b,a):
        p=launch(path);assert p.wait(timeout=20)==0
    wait_for(lambda:a.name in title(hwnd),'Sequential handoff lost last file')
    user.PostMessageW(hwnd,0x10,0,0);assert primary.wait(timeout=20)==0
    # The same mutex also handles two launches before a first window exists.
    one=launch(a);two=launch(b)
    wait_for(lambda:(one.poll()==0 and windows(two.pid)) or (two.poll()==0 and windows(one.pid)), 'Cold simultaneous launch failed',20)
    owner=two if one.poll()==0 else one;window=windows(owner.pid)[0]
    assert len(windows(owner.pid))==1
    user.PostMessageW(window,0x10,0,0);assert owner.wait(timeout=20)==0
    print('Unicode file handoff, icons, invalid IPC, minimized restore, existing-instance activation and simultaneous launch: PASS')
finally:
    for p in processes:
        if p.poll() is None:p.terminate();p.wait(timeout=5)
    shutil.rmtree(folder)
