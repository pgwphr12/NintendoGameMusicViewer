"""Package matching runtime and complete corresponding source separately."""
from pathlib import Path
import argparse, hashlib, subprocess, zipfile

parser=argparse.ArgumentParser()
parser.add_argument('--build',required=True,type=Path)
parser.add_argument('--output',required=True,type=Path)
parser.add_argument('--cmake',default='cmake')
args=parser.parse_args()
source=Path(__file__).resolve().parent.parent
output=args.output.resolve();output.mkdir(parents=True,exist_ok=True)
runtime=output/'NintendoGameMusicViewer-V1.3.1-Windows-x64'
subprocess.run([args.cmake,'--install',str(args.build.resolve()),'--config','Release','--component','Runtime','--prefix',str(runtime)],check=True)
assert (runtime/'NintendoGameMusicViewer.exe').is_file() and (runtime/'gme.dll').is_file()
assert not any((runtime/d).exists() for d in ['src','third_party','tests','CMakeLists.txt']), 'Runtime must not contain source'
assert not list(runtime.rglob('*.pdb')) and not list(runtime.rglob('*asan*')), 'Diagnostic artifacts in runtime'
dirs={'src','tests','third_party','docs','licenses','samples','.github','tools'}
files={'CMakeLists.txt','.clang-format','.gitignore','LICENSE','README.md','README.en.md','SOURCE.md','THIRD_PARTY_NOTICES.md','VALIDATION.md'}
for folder,name in [(runtime,'NintendoGameMusicViewer-V1.3.1-Windows-x64.zip'),(source,'NintendoGameMusicViewer-V1.3.1-source.zip')]:
    target=output/name
    with zipfile.ZipFile(target,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
        for f in sorted(folder.rglob('*')):
            rel=f.relative_to(folder)
            if not f.is_file() or '__pycache__' in rel.parts:continue
            if folder==source and rel.parts[0] not in dirs|files:continue
            prefix=folder.name if folder==runtime else 'NintendoGameMusicViewer-V1.3.1-source'
            z.write(f,Path(prefix)/rel)
    with zipfile.ZipFile(target) as z:
        assert z.testzip() is None
        if folder==source:
            assert any(n.endswith('third_party/game-music-emu-0.6.5.zip') for n in z.namelist())
            assert any(n.endswith('third_party/gme-sega-patch/Vgm_Emu.cpp') for n in z.namelist())
        else:
            assert not any(n.endswith(('.cpp','.hpp','.h','.c')) for n in z.namelist())
    digest=hashlib.sha256(target.read_bytes()).hexdigest()
    target.with_suffix('.zip.sha256').write_text(digest+'  '+name+'\n',encoding='ascii')
    print(f'{name}: {target.stat().st_size/1024/1024:.2f} MiB; contents, CRC and SHA256 PASS')
