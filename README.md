# lv2-plugins-brain

This is a mildly sophsticated panner in three different versions (4, 5 or 9 inputs).

It spreads the sound sources evenly on a circle and calculates the different time delays and attenuation factors to mimic a position in the room.
Control values are:
+ Radius [m]: the radius of the circle the sound sources are to be spread evenly
+ Player distance [m]: the distance between two sound sources on a direct path

   The player distance is capped with 2 * Radius.
   
+ Ear distance [m]: the distance between the two simulated microphones for left and right input

   A typical average head width is 0.149m; for smaller values, the stereo effect gets smaller; larger values will produce an artifical and kind of irritating sounding soundscape, which might be a nice effect.
  
+ Alpha 0 [degrees]: the angular displacement in the right direction
+ Window Size [s]: This allows to change the responsivness of the plugin by adjusting the duration of the averaging window, that is used for for interpolation.
+ Relative Delays: If this switch is hit, the relative delays between each source and each mic is preserved, but reduced, so that the smallest delay is always 0 samples

   This can be useful for finding good setup values without wanting to deal with the doppler effect caused by a heavy change in radius.
   If switched of, this can help placing different instruments on different distances away from the listener by using two plugins with different radius values

A word about the CPU usage. This plugin interpolates between samples, when the parameters are changed. This causes a doppler effect, but prevents artifacts from skipping samples. After a second without changes, it stops interpolating and the CPU usage is reduced drastically (on my system typically to ~25% of the previous usage).

## License

This software is distributed under the GPL 3.0 License.
