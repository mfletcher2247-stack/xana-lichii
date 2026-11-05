extern "C" __global__ void half_probe(unsigned *out, unsigned a, unsigned b, unsigned c) {
    unsigned d;
    asm volatile("fma.rn.f16x2 %0, %1, %2, %3;" : "=r"(d) : "r"(a), "r"(b), "r"(c));
    *out = d;
}
