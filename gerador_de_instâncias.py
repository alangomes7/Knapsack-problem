import random

def gen_instance(m, n, ne, b, benefit_max=600, dep_max=400, seed=None):
    if seed is not None:
        random.seed(seed)
    benefits = [random.randint(10, benefit_max) for _ in range(m)]
    deps = [random.randint(5, dep_max) for _ in range(n)]
    # garante pares únicos até m*n
    all_pairs = [(i, j) for i in range(m) for j in range(n)]
    random.shuffle(all_pairs)
    if ne > len(all_pairs):
        raise ValueError("ne cannot exceed m*n")
    pairs = all_pairs[:ne]
    out = []
    out.append(f"{m} {n} {ne} {b}")
    out.append(" ".join(map(str, benefits)))
    out.append(" ".join(map(str, deps)))
    for p in pairs:
        out.append(f"{p[0]} {p[1]}")
    return "\n".join(out)

# exemplo:
print(gen_instance(10, 15, 60, 3000, seed=42))