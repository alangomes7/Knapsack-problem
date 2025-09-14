import random
import datetime

def gen_instance(m, n, ne, b, benefit_max=600, dep_max=400, seed=None):
    """
    Generate a random knapsack-like problem instance with package–dependency relations,
    ensuring each package has at least one dependency.

    The instance format is:
        - First line: "m n ne b"
        - Second line: m integers, benefits of each package
        - Third line: n integers, weights of each dependency
        - Next ne lines: pairs "package dependency"

    Parameters
    ----------
    m : int
        Number of packages.
    n : int
        Number of dependencies.
    ne : int
        Number of package–dependency relations (edges).
        Must satisfy m <= ne <= m * n.
    b : int
        Capacity of the knapsack (disk size limit).
    benefit_max : int, optional
        Maximum possible benefit value for a package (default=600).
    dep_max : int, optional
        Maximum possible weight value for a dependency (default=400).
    seed : int, optional
        Random seed for reproducibility (default=None).

    Returns
    -------
    str
        A string representing the problem instance in the required format.
    """
    if seed is not None:
        random.seed(seed)

    # Validation: ne must be within [m, m*n]
    if ne < m:
        print(f"[!] ne={ne} is too small. Adjusted to {m} to ensure each package has a dependency.")
        ne = m
    elif ne > m * n:
        print(f"[!] ne={ne} is too large. Adjusted to maximum possible {m * n}.")
        ne = m * n

    benefits = [random.randint(10, benefit_max) for _ in range(m)]
    deps = [random.randint(5, dep_max) for _ in range(n)]

    pairs_set = set()

    # Step 1: ensure each package has at least one dependency
    for i in range(m):
        j = random.randint(0, n - 1)
        pairs_set.add((i, j))

    # Step 2: add remaining edges until reaching ne, ensuring uniqueness
    while len(pairs_set) < ne:
        p = random.randint(0, m - 1)
        d = random.randint(0, n - 1)
        pairs_set.add((p, d))

    # Shuffle order for randomness
    pairs = list(pairs_set)
    random.shuffle(pairs)

    out = []
    out.append(f"{m} {n} {ne} {b}")
    out.append(" ".join(map(str, benefits)))
    out.append(" ".join(map(str, deps)))

    for p in pairs:
        out.append(f"{p[0]} {p[1]}")

    return "\n".join(out)


if __name__ == "__main__":
    # 1. Timestamped filename
    timestamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
    filename = f"{timestamp}.knapsack.txt"

    # 2. User input
    try:
        print("▶ Enter values for m, n, ne, b (space-separated).")
        print("  Note: 'ne' must be >= m and <= m*n.")
        print("  Example: 870 900 1500 18383")

        m, n, ne, b = map(int, input().split())

        # Generate instance
        instance_data = gen_instance(m, n, ne, b, seed=42)

        # 3. Save to file
        with open(filename, "w") as f:
            f.write(instance_data)

        print(f"✅ Problem instance successfully saved as: {filename}")

    except ValueError:
        print("❌ Invalid input. Please enter four integers separated by spaces.")
    except IOError as e:
        print(f"❌ File error: {e}")
    except Exception as e:
        print(f"❌ Unexpected error: {e}")
