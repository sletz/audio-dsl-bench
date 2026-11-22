import("stdfaust.lib");
room = hslider("room", 0.84, 0, 0.99, 0.001);
damp = hslider("damp", 0.2, 0, 1, 0.001);
C(i) = ba.take(i+1, (1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617));
A(i) = ba.take(i+1, (556, 441, 341, 225));
comb(L) = (+ : @(L)) ~ (*(1-damp) : (+ ~ *(damp)) : *(room));
ap(L) = _ <: ((((+ : @(L-1)) ~ *(0.5)) : mem), *(-1)) :> _;
process = *(0.1) <: par(i, 8, comb(C(i))) :> seq(i, 4, ap(A(i)));
