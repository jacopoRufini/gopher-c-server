# Linux / Windows C server based on Gopher protocol


[![Build Status](https://travis-ci.org/joemccann/dillinger.svg?branch=master)](https://travis-ci.org/joemccann/dillinger)

Features:
 - The web-server offers same feautures on both Windows and Linux.
 - Server listens by default on port 7070. This can be changed modifying config.txt file.
 - The configuration file is read again if a SIGHUP is sent to the server.
 - If the port changes while the server is up, it starts listening to the given port while serving clients connected to the old port.
 - The web-server supports both multi-thread and multi-process mode.
 - Every time a file is requested, this is locked by the process / thread, even if the operation is read-only.
 - The web-server follows Gopher protocol to read and send responses.
 - Every response sent by the server is registered on a log file, handled by a separate process, which is always listening on events or condition variables.
 - The web-server works also as daemon on Unix/Linux. 

# Installation

Just clone and compile the project

License
----

MIT
