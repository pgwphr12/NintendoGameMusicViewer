"""Original ARM music drivers; no commercial ROM, samples or BIOS."""
from pathlib import Path
import struct,zlib,sys,math
root=Path(sys.argv[1]);root.mkdir(exist_ok=True)
def arm_writes(writes,half=False):
 code=[];literals=[];fix=[]
 for addr,value in writes:
  for reg,val in [(0,addr),(1,value)]:fix.append((len(code),len(literals)));code.append(0xe59f0000|(reg<<12));literals.append(val)
  code.append(0xe1c010b0 if half else 0xe5801000)
 code.append(0xeafffffe)
 for pos,lit in fix:code[pos]|=4*(len(code)+lit-pos-2)
 return b''.join(struct.pack('<I',n) for n in code+literals)
def psf(path,version,exe,tags='',reserved=b''):
 compressed=zlib.compress(exe) if exe else b''
 path.write_bytes(b'PSF'+bytes([version])+struct.pack('<III',len(reserved),len(compressed),zlib.crc32(compressed))+reserved+compressed+(b'[TAG]'+tags.encode() if tags else b''))
rom=arm_writes([(0x4000084,0x80),(0x4000080,0xff77),(0x4000082,2),(0x4000060,0),(0x4000062,0xf080),(0x4000064,0x8700),(0x4000068,0xf040),(0x400006c,0x8600)],True)
exe=struct.pack('<III',0x8000000,0,len(rom))+rom
psf(root/'channels.gsf',0x22,exe,'title=Original GBA channel test\ngame=Original GSF fixture\nlength=0:03\nfade=0:01\n')
psf(root/'channels.gsflib',0x22,exe)
psf(root/'channels.minigsf',0x22,b'','_lib=channels.gsflib\ntitle=Mini GBA library test\nlength=0:03\n')
# Independent Direct Sound FIFO streams at 10512 Hz and 16384 Hz, like real game timers.
pcm_rom=bytearray(0x41000)
driver=arm_writes([(0x4000084,0x80),(0x4000080,0xfb0c0077),
 (0x40000bc,0x8001000),(0x40000c0,0x40000a0),(0x40000c4,0xb6400004),
 (0x40000c8,0x8021000),(0x40000cc,0x40000a4),(0x40000d0,0xb6400004),
 (0x4000100,0x0080f9c4),(0x4000104,0x0080fc00)])
pcm_rom[:len(driver)]=driver
for off,period in [(0x1000,24),(0x21000,37)]:
 pcm_rom[off:off+0x20000]=bytes(int(100*math.sin(i*2*math.pi/period))&255 for i in range(0x20000))
psf(root/'pcm.gsf',0x22,struct.pack('<III',0x8000000,0,len(pcm_rom))+pcm_rom,
 'title=Original GBA PCM reconstruction test\nlength=0:03\n')
arm7=arm_writes([(0x4000500,0x807f),(0x4000488,0xf000),(0x4000480,0xe040007f),(0x4000498,0xf500),(0x4000490,0xe240007f),(0x4000404,0x3800800),(0x4000408,0xfc00),(0x400040c,64),(0x4000400,0x8840007f)])
program=bytearray(0x900);program[:len(arm7)]=arm7
program[0x800:]=bytes(int(110*math.sin(i*2*math.pi/32))&255 for i in range(256))
rom=bytearray(0x204+len(program));rom[:12]=b'NGMVTEST    ';struct.pack_into('<IIII',rom,0x20,0x200,0x2000000,0x2000000,4);struct.pack_into('<IIII',rom,0x30,0x204,0x3800000,0x3800000,len(program));struct.pack_into('<I',rom,0x200,0xeafffffe);rom[0x204:]=program
exe=struct.pack('<II',0,len(rom))+rom
psf(root/'channels.2sf',0x24,exe,'title=Original DS channel test\ngame=Original 2SF fixture\nlength=0:03\n')
one_shot=exe.replace(struct.pack('<I',0x8840007f),struct.pack('<I',0x9040007f))
psf(root/'oneshot.2sf',0x24,one_shot,'title=Original DS one-shot regression\nlength=0:03\n')
psf(root/'channels.2sflib',0x24,exe)
psf(root/'channels.mini2sf',0x24,b'','_lib=channels.2sflib\ntitle=Mini DS library test\nlength=0:03\n')
psf(root/'missing.minigsf',0x22,b'','_lib=absent.gsflib\n')
psf(root/'cycle.minigsf',0x22,b'','_lib=cycle.minigsf\n')
corrupt=bytearray((root/'channels.gsf').read_bytes());corrupt[12]^=1;(root/'bad-crc.gsf').write_bytes(corrupt)
psf(root/'huge-map.gsf',0x22,struct.pack('<III',0x8000000,0x1ffffff,0xffffffff))
psf(root/'truncated-save.2sf',0x24,exe,reserved=b'SAVE')

# Original N64 MIPS driver writes repeated stereo sample blocks to Audio Interface DMA.
state=bytearray(0x75c);struct.pack_into('<II',state,0,0x23d8a6c8,0x400000)
struct.pack_into('<II',state,0x48,500000,0x80000000)
struct.pack_into('<I',state,0x420+16,1513)
code=[0x3c08a450,0x34091000,0xad090000,0x34090400,0xad090004,0x08000001,0]
program=b''.join(struct.pack('<I',n) for n in code)
wave=b''.join(struct.pack('<hh',int(15000*math.sin(i*2*math.pi/21)),int(18000*math.sin(i*2*math.pi/32))) for i in range(256))
def chunk(offset,data):return struct.pack('<II',len(data),offset)+data
reserved=struct.pack('<II',0,0x34365253)+chunk(0,state)+chunk(0x75c,program)+chunk(0x75c+0x1000,wave)+struct.pack('<I',0)
psf(root/'channels.usf',0x21,b'','title=Original N64 AI test\nlength=0:03\n',reserved)
psf(root/'channels.usflib',0x21,b'',reserved=reserved)
psf(root/'channels.miniusf',0x21,b'','_lib=channels.usflib\ntitle=Mini N64 library test\nlength=0:03\n')
psf(root/'bad-map.usf',0x21,b'',reserved=struct.pack('<IIII',0,0x34365253,4,0xffffffff)+b'bad!'+struct.pack('<I',0))
# Native 3DS BCSTM with independent PCM16 stereo; interleaved channel blocks.
rate=32000;samples=rate*3
pcm=[b''.join(struct.pack('<h',int(20000*math.sin(i*2*math.pi*f/rate))) for i in range(samples)) for f in (440,660)]
interleave=4096;payload=b''.join(channel[at:at+interleave] for at in range(0,len(pcm[0]),interleave) for channel in pcm)
info=bytearray(0x100);info[:4]=b'INFO';struct.pack_into('<I',info,4,len(info));info[0x20:0x24]=bytes([1,0,2,0])
struct.pack_into('<IIIIIIIIII',info,0x24,rate,0,samples,(len(pcm[0])+4095)//4096,4096,2048,len(pcm[0])%4096,(len(pcm[0])%4096)//2,len(pcm[0])%4096,0)
header=bytearray(0x200);header[:4]=b'CSTM';struct.pack_into('<HHIIH',header,4,0xfeff,0x40,0x400,0x220+len(payload),2)
struct.pack_into('<HHII',header,0x14,0x4000,0,0x40,len(info));struct.pack_into('<HHII',header,0x20,0x4002,0,0x200,0x20+len(payload));header[0x40:0x140]=info
data=b'DATA'+struct.pack('<I',0x20+len(payload))+bytes(0x18)+payload
(root/'channels.bcstm').write_bytes(header+data)
# Two stereo stems plus a mono stem, with an explicit BCSTM track channel table.
pcm5=[b''.join(struct.pack('<h',int(12000*math.sin(i*2*math.pi*f/rate))) for i in range(samples)) for f in (220,330,440,550,660)]
payload5=b''.join(ch[at:at+4096] for at in range(0,len(pcm5[0]),4096) for ch in pcm5)
info5=bytearray(info);info5[0x22]=5;struct.pack_into('<I',info5,0x14,0x58)
struct.pack_into('<I',info5,0x60,3)
for t,ids in enumerate([(0,1),(2,3),(4,)]):
 track=0xa0+t*0x18
 struct.pack_into('<HHI',info5,0x64+t*8,0x4101,0,track-0x60)
 info5[track:track+4]=bytes([127,64,0,0]);struct.pack_into('<HHI',info5,track+4,0x100,0,12)
 struct.pack_into('<I',info5,track+12,len(ids));info5[track+16:track+16+len(ids)]=bytes(ids)
header5=bytearray(header);struct.pack_into('<I',header5,12,0x220+len(payload5));struct.pack_into('<I',header5,0x28,0x20+len(payload5));header5[0x40:0x140]=info5
(root/'stems.bcstm').write_bytes(header5+b'DATA'+struct.pack('<I',0x20+len(payload5))+bytes(0x18)+payload5)
# Native BCWAV DSP-ADPCM with two zero-predictor coefficient sets and original nibbles.
frames=3000;num_samples=frames*14
adpcm=[]
for phase in (0,4):
 channel=bytearray()
 for frame in range(frames):
  channel.append(12)
  nibbles=[int(7*math.sin((frame*14+j+phase)*2*math.pi/(28 if phase==0 else 42)))&15 for j in range(14)]
  channel.extend((nibbles[j]<<4)|nibbles[j+1] for j in range(0,14,2))
 adpcm.append(channel)
info=bytearray(0x100);info[:4]=b'INFO';struct.pack_into('<I',info,4,len(info));info[8]=2
struct.pack_into('<III',info,0xc,rate,0,num_samples);struct.pack_into('<I',info,0x1c,2)
for c in range(2):
 ch=0x40+c*0x20;coef=0x90+c*0x30
 struct.pack_into('<HHI',info,0x20+c*8,0x7100,0,ch-0x1c)
 struct.pack_into('<HHIHHI',info,ch,0x1f00,0,c*len(adpcm[0]),0x300,0,coef-ch)
header=bytearray(0x140);header[:4]=b'CWAV';struct.pack_into('<HHIIH',header,4,0xfeff,0x40,0x102,0x148+sum(map(len,adpcm)),2)
struct.pack_into('<HHII',header,0x14,0x7000,0,0x40,len(info));struct.pack_into('<HHII',header,0x20,0x7001,0,0x140,8+sum(map(len,adpcm)));header[0x40:]=info
(root/'channels.bcwav').write_bytes(header+b'DATA'+struct.pack('<I',8+sum(map(len,adpcm)))+b''.join(adpcm))
(root/'invalid.bcstm').write_bytes(b'CSTM'+bytes(64))

folder=root/'playlist-ds';folder.mkdir(exist_ok=True)
for number in (1,2,10):
    duration='0:00.35' if number == 1 else '0:03'
    psf(folder/f'{number:02d} Track.mini2sf',0x24,b'',f'_lib=../channels.2sflib\ntitle=Playlist {number}\nlength={duration}\n')
(folder/'ignore.2sflib').write_bytes(b'Not a playable track')
