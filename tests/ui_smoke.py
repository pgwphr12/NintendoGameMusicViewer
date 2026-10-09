"""Native renderer smoke tests; real music core PCM, offscreen windows, no GUI clicks."""
from pathlib import Path
import csv, os, struct, subprocess, sys
exe=Path(sys.argv[1]).resolve();fixtures=Path(sys.argv[2]).resolve();out=fixtures/'screens';out.mkdir(exist_ok=True)
env=dict(os.environ,SDL_AUDIODRIVER='dummy')
for width,height,music,recording in [(1280,720,'fds.nsf',False),(1600,900,'channels.gbs',False),(1920,1080,'fds.nsf',True),(2560,1440,'channels.spc',False),(3840,2160,'channels.spc',True),(1280,720,'channels.minigsf',False),(1920,1080,'channels.mini2sf',True),(1280,720,'channels.miniusf',False),(1920,1080,'channels.bcstm',False),(1280,720,'channels.bcwav',True)]:
    target=out/(f'{width}x{height}.bmp' if music not in ('channels.minigsf','channels.mini2sf','channels.miniusf','channels.bcstm','channels.bcwav') else music+'.bmp')
    args=[str(exe),'--snapshot',str(fixtures/music),str(target),f'--size={width}x{height}']
    if recording:args.append('--recording')
    current_env=dict(env)
    if width==3840:current_env['NGMV_RENDER_BENCHMARK']='1'
    subprocess.run(args,env=current_env,check=True,timeout=30,creationflags=subprocess.CREATE_NO_WINDOW)
    data=target.read_bytes();w,h=struct.unpack_from('<ii',data,18)
    assert (w,abs(h))==(width,height),(target,w,h)
    assert len(data)>=width*height*4
    # At least a range of colored waveform/text pixels beyond a single flat background.
    offset=struct.unpack_from('<I',data,10)[0];colors=set(data[i:i+3] for i in range(offset,len(data)-3,4*43))
    assert len(colors)>30,(target,len(colors))
    print(f'{width}x{height}, {music}, recording={recording}: native frame and layout PASS')

    if width==3840:
        with Path(str(target)+'.render.csv').open() as report:
            stats=next(csv.DictReader(report))
        assert stats['gdi_before']==stats['gdi_after'],stats
        assert int(stats['frames'])==120,stats
        print('4K repeated rendering: GDI object count stable PASS')
