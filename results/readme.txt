- install ffmpeg and add bin\*.exe to the environment variables

- run in the command line, with desired framerate and resolution :

	$ ffmpeg -framerate 60 -i screenshots/frame_%6d.ppm -s 1600x1200 -pix_fmt yuv420p -vf fps=60 sim3D.mp4



- here are some parameters used in the predefined scenarii, as well as the corresponding statistics :

/////////////////////
///drop and splash///
/////////////////////
 
- fluid particles      =    75 000
- boundary particles   =    40 000
- distance field nodes = 4 000 000

- spacing  = 0.25 m
- timestep = 2 x 0.00833 s / frame (60 fps)

- particle simulation time    = 210 ms / frame
- surface reconstruction time = 400 ms / frame



//////////////////
///breaking dam///
//////////////////
 
- fluid particles      =   ?
- boundary particles   =   ?
- distance field nodes = ? ?

- spacing  = 0.25 m
- timestep = 2 x 0.00833 s / frame (60 fps)

- particle simulation time    = ? ms / frame
- surface reconstruction time = ? ms / frame



////////////////
///fluid flow///
////////////////
 
- fluid particles      =    22 000
- boundary particles   =    38 000
- distance field nodes = 3 137 000

- spacing  = 0.25 m
- timestep = 2 x 0.00833 s / frame (60 fps)

- particle simulation time    = 80 ms / frame
- surface reconstruction time = 640 ms / frame