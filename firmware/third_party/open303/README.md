# Open303 TeeBee reference

Source: https://github.com/midilab/jc303/tree/305ad279cf864e7954c6db47612645befb36956c/src/dsp/open303

Copyright (c) 2009 Robin Schmidt. MIT license: see LICENSE.

The four rosic_TeeBeeFilter / rosic_OnePoleFilter .h/.cpp files are unchanged
reference sources, compiled only in the PC comparison test using the host
math shim in tests/open303_compat. The firmware compiles the independent
float adaptation in main/tb303_filter.h, limited to the TB_303 branch.
It retains the coupled four-stage recurrence, TB coefficient polynomials,
resonance mapping and 150 Hz highpass feedback. It uses 4x processing,
linear input interpolation, a fourth-order anti-alias lowpass, block ramps,
an optional external input saturator and bounded state/output guards.
These additions and the 95% resonance limit differ from the original voice.

No dfl_* sources, JUCE, other synthesizer/filter models, oscillator, accent,
sequencer or output network are included. In particular the separate
AGPL-licensed diode ladder and TB303 output network are not used.

The complete MIT notice is distributed in source, offline packages and
firmware release/bundle metadata under thirdPartyLicenses.Open303.
