import("stdfaust.lib");
// examples/gameaudio/bubble.dsp: the bubble triggered by a slider held at 1 instead of the "drop"
// button (one bubble dropped at the start), then dm.freeverb_demo written out with its default
// wet, damping and room size as sliders and its default stereo spread (0.5) as a constant.
bubble(f0, trig) = os.osc(f)*(exp(-damp*time):si.smooth(0.99))
    with {
        damp = 0.043*f0+0.0014*f0^(3/2);
        f = f0*(1+sigma*time);
        sigma = eta*damp;
        eta = 0.075;
        time = 0:(select2(trig>trig'):+(1))~_:ba.samp2sec;
    };

drop = hslider("drop", 1, 0, 1, 1);
freq = hslider("freq", 600, 150, 2000, 1);

// dm.freeverb_demo
scaleroom = 0.28;
offsetroom = 0.7;
allpassfeed = 0.5;
scaledamp = 0.4;
fixedgain = 0.1;
origSR = 44100;
damping = hslider("damp", 0.5, 0, 1, 0.025)*scaledamp*origSR/ma.SR;
combfeed = hslider("roomSize", 0.5, 0, 1, 0.025)*scaleroom*origSR/ma.SR + offsetroom;
spatSpread = 0.5*46*ma.SR/origSR : int;
g = hslider("wet", 0.3333, 0, 1, 0.025);
freeverb = _,_ <: (*(g)*fixedgain,*(g)*fixedgain :
    re.stereo_freeverb(combfeed, allpassfeed, damping, spatSpread)),
    *(1-g), *(1-g) :> _,_;

process = drop : bubble(freq) <: freeverb;
