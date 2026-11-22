import("stdfaust.lib");
c(i) = sin(0.3*(i+1))/(i+1);
process = _ <: sum(i, 32, c(i) * (_@i));
