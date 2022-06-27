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

/-----zoom on CPU computation only-----\

general statistics :

|    SPH simulation         :  48.45 ms
|    surface reconstruction : 297.507 ms


detailed statistics :

|    search neighbors  : 19.6616 ms
|    predict advection : 11.9035 ms
|    solve pressure    : 14.6869 ms
|    correct position  : 2.19791 ms
|    distance field    : 252.608 ms
|    marching cubes    : 44.8991 ms



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

/-----zoom on CPU computation only-----\

general statistics :

|    SPH simulation         : 53.9425 ms
|    surface reconstruction : 433.297 ms


detailed statistics :

|    search neighbors  : 22.4007 ms
|    predict advection : 13.5631 ms
|    solve pressure    : 15.6671 ms
|    correct position  : 2.31164 ms
|    distance field    : 343.693 ms
|    marching cubes    : 89.6040 ms



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

/-----zoom on CPU computation only-----\

general statistics :

|    SPH simulation         : 11.1033 ms
|    surface reconstruction : 168.351 ms


detailed statistics :

|    search neighbors  : 5.39516 ms
|    predict advection : 2.61276 ms
|    solve pressure    : 2.94987 ms
|    correct position  : 0.14553 ms
|    distance field    : 132.432 ms
|    marching cubes    : 35.9197 ms