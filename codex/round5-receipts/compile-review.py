import concurrent.futures,json,pathlib,shlex,subprocess
root=pathlib.Path.cwd()
lane=pathlib.Path('/home/xana/work/oot/oot-lane-p/lichii')
old='/home/xana/.hermes/profiles/xana-one/cache/scratch/oot-lane-p/lichii'
rows=json.loads((lane/'build-check/compile_commands.json').read_text())
out=root/'build-gather/round5/compile'
out.mkdir(exist_ok=True)
files=['src/common/nx2_sample_profile.cpp','src/video_core/gpu_split.cpp',
       'src/video_core/gpu_thread.cpp','src/video_core/renderer_vulkan/vk_rasterizer.cpp',
       'src/video_core/renderer_vulkan/vk_graphics_pipeline.cpp',
       'src/video_core/renderer_vulkan/vk_texture_cache.cpp',
       'src/video_core/renderer_vulkan/vk_scheduler.cpp',
       'src/tests/video_core/fast_draw.cpp','src/tests/video_core/memory_tracker.cpp',
       'src/tests/video_core/parallel_record.cpp','src/tests/video_core/parallel_record_vulkan.cpp']
def compile_file(name):
    row=next((r for r in rows if r['file'].endswith('/'+name)), next(r for r in rows if r['file'].endswith('/src/video_core/renderer_vulkan/vk_rasterizer.cpp')))
    args=shlex.split(row['command'])
    flags=[];skip=False
    for arg in args[1:]:
        if skip: skip=False;continue
        if arg=='-o':skip=True;continue
        if arg=='-c' or arg==row['file'] or arg.startswith('-O') or arg.startswith('-Werror') or arg.startswith('-mtls-dialect='):continue
        arg=arg.replace(old+'/src',str(root/'src')).replace(old,str(lane))
        flags.append(arg)
    target=out/(pathlib.Path(name).name+'.o')
    cmd=['clang++-18',*flags,'-O0','-Wno-unknown-warning-option','-c',str(root/name),'-o',str(target)]
    (out/(pathlib.Path(name).name+'.command.json')).write_text(json.dumps(cmd))
    proc=subprocess.run(cmd,cwd=lane/'build-check',capture_output=True,text=True)
    (out/(pathlib.Path(name).name+'.log')).write_text(proc.stdout+proc.stderr)
    print(name,'exit',proc.returncode,flush=True)
    if proc.returncode:print((proc.stdout+proc.stderr)[-1500:],flush=True)
    return proc.returncode
with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool:
    codes=list(pool.map(compile_file,files))
raise SystemExit(any(codes))
