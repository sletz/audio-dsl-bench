import("stdfaust.lib");
freq = hslider("freq", 440, 20, 20000, 0.01);
wrap(x) = x - floor(x);
phase = (+(freq/ma.SR) : wrap) ~ _;
process = sin(2*ma.PI*phase) * 0.5;
