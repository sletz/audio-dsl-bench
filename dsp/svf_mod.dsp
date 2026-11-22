import("stdfaust.lib");
base = hslider("base", 800, 50, 5000, 0.01);
depth = hslider("depth", 2, 0, 4, 0.001);
rate = hslider("rate", 0.5, 0.01, 20, 0.001);
q = hslider("q", 2, 0.5, 20, 0.001);
wrap(x) = x - floor(x);
lfo = sin(2*ma.PI*ph) with { ph = (+(rate/ma.SR) : wrap) ~ _; };
fc = base * pow(2.0, depth*lfo);
g = tan(ma.PI*fc/ma.SR);
k = 1.0/q;
a1 = 1.0/(1.0 + g*(g + k));
a2 = g*a1;
a3 = g*a2;
svf(x) = (step ~ (_,_)) : (!,!,_) with {
  step(ic1, ic2) = 2*v1 - ic1, 2*v2 - ic2, v2 with {
    v3 = x - ic2;
    v1 = a1*ic1 + a2*v3;
    v2 = ic2 + a2*ic1 + a3*v3;
  };
};
process = svf;
