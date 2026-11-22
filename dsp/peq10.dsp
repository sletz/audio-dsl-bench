import("stdfaust.lib");
// 10-band parametric EQ (RBJ peaking, octave bands from 31.25 Hz, Q = 1.41), direct form I.
fr(k) = 31.25 * pow(2, k);
gdb(k) = hslider("g%k", ba.take(k+1, (3, -2, 4, -3, 2, -4, 3, -2, 1, -1)), -12, 12, 0.1);
peq(k) = df1 with {
  A = pow(10, gdb(k)/40);
  w0 = 2*ma.PI*fr(k)/ma.SR;
  al = sin(w0)/(2*1.41);
  cw = cos(w0);
  a0 = 1 + al/A;
  b0 = (1 + al*A)/a0;
  b1 = -2*cw/a0;
  b2 = (1 - al*A)/a0;
  a1 = -2*cw/a0;
  a2 = (1 - al/A)/a0;
  df1(x) = (b0*x + b1*x' + b2*x'') : (+ ~ (\(y).(-a1*y - a2*y')));
};
process = seq(k, 10, peq(k));
