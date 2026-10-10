"""Decline prompts in a process with HKCU redirected to an isolated test key."""
import ctypes, os, subprocess, sys, time
from ctypes import wintypes as w
user=ctypes.WinDLL('user32')
callback=ctypes.WINFUNCTYPE(w.BOOL,w.HWND,w.LPARAM)
user.EnumWindows.argtypes=[callback,w.LPARAM]
user.EnumChildWindows.argtypes=[w.HWND,callback,w.LPARAM]
user.GetWindowThreadProcessId.argtypes=[w.HWND,ctypes.POINTER(w.DWORD)]
user.GetClassNameW.argtypes=[w.HWND,w.LPWSTR,ctypes.c_int]
user.GetWindowTextW.argtypes=[w.HWND,w.LPWSTR,ctypes.c_int]
user.SendMessageW.argtypes=[w.HWND,w.UINT,w.WPARAM,w.LPARAM];user.SendMessageW.restype=w.LPARAM
env={k:v for k,v in os.environ.items() if k.upper()!='NGMV_TEST_SESSION'}
p=subprocess.Popen([sys.argv[1],'--prompt-test'],env=env)
def text(hwnd):
    t=ctypes.create_unicode_buffer(4096);user.GetWindowTextW(hwnd,t,4096);return t.value
def controls():
    result=[]
    @callback
    def child(hwnd,_):result.append((hwnd,text(hwnd)));return True
    @callback
    def visit(hwnd,_):
        pid=w.DWORD();user.GetWindowThreadProcessId(hwnd,ctypes.byref(pid))
        name=ctypes.create_unicode_buffer(128);user.GetClassNameW(hwnd,name,128)
        if pid.value==p.pid and name.value=='#32770':
            result.append((hwnd,'__dialog__'))
            user.EnumChildWindows(hwnd,child,0)
        return True
    user.EnumWindows(visit,0);return result
def wait_controls(label):
    deadline=time.monotonic()+10
    while time.monotonic()<deadline:
        found=controls()
        if any(value==label for _,value in found):return found
        assert p.poll() is None,'Dialog process exited early'
        time.sleep(.03)
    raise AssertionError('Prompt missing: '+label)
try:
    korean=wait_controls('나중에')
    # TaskDialog renders its body with DirectUI, rather than child HWND text.
    # The extension list is checked against the registry plan in DesktopTests.
    dialog=next(h for h,t in korean if t=='__dialog__')
    # Verification is a DirectUI element. Use its documented TaskDialog message.
    user.SendMessageW(dialog,0x471,1,1) # TDM_CLICK_VERIFICATION: checked
    user.SendMessageW(next(h for h,t in korean if t=='나중에'),0xf5,0,0)
    english=wait_controls('Not now')
    user.SendMessageW(next(h for h,t in english if t=='Not now'),0xf5,0,0)
    assert p.wait(timeout=10)==0,'Preference or reopen test failed'
    print('Native Korean/English startup dialogs, optional decline, persisted suppression and forced reopen: PASS')
finally:
    if p.poll() is None:p.terminate();p.wait(timeout=5)
    advapi=ctypes.WinDLL('advapi32')
    advapi.RegDeleteTreeW.argtypes=[ctypes.c_void_p,w.LPCWSTR]
    # Only this helper's dedicated PID key, including failure/timeout cleanup.
    advapi.RegDeleteTreeW(ctypes.c_void_p(-2147483647),'Software\\NGMVDesktopTests\\'+str(p.pid))
