import sys
import matplotlib
matplotlib.use("Agg")                 # no display needed: just write the PNG
import matplotlib.pyplot as plt
import pandas as pd

csv_path = sys.argv[1]
png_path = sys.argv[2] if len(sys.argv) > 2 else "latency_hist.png"

df = pd.read_csv(csv_path)

# Cross-check against the C++ report
print(df.groupby("category")["ns"].describe(percentiles=[0.5, 0.9, 0.99, 0.999]).round(0))

for cat, g in df.groupby("category"):
    plt.hist(g["ns"].clip(upper=g["ns"].quantile(0.999)),
             bins=100, alpha=0.5, label=cat, log=True)

plt.xlabel("latency (ns, clipped at p99.9)")
plt.ylabel("count (log scale)")
plt.legend()
plt.savefig(png_path, dpi=150)
print(f"saved {png_path}")