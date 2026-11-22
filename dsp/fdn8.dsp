import("stdfaust.lib");
g = hslider("g", 0.9, 0, 0.999, 0.001) / sqrt(8.0);
damp = hslider("damp", 0.3, 0, 0.99, 0.001);
L(i) = ba.take(i+1, (1009, 1151, 1279, 1381, 1499, 1607, 1733, 1861));
lp = *(1 - damp) : + ~ *(damp);
sgn(i,j) = (1-2*((i%2)*(j%2))) * (1-2*((int(i/2)%2)*(int(j/2)%2))) * (1-2*((int(i/4)%2)*(int(j/4)%2)));
had = si.bus(8) <: par(i, 8, (par(j, 8, *(sgn(i,j))) :> _));
inmix = (si.bus(8), (_ <: si.bus(8))) :> par(i, 8, @(L(i)-1));
process = (inmix ~ (par(i, 8, lp) : had : par(i, 8, *(g)))) :> *(0.25);
