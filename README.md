# web_server
Web Server in C to serve files upon request 

### Usage 

To launch server: make server

then,server --document\_root [FILE DIRECTORY] --port [portno]

## Architecture 
Supports HTTP1.0 and HTTP1.1, from localhost as well as internet access through Chrome, Safari, and Firefox

Supports GET function for any files specified in document\_root

Implemented using multi-threading via pthreads.c package

Uses timeout function for HTTP1.1 that scales inversely with active connections 
