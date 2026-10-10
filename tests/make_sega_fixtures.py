"""Authored chip-register sequences; no game music is included."""
from pathlib import Path
import sys, struct, gzip, math
out = Path(sys.argv[1]); out.mkdir(parents=True, exist_ok=True)
def vgm(commands, fm=0, opll=0):
    header = bytearray(64); header[:4] = b'Vgm '
    for offset,value in [(4,60+len(commands)),(8,0x150),(12,3579545),(16,opll),(44,fm)]:
        struct.pack_into('<I',header,offset,value)
    return bytes(header)+commands
psg = bytearray()
for ch,period in enumerate([220,330,440]):
    psg.extend([0x50,0x80|(ch<<5)|(period&15),0x50,period>>4,0x50,0x90|(ch<<5)])
psg.extend([0x50,0xe4,0x50,0xf3])
wait = bytes([0x61,0xff,0xff])*3 + bytes([0x66])
sms = vgm(psg+wait)
(out/'sms.vgm').write_bytes(sms)
(out/'sms.vgz').write_bytes(gzip.compress(sms,mtime=0))
opll = bytearray(psg)
for ch,freq in enumerate([170,230]):
    for reg,value in [(0x10+ch,freq),(0x20+ch,0x17),(0x30+ch,0x10)]:
        opll.extend([0x51,reg,value])
(out/'sms-fm.vgm').write_bytes(vgm(opll+wait,opll=3579545))
rhythm=bytearray(opll)
for ch in [6,7,8]:
    for reg,value in [(0x10+ch,180),(0x20+ch,0x07),(0x30+ch,0x00)]:
        rhythm.extend([0x51,reg,value])
rhythm.extend([0x51,0x0e,0x3f])
(out/'sms-rhythm.vgm').write_bytes(vgm(rhythm+wait,opll=3579545))
md = bytearray(psg)
for ch in range(2):
    for slot in [0,4,8,12]:
        for reg,value in [(0x30,1),(0x40,0),(0x50,31),(0x60,8),(0x70,0),(0x80,15)]:
            md.extend([0x52,reg+slot+ch,value])
    for reg,value in [(0xb0+ch,7),(0xb4+ch,0xc0),(0xa4+ch,0x22),(0xa0+ch,0x69+ch*50),(0x28,0xf0|ch)]:
        md.extend([0x52,reg,value])
md.extend([0x53,0xb6,0xc0,0x52,0x2b,0x80])
bank = bytes(int(128+80*math.sin(i*.12)) for i in range(15000))
md.extend(bytes([0x67,0x66,0])+struct.pack('<I',len(bank))+bank)
md.extend(bytes([0xe0,0,0,0,0])+bytes([0x8a])*len(bank)+bytes([0x66]))
(out/'md.vgm').write_bytes(vgm(md,fm=7670454))
(out/'md.vgz').write_bytes(gzip.compress(vgm(md,fm=7670454),mtime=0))
for name,commands in [('truncated',bytes([0x52,0x30])),('stream',bytes([0x90,0,0,0,0,0x66])),('pcm',bytes([0x80,0x66])),('no-end',bytes([0x62]))]:
    (out/('bad-'+name+'.vgm')).write_bytes(vgm(commands,fm=7670454))
bad = bytearray(gzip.compress(sms,mtime=0)); bad[-8] ^= 1
(out/'bad-crc.vgz').write_bytes(bad)
folder=out/'playlist-sega';folder.mkdir(exist_ok=True)
for name in ['10 Track.vgm','02 Track.vgz','01 Track.vgm']:
    data=vgm(psg+bytes([0x61,0x4b,0x3c,0x66])) # 0.35 seconds
    (folder/name).write_bytes(gzip.compress(data,mtime=0) if name.endswith('vgz') else data)
