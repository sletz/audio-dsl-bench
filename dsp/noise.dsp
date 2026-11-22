damp = hslider("damp", 0.9, 0, 0.999, 0.001);
rnd = +(12345) ~ *(1103515245);
noise = float(rnd) * (1.0/2147483647.0);
process = noise : *(1 - damp) : + ~ *(damp);
