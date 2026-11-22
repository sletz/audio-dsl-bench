import("stdfaust.lib");
// 32 Schroeder allpass combs in series (fi.allpass_comb), alternating +-0.6.
L(i) = ba.take(i+1, (113, 127, 137, 149, 157, 173, 191, 211, 223, 239, 263, 283, 307, 337, 367, 397, 431, 467, 509, 557, 599, 653, 709, 769, 839, 911, 991, 1087, 1171, 1277, 1381, 1499));
g(i) = select2(i % 2, 0.6, -0.6);
process = seq(i, 32, fi.allpass_comb(2048, L(i), g(i)));
