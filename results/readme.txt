- install ffmpeg and add bin\*.exe to the environment variables

- run in the command line, with desired framerate and resolution :

	$ ffmpeg -framerate 60 -i screenshots/frame_%6d.ppm -s 1600x1200 -pix_fmt yuv420p -vf fps=60 sim3D.mp4



- here are some parameters used in the predefined scenarii, as well as the corresponding statistics :


/////////////////////
///drop and splash///
/////////////////////
 
- fluid particles      =    74 000
- boundary particles   =    37 000
- distance field nodes = 4 000 000

- spacing  = 0.25 m
- timestep = 2 x 0.00833 s / frame (60 fps)

- particle simulation =  688 ms / frame
- surface  generation = 1844 ms / frame

- particle simulation omp =  91 ms / frame
- surface  generation omp = 424 ms / frame



//////////////////
///breaking dam///
//////////////////
 
- fluid particles      =   88 000
- boundary particles   =   51 000
- distance field nodes = 4 900 000

- spacing  = 0.25 m
- timestep = 2 x 0.00833 s / frame (60 fps)

- particle simulation =  958 ms / frame
- surface  generation = 2450 ms / frame

- particle simulation omp =  117 ms / frame
- surface  generation omp =  800 ms / frame



////////////////
///fluid flow///
////////////////
 
- fluid particles      =    21 000
- boundary particles   =    38 000
- distance field nodes = 3 100 000

- spacing  = 0.25 m
- timestep = 2 x 0.00833 s / frame (60 fps)

- particle simulation =  192 ms / frame
- surface  generation =  900 ms / frame

- particle simulation omp =   26 ms / frame
- surface  generation omp =  260 ms / frame