In order to run the program:
---------------------------
-Download and start Docker

-Go to branch libl_queue_disk__move 

-Run "docker build -t s3k-docker ." to create the docker image

-Run "docker run -t s3k-docker" to run the program

In order to deactivate the monitor:
-----------------------------------
On row 471 in monitor/main.c file change the row
"*shared_result = result;"
to
"*shared_result = 1;"
To will make the monitor simply pass all request, rendering the monitor useless.