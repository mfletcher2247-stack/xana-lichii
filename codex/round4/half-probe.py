#!/usr/bin/env python3
"""Check H0_NH1 on a local SM86 GPU using one modified HFMA2 instruction."""
import argparse
import ctypes as C
from pathlib import Path
import struct


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('cubin', type=Path, help='nvcc -cubin -arch=sm_86 half-probe.cu output')
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    data = bytearray(args.cubin.read_bytes())
    if data[:6] != b'\x7fELF\x02\x01':
        raise ValueError('expected little-endian ELF64 cubin')
    shoff = struct.unpack_from('<Q', data, 40)[0]
    size, count, names = struct.unpack_from('<HHH', data, 58)
    headers = [struct.unpack_from('<IIQQQQIIQQ', data, shoff + i * size) for i in range(count)]
    header = headers[names]
    table = data[header[4]:header[4] + header[5]]
    text = next(h for h in headers
                if bytes(table[h[0]:]).split(b'\0')[0] == b'.text.half_probe')
    matches = [offset for offset in range(text[4], text[4] + text[5], 16)
               if int.from_bytes(data[offset:offset + 16], 'little') & 4095
               in (0x231, 0x431, 0x631, 0x831, 0xa31)]
    if len(matches) != 1:
        raise ValueError('expected exactly one HFMA2')
    offset = matches[0]
    if data[offset + 10] & 0x40:
        raise ValueError('HFMA2 already has H0_NH1')
    lib = C.CDLL('libcuda.so.1')

    def call(name, *params):
        result = getattr(lib, name)(*params)
        if result:
            raise RuntimeError((name, result))

    call('cuInit', 0)
    device = C.c_int()
    call('cuDeviceGet', C.byref(device), 0)
    context = C.c_void_p()
    call('cuCtxCreate_v2', C.byref(context), 0, device)
    try:
        for mixed, expected in ((False, 0x4b804800), (True, 0x4a00c900)):
            raw = bytearray(data)
            if mixed:
                raw[offset + 10] |= 0x40
            path = args.output / ('half-mixed.cubin' if mixed else 'half-plain.cubin')
            path.write_bytes(raw)
            module = C.c_void_p()
            call('cuModuleLoad', C.byref(module), str(path).encode())
            try:
                function = C.c_void_p()
                call('cuModuleGetFunction', C.byref(function), module, b'half_probe')
                output = C.c_uint64()
                call('cuMemAlloc_v2', C.byref(output), C.c_size_t(4))
                try:
                    values = [output, C.c_uint32(0x42004000), C.c_uint32(0x45004400), C.c_uint32(0)]
                    params = (C.c_void_p * 4)(*[C.cast(C.byref(v), C.c_void_p) for v in values])
                    call('cuLaunchKernel', function, 1, 1, 1, 1, 1, 1, 0, None, params, None)
                    call('cuCtxSynchronize')
                    result = C.c_uint32()
                    call('cuMemcpyDtoH_v2', C.byref(result), output, C.c_size_t(4))
                    print('H0_NH1' if mixed else 'plain', hex(result.value),
                          struct.unpack('<ee', struct.pack('<I', result.value)), flush=True)
                    if result.value != expected:
                        raise ValueError('hardware result differs from lowering semantics')
                finally:
                    call('cuMemFree_v2', output)
            finally:
                call('cuModuleUnload', module)
    finally:
        call('cuCtxDestroy_v2', context)


if __name__ == '__main__':
    main()
